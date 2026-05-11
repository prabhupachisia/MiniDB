#include "executor.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

Executor::Executor()
    : storage(), indexManager(storage.getPager()) {}

void Executor::execute(const Query& query) {
    try {
        switch (query.type) {
            case QueryType::CREATE:
                handleCreate(static_cast<const CreateQuery&>(query));
                break;
            case QueryType::INSERT:
                handleInsert(static_cast<const InsertQuery&>(query));
                break;
            case QueryType::SELECT: {
                const auto& selectQuery = static_cast<const SelectQuery&>(query);
                auto rows = handleSelect(selectQuery);
                auto columns = getSelectColumnNames(selectQuery);

                for (const auto& col : columns) {
                    std::cout << col << " ";
                }
                std::cout << "\n";

                if (rows.empty()) {
                    std::cout << "No rows found.\n";
                    return;
                }

                for (const auto& row : rows) {
                    for (const auto& val : row) {
                        std::cout << val.toString() << " ";
                    }
                    std::cout << "\n";
                }
                break;
            }
            case QueryType::UPDATE:
                handleUpdate(static_cast<const UpdateQuery&>(query));
                break;
            case QueryType::DELETE:
                handleDelete(static_cast<const DeleteQuery&>(query));
                break;
            case QueryType::USE:
                throw std::runtime_error("USE handled by CLI");
            case QueryType::COMMIT:
                commit();
                std::cout << "Changes committed.\n";
                break;
            case QueryType::DESCRIBE:
                handleDescribe(static_cast<const DescribeQuery&>(query));
                break;
            default:
                throw std::runtime_error("Unsupported query type");
        }
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

void Executor::setDatabase(const std::string& path) {
    db_path = path;

    storage.openDatabase(path);
    schemas = storage.getSchemas();

    indexManager.loadIndexesFromMeta();
    for (const auto& index : storage.getIndexes()) {
        indexManager.loadIndex(
            index.tableName,
            index.columnName,
            toIndexType(index.type),
            index.rootPage
        );
    }
}

void Executor::commit() {
    storage.commit();
}

void Executor::handleCreate(const CreateQuery& q) {
    if (schemas.count(q.table_name)) {
        throw std::runtime_error("Table already exists");
    }

    Schema schema;
    schema.tableName = q.table_name;

    std::unordered_set<std::string> seen;
    int pkCount = 0;

    for (const auto& col : q.columns) {
        if (seen.count(col.name)) {
            throw std::runtime_error("Duplicate column");
        }
        seen.insert(col.name);

        Column column;
        column.name = col.name;
        column.type = parseDataType(col.type);
        column.isPrimaryKey = col.is_primary;

        if (column.isPrimaryKey) pkCount++;

        schema.columns.push_back(column);
    }

    if (pkCount > 1) {
        throw std::runtime_error("Multiple primary keys not allowed");
    }

    schemas[q.table_name] = schema;
    storage.saveSchema(schema);

    for (const auto& col : schema.columns) {
        if (col.isPrimaryKey && col.type == DataType::INT) {
            size_t rootPage = indexManager.createIndex(
                q.table_name,
                col.name,
                IndexManager::PRIMARY
            );

            storage.saveIndex({
                q.table_name,
                col.name,
                static_cast<int>(IndexManager::PRIMARY),
                rootPage
            });
        }
    }

    for (const auto& idx : q.indexes) {
        int colIndex = schema.getColumnIndex(idx.column);
        if (schema.columns[colIndex].type != DataType::INT) {
            throw std::runtime_error("Only INT indexes are supported");
        }
        size_t rootPage = indexManager.createIndex(
            q.table_name,
            idx.column,
            IndexManager::SECONDARY
        );

        storage.saveIndex({
            q.table_name,
            idx.column,
            static_cast<int>(IndexManager::SECONDARY),
            rootPage
        });
    }

    std::cout << "Table created.\n";
}

void Executor::handleInsert(const InsertQuery& q) {
    if (!schemas.count(q.table_name)) {
        throw std::runtime_error("Table not found");
    }

    const Schema& schema = schemas[q.table_name];

    if (q.values.size() != schema.columns.size()) {
        throw std::runtime_error("Column count mismatch");
    }

    Row row;
    for (size_t i = 0; i < q.values.size(); i++) {
        row.push_back(parseValue(q.values[i], schema.columns[i].type));
    }

    int pkIndex = schema.getPrimaryKeyIndex();
    if (pkIndex != -1) {
        auto records = storage.readAllRows(q.table_name);
        for (const auto& [rid, existingRow] : records) {
            if (existingRow[pkIndex] == row[pkIndex]) {
                throw std::runtime_error("Duplicate primary key");
            }
        }
    }

    RID rid = storage.insertRow(q.table_name, row);
    insertIndexesForRow(q.table_name, schema, row, rid);

    std::cout << "Row inserted.\n";
}

std::vector<Row> Executor::handleSelect(const SelectQuery& q) {
    if (!schemas.count(q.table_name)) {
        throw std::runtime_error("Table not found");
    }

    const Schema& schema = schemas[q.table_name];
    std::vector<std::pair<RID, Row>> records;
    bool usedIndex = false;

    if (q.has_where && indexManager.hasIndex(q.table_name, q.where.column)) {
        int colIndex = schema.getColumnIndex(q.where.column);
        if (schema.columns[colIndex].type == DataType::INT && q.where.op == "=") {
            usedIndex = true;
            int key = parseValue(q.where.value, DataType::INT).asInt();
            for (const auto& rid : indexManager.searchAll(q.table_name, q.where.column, key)) {
                Row row = storage.getRow(rid, q.table_name);
                if (!row.empty()) {
                    records.push_back({rid, row});
                }
            }
        }
    }

    if (!usedIndex) {
        records = storage.readAllRows(q.table_name);
    }

    std::vector<Row> result;
    bool selectAll = (q.columns.size() == 1 && q.columns[0] == "*");

    for (const auto& [rid, row] : records) {
        if (q.has_where && !matchCondition(row, schema, q.where)) {
            continue;
        }

        if (selectAll) {
            result.push_back(row);
        } else {
            Row projected;
            for (const auto& col : q.columns) {
                int idx = schema.getColumnIndex(col);
                projected.push_back(row[idx]);
            }
            result.push_back(projected);
        }
    }

    return result;
}

void Executor::handleDelete(const DeleteQuery& q) {
    if (!schemas.count(q.table_name)) {
        throw std::runtime_error("Table not found");
    }

    const Schema& schema = schemas[q.table_name];
    auto records = storage.readAllRows(q.table_name);

    for (const auto& [rid, row] : records) {
        if (!q.has_where || matchCondition(row, schema, q.where)) {
            removeIndexesForRow(q.table_name, schema, row, rid);
            storage.deleteRow(rid);
        }
    }

    std::cout << "Rows deleted.\n";
}

void Executor::handleUpdate(const UpdateQuery& q) {
    if (!schemas.count(q.table_name)) {
        throw std::runtime_error("Table not found");
    }

    const Schema& schema = schemas[q.table_name];
    int updateColIdx = schema.getColumnIndex(q.column);
    Value newValue = parseValue(q.value, schema.columns[updateColIdx].type);

    auto records = storage.readAllRows(q.table_name);
    int pkIndex = schema.getPrimaryKeyIndex();

    for (const auto& [rid, rowOrig] : records) {
        if (q.has_where && !matchCondition(rowOrig, schema, q.where)) {
            continue;
        }

        if (updateColIdx == pkIndex) {
            for (const auto& [otherRid, otherRow] : records) {
                bool sameRid = otherRid.pageNum == rid.pageNum &&
                               otherRid.slotIndex == rid.slotIndex;

                if (!sameRid && otherRow[pkIndex] == newValue) {
                    throw std::runtime_error("Duplicate primary key");
                }
            }
        }

        Row row = rowOrig;
        row[updateColIdx] = newValue;
        removeIndexesForRow(q.table_name, schema, rowOrig, rid);
        RID newRid = storage.updateRow(rid, row, q.table_name);
        insertIndexesForRow(q.table_name, schema, row, newRid);
    }

    std::cout << "Rows updated.\n";
}

void Executor::handleDescribe(const DescribeQuery& q) {
    if (!schemas.count(q.table_name)) {
        throw std::runtime_error("Table not found");
    }

    const Schema& schema = schemas[q.table_name];

    std::cout << "Column Type Key Index\n";
    for (const auto& col : schema.columns) {
        std::string indexType = "-";
        for (const auto& index : storage.getIndexes()) {
            if (index.tableName == q.table_name &&
                index.columnName == col.name) {
                indexType = toIndexType(index.type) == IndexManager::PRIMARY
                    ? "PRIMARY"
                    : "SECONDARY";
                break;
            }
        }

        std::cout << col.name << " "
                  << dataTypeToString(col.type) << " "
                  << (col.isPrimaryKey ? "PRIMARY" : "-") << " "
                  << indexType
                  << "\n";
    }
}

DataType Executor::parseDataType(const std::string& typeStr) {
    std::string t = typeStr;
    std::transform(t.begin(), t.end(), t.begin(), ::toupper);

    if (t == "INT") return DataType::INT;
    if (t == "TEXT") return DataType::TEXT;

    throw std::runtime_error("Unknown type: " + typeStr);
}

IndexManager::IndexType Executor::toIndexType(int type) const {
    if (type == static_cast<int>(IndexManager::PRIMARY)) {
        return IndexManager::PRIMARY;
    }
    if (type == static_cast<int>(IndexManager::SECONDARY)) {
        return IndexManager::SECONDARY;
    }

    throw std::runtime_error("Unknown index type");
}

std::string Executor::dataTypeToString(DataType type) const {
    switch (type) {
        case DataType::INT:
            return "INT";
        case DataType::TEXT:
            return "TEXT";
    }

    return "UNKNOWN";
}

Value Executor::parseValue(const std::string& valStr, DataType expectedType) {
    if (expectedType == DataType::INT) {
        try {
            return Value(std::stoi(valStr));
        } catch (...) {
            throw std::runtime_error("Invalid INT value: " + valStr);
        }
    }

    if (expectedType == DataType::TEXT) {
        return Value(valStr);
    }

    throw std::runtime_error("Invalid value type");
}

std::vector<std::string> Executor::getSelectColumnNames(const SelectQuery& q) const {
    if (!schemas.count(q.table_name)) {
        throw std::runtime_error("Table not found");
    }

    const Schema& schema = schemas.at(q.table_name);

    if (q.columns.size() == 1 && q.columns[0] == "*") {
        std::vector<std::string> columns;
        for (const auto& col : schema.columns) {
            columns.push_back(col.name);
        }
        return columns;
    }

    for (const auto& col : q.columns) {
        schema.getColumnIndex(col);
    }

    return q.columns;
}

void Executor::insertIndexesForRow(
    const std::string& tableName,
    const Schema& schema,
    const Row& row,
    RID rid
) {
    for (size_t i = 0; i < schema.columns.size(); i++) {
        const auto& col = schema.columns[i];
        if (col.type == DataType::INT) {
            indexManager.insert(tableName, col.name, row[i].asInt(), rid);
        }
    }
}

void Executor::removeIndexesForRow(
    const std::string& tableName,
    const Schema& schema,
    const Row& row,
    RID rid
) {
    for (size_t i = 0; i < schema.columns.size(); i++) {
        const auto& col = schema.columns[i];
        if (col.type == DataType::INT) {
            indexManager.remove(tableName, col.name, row[i].asInt(), rid);
        }
    }
}

bool Executor::matchCondition(
    const Row& row,
    const Schema& schema,
    const Condition& cond
) {
    if (cond.op != "=") {
        throw std::runtime_error("Only '=' supported in WHERE");
    }

    int colIndex = schema.getColumnIndex(cond.column);
    DataType type = schema.columns[colIndex].type;
    Value rhs = parseValue(cond.value, type);

    return row[colIndex] == rhs;
}
