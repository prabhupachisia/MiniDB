#pragma once
#include <string>
#include <vector>
#include <utility>

enum class QueryType {
    CREATE,
    INSERT,
    SELECT,
    DELETE,
    USE,
    UPDATE
};

struct Query {
    QueryType type;
    Query(QueryType t) : type(t) {}
    virtual ~Query() = default;
};

struct ColumnDef {
    std::string name;
    std::string type;
    bool is_primary = false;
};

struct Index {
    std::string name;
    std::string column;
};

struct Condition {
    std::string column;
    std::string op;
    std::string value;

    Condition() = default;
    Condition(std::string c, std::string o, std::string v)
        : column(c), op(o), value(v) {}
};

struct CreateQuery : Query {
    std::string table_name;
    std::vector<ColumnDef> columns;
    std::vector<Index> indexes;

    CreateQuery() : Query(QueryType::CREATE) {}
};

struct InsertQuery : Query {
    std::string table_name;
    std::vector<std::string> values;

    InsertQuery() : Query(QueryType::INSERT) {}
};

struct UpdateQuery : Query {
    std::string table_name;
    std::string column;
    std::string value;
    bool has_where = false;
    Condition where;

    UpdateQuery() : Query(QueryType::UPDATE) {}
};

struct SelectQuery : Query {
    std::string table_name;
    std::vector<std::string> columns;

    bool has_where = false;
    Condition where;

    SelectQuery() : Query(QueryType::SELECT) {}
};

struct DeleteQuery : Query {
    std::string table_name;

    bool has_where = false;
    Condition where;

    DeleteQuery() : Query(QueryType::DELETE) {}
};

struct UseQuery : Query {
    std::string db_name;

    UseQuery() : Query(QueryType::USE) {}
};
