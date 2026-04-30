#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include "../index/bptree.h"
#include "../storage/pager/pager.h"

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
        size_t rootPage;  // 🔥 persistence hook
    };

    std::unordered_map<
        std::string,
        std::unordered_map<std::string, Index>
    > indexes;

    Pager* pager;

public:
    IndexManager(Pager* pager);

    void createIndex(const std::string& table,
                     const std::string& column,
                     IndexType type);

    void insert(const std::string& table,
                const std::string& column,
                int key,
                RID rid);

    void persistIndexMeta(const std::string& table,
                                    const std::string& column,
                                    size_t rootPage,
                                    IndexType type);

    std::optional<RID> search(const std::string& table,
                              const std::string& column,
                              int key);

    void loadIndexesFromMeta();

    void IndexManager::updateRootPage(const std::string& table,
                                  const std::string& column,
                                  size_t newRootPage);
};