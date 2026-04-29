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

    static constexpr size_t META_PAGE = 0;

    void initNewDatabase();
    void readHeader();

    size_t getLastDataPage(const std::string& tableName);
    size_t allocateNewDataPage(const std::string& tableName);

public:
    void openDatabase(const std::string& path);

    void saveSchema(const Schema& schema);
    void loadSchemas();

    RID insertRow(const std::string& tableName, const Row& row);
    Row getRow(const RID& rid);
    std::vector<std::pair<RID, Row>> readAllRows(const std::string& tableName);
    void updateRow(const RID& rid, const Row& newRow);
    void deleteRow(const RID& rid);
    std::unordered_map<std::string, std::vector<size_t>> tablePages;

    void debugPager();

    const std::unordered_map<std::string, Schema>& getSchemas() const;
};