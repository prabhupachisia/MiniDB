#include "index_manager.h"
#include "storage/meta_page.h"
#include <stdexcept>

IndexManager::IndexManager(Pager* pager) {
    this->pager = pager;
}

size_t IndexManager::createIndex(const std::string& table,
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

    return rootPage;
}

size_t IndexManager::loadIndex(const std::string& table,
                               const std::string& column,
                               IndexType type,
                               size_t rootPage) {
    bool allowDuplicates = (type == SECONDARY);

    auto tree = std::make_unique<BPlusTree>(pager, rootPage, allowDuplicates);

    indexes[table][column] = {
        type,
        std::move(tree),
        rootPage
    };

    return rootPage;
}

void IndexManager::insert(const std::string& table,
                          const std::string& column,
                          int key,
                          RID rid) {

    if (!indexes.count(table) || !indexes[table].count(column))
        return;

    indexes[table][column].tree->insert(key, rid);
}

bool IndexManager::remove(const std::string& table,
                          const std::string& column,
                          int key,
                          RID rid) {
    if (!indexes.count(table) || !indexes[table].count(column)) {
        return false;
    }

    return indexes[table][column].tree->remove(key, rid);
}

std::optional<RID> IndexManager::search(const std::string& table,
                                         const std::string& column,
                                         int key) {

    if (!indexes.count(table) || !indexes[table].count(column))
        return std::nullopt;

    return indexes[table][column].tree->search(key);
}

std::vector<RID> IndexManager::searchAll(const std::string& table,
                                         const std::string& column,
                                         int key) {
    if (!indexes.count(table) || !indexes[table].count(column)) {
        return {};
    }

    return indexes[table][column].tree->searchAll(key);
}

bool IndexManager::hasIndex(const std::string& table,
                            const std::string& column) const {
    auto tableIt = indexes.find(table);
    if (tableIt == indexes.end()) {
        return false;
    }

    return tableIt->second.count(column) > 0;
}

void IndexManager::persistIndexMeta(const std::string& table,
                                    const std::string& column,
                                    size_t rootPage,
                                    IndexType type) {
    (void)table;
    (void)column;
    (void)rootPage;
    (void)type;
}

void IndexManager::loadIndexesFromMeta() {
    indexes.clear();
}

void IndexManager::updateRootPage(const std::string& table,
                                  const std::string& column,
                                  size_t newRootPage) {

    // ---- update in-memory ----
    if (!indexes.count(table) || !indexes[table].count(column)) {
        throw std::runtime_error("Index not found");
    }

    indexes[table][column].rootPage = newRootPage;

}
