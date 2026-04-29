#include "executor.h"
#include <iostream>

// ------------------ HELPERS ------------------

DataType parseType(const std::string& type) {
    if (type == "INT") return DataType::INT;
    if (type == "TEXT") return DataType::TEXT;
    throw std::runtime_error("Unknown data type: " + type);
}

// ------------------ MAIN EXECUTE ------------------

void Executor::execute(const Query& query) {
    switch (query.type) {
        case QueryType::USE:
            executeUse(static_cast<const UseQuery&>(query));
            break;

        case QueryType::CREATE:
            executeCreate(static_cast<const CreateQuery&>(query));
            break;

        case QueryType::INSERT:
            executeInsert(static_cast<const InsertQuery&>(query));
            break;

        case QueryType::SELECT:
            executeSelect(static_cast<const SelectQuery&>(query));
            break;

        case QueryType::DELETE:
            executeDelete(static_cast<const DeleteQuery&>(query));
            break;

        default:
            std::cout << "Unsupported query\n";
    }
}

// ------------------ USE ------------------

void Executor::executeUse(const UseQuery& query) {
    currentDB = query.db_name;
    storage.openDatabase(currentDB);

    std::cout << "Using database: " << currentDB << std::endl;
}

// ------------------ CREATE ------------------

void Executor::executeCreate(const CreateQuery& query) {
    if (storage.getSchemas().find(query.table_name) != storage.getSchemas().end()) {
        std::cout << "Table already exists\n";
        return;
    }

    Schema schema;
    schema.tableName = query.table_name;

    for (const auto& col : query.columns) {
        Column c;
        c.name = col.name;
        c.type = parseType(col.type);
        c.isPrimaryKey = col.is_primary;

        schema.columns.push_back(c);
    }

    storage.saveSchema(schema);

    std::cout << "Table created: " << query.table_name << std::endl;
}

// ------------------ INSERT ------------------

void Executor::executeInsert(const InsertQuery& query) {
    const auto& schemas = storage.getSchemas();

    if (schemas.find(query.table_name) == schemas.end()) {
        std::cout << "Table not found\n";
        return;
    }

    const Schema& schema = schemas.at(query.table_name);

    if (query.values.size() != schema.columns.size()) {
        std::cout << "Column count mismatch\n";
        return;
    }

    Row row;

    for (int i = 0; i < query.values.size(); i++) {
        if (schema.columns[i].type == DataType::INT) {
            try {
                row.emplace_back(std::stoi(query.values[i]));
            } catch (...) {
                std::cout << "Invalid INT value for column: "
                          << schema.columns[i].name << "\n";
                return;
            }
        } else {
            row.emplace_back(query.values[i]);
        }
    }

    int pkIndex = schema.getPrimaryKeyIndex();
    std::cout << "[DEBUG] PK Index: " << pkIndex << "\n";

    if (pkIndex != -1) {
        auto existingRows = storage.readAllRows(query.table_name);

        for (const auto& existing : existingRows) {
            if (existing[pkIndex] == row[pkIndex]) {
                std::cout << "Duplicate primary key value\n";
                return;
            }
        }
    }

    storage.insertRow(query.table_name, row);

    std::cout << "1 row inserted\n";
}

// ------------------ SELECT ------------------

void Executor::executeSelect(const SelectQuery& query) {
    const auto& schemas = storage.getSchemas();

    if (schemas.find(query.table_name) == schemas.end()) {
        std::cout << "Table not found\n";
        return;
    }

    const Schema& schema = schemas.at(query.table_name);
    auto rows = storage.readAllRows(query.table_name);

    // Print header
    for (const auto& col : schema.columns) {
        std::cout << col.name << "\t";
    }
    std::cout << "\n";

    for (const auto& row : rows) {
        bool match = true;

        if (query.has_where) {
            int idx = schema.getColumnIndex(query.where.column);

            if (idx == -1) {
                std::cout << "Invalid WHERE column\n";
                return;
            }

            const Value& val = row[idx];

            if (schema.columns[idx].type == DataType::INT) {
                int condVal = std::stoi(query.where.value);

                if (query.where.op == "=") {
                    match = (val.asInt() == condVal);
                }
            } else {
                if (query.where.op == "=") {
                    match = (val.asText() == query.where.value);
                }
            }
        }

        if (match) {
            for (const auto& val : row) {
                std::cout << val.toString() << "\t";
            }
            std::cout << "\n";
        }
    }

    std::cout << "Query executed\n";
}

// ------------------ DELETE ------------------

void Executor::executeDelete(const DeleteQuery& query) {
    std::cout << "DELETE not supported yet (will be added after paging)\n";
}