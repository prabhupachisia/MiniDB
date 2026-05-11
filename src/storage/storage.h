#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "../common/schema.h"
#include "../common/value.h"
#include "pager/pager.h"
#include "rid.h"

struct StoredIndex {
    std::string tableName;
    std::string columnName;
    int type;
    size_t rootPage;
};

class Storage {
private:
    std::string dbPath;

    Pager pager;

    std::unordered_map<std::string, Schema> schemas;

    std::unordered_map<std::string, std::vector<size_t>> tablePages;
    std::vector<StoredIndex> indexes;

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
    void commit();

    void saveSchema(const Schema& schema);
    void saveIndex(const StoredIndex& index);

    // data
    RID insertRow(const std::string& tableName, const Row& row);

    Row getRow(const RID& rid, const std::string& tableName);

    void deleteRow(const RID& rid);

    RID updateRow(const RID& rid, const Row& newRow, const std::string& tableName);

    std::vector<std::pair<RID, Row>> readAllRows(const std::string& tableName);

    const std::unordered_map<std::string, Schema>& getSchemas() const;
    const std::vector<StoredIndex>& getIndexes() const;

    Pager* getPager();
};
