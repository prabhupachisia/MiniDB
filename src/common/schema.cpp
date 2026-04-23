#include "schema.h"
#include <stdexcept>

int Schema::getColumnIndex(const std::string& name) const {
    for (int i = 0; i < columns.size(); i++) {
        if (columns[i].name == name)
            return i;
    }
    throw std::runtime_error("Column not found: " + name);
}

int Schema::getPrimaryKeyIndex() const {
    for (int i = 0; i < columns.size(); i++) {
        if (columns[i].isPrimaryKey)
            return i;
    }
    return -1;
}