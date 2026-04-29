#include "executor.h"
#include <iostream>
#include <unordered_set>
#include <algorithm>

// ------------------ HELPERS ------------------

DataType parseType(const std::string& type) {
    if (type == "INT") return DataType::INT;
    if (type == "TEXT") return DataType::TEXT;
    throw std::runtime_error("Unknown data type: " + type);
}

// ------------------ MAIN EXECUTE ------------------


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
                auto rows = handleSelect(static_cast<const SelectQuery&>(query));

                if (rows.empty()) {
                    std::cout << "[Executor] No rows found\n";
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

            default:
                throw std::runtime_error("Unsupported query type");
        }

    } catch (const std::exception& e) {
        std::cout << "[Executor ERROR] " << e.what() << "\n";
    }
}



// ------------------ USE ------------------

void Executor::setDatabase(const std::string& path) {
    db_path = path;

    storage.openDatabase(path);

    storage.loadMetadata();

    schemas = storage.getSchemas();
}

// ------------------ CREATE ------------------

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

    if (pkCount > 1)
        throw std::runtime_error("Multiple primary keys not allowed");

    schemas[q.table_name] = schema;
    storage.saveSchema(schema);
    

    for (const auto& col : schema.columns) {
        if (col.isPrimaryKey && col.type == DataType::INT) {
            indexManager.createIndex(q.table_name, col.name, IndexManager::PRIMARY);
        }
    }

    std::cout << "[Executor] Table created\n";
}
// ------------------ INSERT ------------------

void Executor::handleInsert(const InsertQuery& q) {
    if (!schemas.count(q.table_name))
        throw std::runtime_error("Table not found");

    const Schema& schema = schemas[q.table_name];

    if (q.values.size() != schema.columns.size())
        throw std::runtime_error("Column count mismatch");

    Row row;

    for (int i = 0; i < q.values.size(); i++) {
        row.push_back(parseValue(q.values[i], schema.columns[i].type));
    }

    int pkIndex = schema.getPrimaryKeyIndex();
    if (pkIndex != -1) {
        const auto& pkCol = schema.columns[pkIndex];

        if (pkCol.type == DataType::INT &&
            indexManager.hasIndex(q.table_name, pkCol.name)) {

            int key = row[pkIndex].asInt();

            if (indexManager.search(q.table_name, pkCol.name, key)) {
                throw std::runtime_error("Duplicate primary key");
            }
        }
    }

    RID rid = storage.insertRow(q.table_name, row);

    for (int i = 0; i < schema.columns.size(); i++) {
        const auto& col = schema.columns[i];

        if (col.type == DataType::INT &&
            indexManager.hasIndex(q.table_name, col.name)) {

            indexManager.insert(
                q.table_name,
                col.name,
                row[i].asInt(),
                rid
            );
        }
    }

    std::cout << "[Executor] Row inserted\n";
}

// ------------------ SELECT ------------------

std::vector<Row> Executor::handleSelect(const SelectQuery& q) {
    if (!schemas.count(q.table_name))
        throw std::runtime_error("Table not found");

    const Schema& schema = schemas[q.table_name];

    // 🔥 INDEX OPTIMIZATION
    if (q.has_where && q.where.op == "=" &&
        indexManager.hasIndex(q.table_name, q.where.column)) {

        int key = std::stoi(q.where.value);

        auto rids = indexManager.searchAll(q.table_name, q.where.column, key);

        if (rids.empty()) return {};

        std::vector<Row> result;

        for (const auto& rid : rids) {
            Row row = storage.getRow(rid, q.table_name);

            if (q.columns.size() == 1 && q.columns[0] == "*") {
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

    auto records = storage.readAllRows(q.table_name);

    std::vector<Row> result;
    bool selectAll = (q.columns.size() == 1 && q.columns[0] == "*");

    for (const auto& [rid, row] : records) {

        if (q.has_where && !matchCondition(row, schema, q.where))
            continue;

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

// ------------------ DELETE ------------------

void Executor::handleDelete(const DeleteQuery& q) {
    if (!schemas.count(q.table_name))
        throw std::runtime_error("Table not found");

    const Schema& schema = schemas[q.table_name];

    // 🔥 INDEX OPTIMIZED DELETE
    if (q.has_where && q.where.op == "=" &&
        indexManager.hasIndex(q.table_name, q.where.column)) {

        int key = std::stoi(q.where.value);

        auto rids = indexManager.searchAll(q.table_name, q.where.column, key);

        for (const auto& rid : rids) {

            Row row = storage.getRow(rid, q.table_name);

            // remove from index
            for (int i = 0; i < schema.columns.size(); i++) {
                const auto& col = schema.columns[i];

                if (col.type == DataType::INT &&
                    indexManager.hasIndex(q.table_name, col.name)) {

                    indexManager.remove(
                        q.table_name,
                        col.name,
                        row[i].asInt(),
                        rid
                    );
                }
            }

            storage.deleteRow(rid);
        }

        std::cout << "[Executor] Rows deleted\n";
        return;
    }

    // 🔁 FALLBACK SCAN
    auto records = storage.readAllRows(q.table_name);

    for (const auto& [rid, row] : records) {

        if (!q.has_where || matchCondition(row, schema, q.where)) {

            for (int i = 0; i < schema.columns.size(); i++) {
                const auto& col = schema.columns[i];

                if (col.type == DataType::INT &&
                    indexManager.hasIndex(q.table_name, col.name)) {

                    indexManager.remove(
                        q.table_name,
                        col.name,
                        row[i].asInt(),
                        rid
                    );
                }
            }

            storage.deleteRow(rid);
        }
    }

    std::cout << "[Executor] Rows deleted\n";
}


DataType Executor::parseDataType(const std::string& typeStr) {
    std::string t = typeStr;

    std::transform(t.begin(), t.end(), t.begin(), ::toupper);

    if (t == "INT") return DataType::INT;
    if (t == "TEXT") return DataType::TEXT;

    throw std::runtime_error("Unknown type: " + typeStr);
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

void Executor::handleUpdate(const UpdateQuery& q) {
    if (!schemas.count(q.table_name))
        throw std::runtime_error("Table not found");

    const Schema& schema = schemas[q.table_name];

    int updateColIdx = schema.getColumnIndex(q.column);
    DataType updateType = schema.columns[updateColIdx].type;

    Value newValue = parseValue(q.value, updateType);

    if (q.has_where && q.where.op == "=") {

        int whereIdx = schema.getColumnIndex(q.where.column);

        // only use index if INT + index exists
        if (schema.columns[whereIdx].type == DataType::INT &&
            indexManager.hasIndex(q.table_name, q.where.column)) {

            int key;
            try {
                key = std::stoi(q.where.value);
            } catch (...) {
                throw std::runtime_error("Invalid WHERE value");
            }

            auto rids = indexManager.searchAll(q.table_name, q.where.column, key);

            for (const auto& rid : rids) {

                Row row = storage.getRow(rid, q.table_name);

                Value oldValue = row[updateColIdx];

                if (schema.columns[updateColIdx].type == DataType::INT &&
                    indexManager.hasIndex(q.table_name, q.column)) {

                    indexManager.update(
                        q.table_name,
                        q.column,
                        oldValue.asInt(),
                        newValue.asInt(),
                        rid
                    );
                }

                row[updateColIdx] = newValue;

                storage.updateRow(rid, row, q.table_name);
            }

            std::cout << "[Executor] Rows updated\n";
            return;
        }
    }
    auto records = storage.readAllRows(q.table_name);

    for (const auto& [rid, rowOrig] : records) {

        if (!q.has_where || matchCondition(rowOrig, schema, q.where)) {

            Row row = rowOrig;

            Value oldValue = row[updateColIdx];

            // update index if needed
            if (schema.columns[updateColIdx].type == DataType::INT &&
                indexManager.hasIndex(q.table_name, q.column)) {

                indexManager.update(
                    q.table_name,
                    q.column,
                    oldValue.asInt(),
                    newValue.asInt(),
                    rid
                );
            }

            row[updateColIdx] = newValue;

            storage.updateRow(rid, row, q.table_name);
        }
    }

    std::cout << "[Executor] Rows updated\n";
}