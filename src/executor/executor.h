#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "../common/schema.h"
#include "../common/value.h"
#include "../parser/ast.h"
#include "../storage/storage.h"
#include "../storage/pager/pager.h"
#include "../index/index_manager.h"

class Executor {
private:
    std::unordered_map<std::string, Schema> schemas;

    Storage storage;
    IndexManager indexManager;

    std::string db_path;

public:
    Executor();

    void setDatabase(const std::string& path);
    void execute(const Query& query);
    void commit();

private:
    void handleCreate(const CreateQuery& q);
    void handleInsert(const InsertQuery& q);
    std::vector<Row> handleSelect(const SelectQuery& q);
    void handleUpdate(const UpdateQuery& q);
    void handleDelete(const DeleteQuery& q);
    void handleDescribe(const DescribeQuery& q);

    DataType parseDataType(const std::string& typeStr);
    IndexManager::IndexType toIndexType(int type) const;
    std::string dataTypeToString(DataType type) const;
    Value parseValue(const std::string& valStr, DataType expectedType);
    std::vector<std::string> getSelectColumnNames(const SelectQuery& q) const;
    void insertIndexesForRow(const std::string& tableName, const Schema& schema, const Row& row, RID rid);
    void removeIndexesForRow(const std::string& tableName, const Schema& schema, const Row& row, RID rid);

    bool matchCondition(const Row& row, const Schema& schema, const Condition& cond);
};
