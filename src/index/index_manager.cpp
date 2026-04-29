    #include "index_manager.h"
    #include <stdexcept>

    // create index
    void IndexManager::createIndex(const std::string& table,
                                const std::string& column,
                                IndexType type) {

        if (indexes[table].count(column)) {
            throw std::runtime_error("Index already exists");
        }

        bool allowDuplicates = (type == SECONDARY);

        indexes[table][column] = {
            type,
            std::make_unique<BPlusTree>(allowDuplicates)
        };
    }

    // insert
    void IndexManager::insert(const std::string& table,
                            const std::string& column,
                            int key,
                            RID rid) {

        if (!hasIndex(table, column)) return;

        indexes[table][column].tree->insert(key, rid);
    }

    // delete
    void IndexManager::remove(const std::string& table,
                            const std::string& column,
                            int key,
                            RID rid) {

        if (!hasIndex(table, column)) return;

        indexes[table][column].tree->remove(key, rid);
    }

    // update
    void IndexManager::update(const std::string& table,
                            const std::string& column,
                            int oldKey,
                            int newKey,
                            RID rid) {

        if (!hasIndex(table, column)) return;

        indexes[table][column].tree->update(oldKey, newKey, rid);
    }

    // primary search
    std::optional<RID> IndexManager::search(const std::string& table,
                                            const std::string& column,
                                            int key) {

        if (!hasIndex(table, column)) return std::nullopt;

        return indexes[table][column].tree->search(key);
    }

    // secondary search
    std::vector<RID> IndexManager::searchAll(const std::string& table,
                                            const std::string& column,
                                            int key) {

        if (!hasIndex(table, column)) return {};

        return indexes[table][column].tree->searchAll(key);
    }

    // range
    std::vector<RID> IndexManager::rangeSearch(const std::string& table,
                                                const std::string& column,
                                                int left,
                                                int right) {

        if (!hasIndex(table, column)) return {};

        return indexes[table][column].tree->rangeSearch(left, right);
    }

    // helper
    bool IndexManager::hasIndex(const std::string& table,
                                const std::string& column) {

        return indexes.count(table) &&
            indexes[table].count(column);
    }