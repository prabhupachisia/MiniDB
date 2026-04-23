#pragma once

#include <string>
#include <vector>
#include "types.h"

// Column definition
struct Column {
    std::string name;
    DataType type;
    bool isPrimaryKey = false;
};

// Table schema
class Schema {
public:
    std::string tableName;
    std::vector<Column> columns;

    // Get column index by name
    int getColumnIndex(const std::string& name) const;

    // Get primary key column index (-1 if none)
    int getPrimaryKeyIndex() const;
};