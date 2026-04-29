#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include "../index/bptree.h"

class IndexManager {
public:
    enum IndexType {
        PRIMARY,
        SECONDARY
    };

private:
    struct Index {
        IndexType type;
        std::unique_ptr<BPlusTree> tree;
    };

    // table → column → index
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, Index>
    > indexes;

public:
    // create index
    void createIndex(const std::string& table,
                     const std::string& column,
                     IndexType type);

    // insert
    void insert(const std::string& table,
                const std::string& column,
                int key,
                RID rid);

    // delete
    void remove(const std::string& table,
                const std::string& column,
                int key,
                RID rid);

    // update
    void update(const std::string& table,
                const std::string& column,
                int oldKey,
                int newKey,
                RID rid);

    // search primary
    std::optional<RID> search(const std::string& table,
                              const std::string& column,
                              int key);

    // search secondary
    std::vector<RID> searchAll(const std::string& table,
                              const std::string& column,
                              int key);

    // range
    std::vector<RID> rangeSearch(const std::string& table,
                                const std::string& column,
                                int left,
                                int right);

    // helper
    bool hasIndex(const std::string& table,
                  const std::string& column);
};