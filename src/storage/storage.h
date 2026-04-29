#pragma once

#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>

#include "../common/schema.h"
#include "../common/value.h"

class Storage {
private:
    std::fstream dbFile;
    std::string dbPath;

    std::unordered_map<std::string, Schema> schemas;

    std::streampos schemaOffset = 64;

    void initNewDatabase();
    void readHeader();

public:
    void openDatabase(const std::string& path);

    void saveSchema(const Schema& schema);
    void loadSchemas();

    void insertRow(const std::string& tableName, const Row& row);
    std::vector<Row> readAllRows(const std::string& tableName);

    const std::unordered_map<std::string, Schema>& getSchemas() const;
};