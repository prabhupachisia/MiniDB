#include "executor/executor.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>

void Executor::setDatabase(const std::string& path) {
    db_path = path;

    schemas.clear();
    tables.clear();
}

void Executor::execute(const Query& query) {
    switch (query.type) {

        case QueryType::CREATE:
            handleCreate(static_cast<const CreateQuery&>(query));
            break;

        case QueryType::INSERT:
            handleInsert(static_cast<const InsertQuery&>(query));
            break;

        case QueryType::SELECT: {
            auto rows = handleSelect(static_cast<const SelectQuery&>(query));

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

        default:
            throw std::runtime_error("Unsupported query type in executor");
    }
}


void Executor::handleCreate(const CreateQuery& q) {
    if (schemas.count(q.table_name)) {
        throw std::runtime_error("Table already exists");
    }

    Schema schema;
    schema.tableName = q.table_name;

    for (const auto& col : q.columns) {
        Column column;
        column.name = col.name;
        column.type = parseDataType(col.type);
        column.isPrimaryKey = col.is_primary;

        schema.columns.push_back(column);
    }

    schemas[q.table_name] = schema;
    tables[q.table_name] = {};

    std::cout << "[Executor] Table created: " << q.table_name << "\n";
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
        for (const auto& existingRow : tables[q.table_name]) {
            if (existingRow[pkIndex] == row[pkIndex]) {
                throw std::runtime_error("Duplicate primary key");
            }
        }
    }

    tables[q.table_name].push_back(row);

    std::cout << "[Executor] Row inserted into " << q.table_name << "\n";
}

std::vector<Row> Executor::handleSelect(const SelectQuery& q) {
    if (!schemas.count(q.table_name)) {
        throw std::runtime_error("Table not found");
    }

    const Schema& schema = schemas[q.table_name];
    const auto& table = tables[q.table_name];

    std::vector<Row> result;

    for (const auto& row : table) {
        if (q.has_where) {
            if (!matchCondition(row, schema, q.where)) continue;
        }
        result.push_back(row);
    }

    return result;
}

void Executor::handleDelete(const DeleteQuery& q) {
    if (!schemas.count(q.table_name)) {
        throw std::runtime_error("Table not found");
    }

    const Schema& schema = schemas[q.table_name];
    auto& table = tables[q.table_name];

    table.erase(
        std::remove_if(table.begin(), table.end(),
            [&](const Row& row) {
                if (!q.has_where) return true;
                return matchCondition(row, schema, q.where);
            }),
        table.end()
    );

    std::cout << "[Executor] Rows deleted from " << q.table_name << "\n";
}

DataType Executor::parseDataType(const std::string& typeStr) {
    if (typeStr == "INT") return DataType::INT;
    if (typeStr == "TEXT") return DataType::TEXT;

    throw std::runtime_error("Unknown type: " + typeStr);
}

Value Executor::parseValue(const std::string& valStr, DataType expectedType) {
    if (expectedType == DataType::INT) {
        return Value(std::stoi(valStr));
    }

    if (expectedType == DataType::TEXT) {
        if (valStr.front() == '\'' && valStr.back() == '\'') {
            return Value(valStr.substr(1, valStr.size() - 2));
        }
        return Value(valStr);
    }

    throw std::runtime_error("Invalid value");
}

bool Executor::matchCondition(
    const Row& row,
    const Schema& schema,
    const Condition& cond
) {
    int colIndex = schema.getColumnIndex(cond.column);

    DataType type = schema.columns[colIndex].type;
    Value rhs = parseValue(cond.value, type);

    return row[colIndex] == rhs;
}

