#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "../common/schema.h"
#include "../common/value.h"
#include "pager/pager.h"
#include "rid.h"

class Storage {
private:
    std::string dbPath;

    Pager pager;

    std::unordered_map<std::string, Schema> schemas;

    std::unordered_map<std::string, std::vector<size_t>> tablePages;

    static constexpr size_t META_PAGE = 0;

    void initNewDatabase();
    void validateTable(const std::string &tableName);

    size_t getLastDataPage(const std::string& tableName);
    size_t allocateNewDataPage(const std::string& tableName);

public:
    void openDatabase(const std::string& path);

    // metadata
    void saveMetadata();
    void loadMetadata();

    void saveSchema(const Schema& schema);

    // data
    RID insertRow(const std::string& tableName, const Row& row);

    Row getRow(const RID& rid, const std::string& tableName);

    void deleteRow(const RID& rid);

    void updateRow(const RID& rid, const Row& newRow, const std::string& tableName);

    std::vector<std::pair<RID, Row>> readAllRows(const std::string& tableName);

    const std::unordered_map<std::string, Schema>& getSchemas() const;
};