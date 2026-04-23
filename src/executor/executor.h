#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "common/schema.h"
#include "common/value.h"
#include "parser/ast.h"

class Executor {
private:
    std::unordered_map<std::string, Schema> schemas;
    std::unordered_map<std::string, std::vector<Row>> tables;

    std::string db_path;

public:
    void setDatabase(const std::string& path);
    void execute(const Query& query);

private:
    void handleCreate(const CreateQuery& q);
    void handleInsert(const InsertQuery& q);
    std::vector<Row> handleSelect(const SelectQuery& q);
    void handleDelete(const DeleteQuery& q);

    DataType parseDataType(const std::string& typeStr);
    Value parseValue(const std::string& valStr, DataType expectedType);

    bool matchCondition(const Row& row, const Schema& schema, const Condition& cond);
};