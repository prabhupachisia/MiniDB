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

    storage.loadSchemas();
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
            throw std::runtime_error("Duplicate column: " + col.name);
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

    std::cout << "[Executor] Table created: " << q.table_name << "\n";
}
// ------------------ INSERT ------------------

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
        auto allRows = storage.readAllRows(q.table_name);

        for (const auto& [rid, existingRow] : allRows) {
            if (existingRow[pkIndex] == row[pkIndex]) {
                throw std::runtime_error("Duplicate primary key");
            }
        }
    }

    storage.insertRow(q.table_name, row);

    std::cout << "[Executor] Row inserted into " << q.table_name << "\n";
}

// ------------------ SELECT ------------------

std::vector<Row> Executor::handleSelect(const SelectQuery& q) {
    if (!schemas.count(q.table_name)) {
        throw std::runtime_error("Table not found");
    }

    const Schema& schema = schemas[q.table_name];

    auto records = storage.readAllRows(q.table_name);

    std::vector<Row> result;

    bool selectAll = (q.columns.size() == 1 && q.columns[0] == "*");

    std::vector<int> colIndexes;

    if (!selectAll) {
        for (const auto& colName : q.columns) {
            colIndexes.push_back(schema.getColumnIndex(colName));
        }
    }

    for (const auto& [rid, row] : records) {

        if (q.has_where && !matchCondition(row, schema, q.where)) {
            continue;
        }

        if (selectAll) {
            result.push_back(row);
        } else {
            Row projected;

            for (int idx : colIndexes) {
                projected.push_back(row[idx]);
            }

            result.push_back(projected);
        }
    }

    return result;
}

// ------------------ DELETE ------------------

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

    std::cout << "[Executor] Rows deleted from " << q.table_name << "\n";
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
        if (valStr.size() >= 2 &&
            valStr.front() == '\'' &&
            valStr.back() == '\'') {

            return Value(valStr.substr(1, valStr.size() - 2));
        }

        throw std::runtime_error("TEXT must be in quotes: " + valStr);
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

