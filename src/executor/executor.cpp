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

void Executor::setDatabase(const std::string& path) {
    db_path = path;

    storage.openDatabase(path);
    schemas = storage.getSchemas();
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
            indexManager.createIndex(q.table_name, col.name, IndexManager::PRIMARY);
        }
    }

    for (const auto& idx : q.indexes) {
        int colIndex = schema.getColumnIndex(idx.column);
        if (schema.columns[colIndex].type != DataType::INT) {
            throw std::runtime_error("Only INT indexes are supported");
        }
        indexManager.createIndex(q.table_name, idx.column, IndexManager::SECONDARY);
    }

    std::cout << "[Executor] Table created\n";
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

    for (size_t i = 0; i < schema.columns.size(); i++) {
        const auto& col = schema.columns[i];
        if (col.type == DataType::INT) {
            indexManager.insert(q.table_name, col.name, row[i].asInt(), rid);
        }
    }

    std::cout << "[Executor] Row inserted\n";
}

std::vector<Row> Executor::handleSelect(const SelectQuery& q) {
    if (!schemas.count(q.table_name)) {
        throw std::runtime_error("Table not found");
    }

    const Schema& schema = schemas[q.table_name];
    auto records = storage.readAllRows(q.table_name);

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
            storage.deleteRow(rid);
        }
    }

    std::cout << "[Executor] Rows deleted\n";
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
        storage.updateRow(rid, row, q.table_name);

        if (schema.columns[updateColIdx].type == DataType::INT) {
            indexManager.insert(q.table_name, q.column, newValue.asInt(), rid);
        }
    }

    std::cout << "[Executor] Rows updated\n";
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
