#include "index_manager.h"
#include "storage/meta_page.h"
#include <stdexcept>

IndexManager::IndexManager(Pager* pager) {
    this->pager = pager;
}

void IndexManager::createIndex(const std::string& table,
                                const std::string& column,
                                IndexType type) {

    if (indexes[table].count(column)) {
        throw std::runtime_error("Index already exists");
    }

    bool allowDuplicates = (type == SECONDARY);

    auto tree = std::make_unique<BPlusTree>(pager, allowDuplicates);
    size_t rootPage = tree->getRootPage();

    persistIndexMeta(table, column, rootPage, type);

    indexes[table][column] = {
        type,
        std::move(tree),
        rootPage
    };
}

void IndexManager::insert(const std::string& table,
                          const std::string& column,
                          int key,
                          RID rid) {

    if (!indexes.count(table) || !indexes[table].count(column))
        return;

    indexes[table][column].tree->insert(key, rid);
}

std::optional<RID> IndexManager::search(const std::string& table,
                                         const std::string& column,
                                         int key) {

    if (!indexes.count(table) || !indexes[table].count(column))
        return std::nullopt;

    return indexes[table][column].tree->search(key);
}

void IndexManager::persistIndexMeta(const std::string& table,
                                    const std::string& column,
                                    size_t rootPage,
                                    IndexType type) {

    auto& page = pager->getPage(0);
    auto meta = getMeta(page);

    IndexMeta entry{};

    strncpy(entry.tableName, table.c_str(), 31);
    entry.tableName[31] = '\0';

    strncpy(entry.columnName, column.c_str(), 31);
    entry.columnName[31] = '\0';

    entry.rootPage = rootPage;
    entry.type = static_cast<uint8_t>(type);

    meta.indexes[meta.numIndexes++] = entry;

    setMeta(page, meta);
    pager->flush(0);
}

void IndexManager::loadIndexesFromMeta() {
    auto& page = pager->getPage(0);
    auto meta = getMeta(page);

    for (uint32_t i = 0; i < meta.numIndexes; i++) {
        auto& entry = meta.indexes[i];

        std::string table(entry.tableName);
        std::string column(entry.columnName);

        bool allowDuplicates = (entry.type == SECONDARY);

        auto tree = std::make_unique<BPlusTree>(
            pager,
            entry.rootPage,
            allowDuplicates
        );

        indexes[table][column] = {
            (IndexType)entry.type,
            std::move(tree),
            entry.rootPage
        };
    }
}

void IndexManager::updateRootPage(const std::string& table,
                                  const std::string& column,
                                  size_t newRootPage) {

    // ---- update in-memory ----
    if (!indexes.count(table) || !indexes[table].count(column)) {
        throw std::runtime_error("Index not found");
    }

    indexes[table][column].rootPage = newRootPage;

    // ---- update meta page ----
    auto& page = pager->getPage(0);
    MetaPage meta = getMeta(page);

    for (uint32_t i = 0; i < meta.numIndexes; i++) {
        IndexMeta& entry = meta.indexes[i];

        if (table == entry.tableName &&
            column == entry.columnName) {

            entry.rootPage = newRootPage;

            setMeta(page, meta);
            pager->flush(0);
            return;
        }
    }

    throw std::runtime_error("Index meta entry not found");
}