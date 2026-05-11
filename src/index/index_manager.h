#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <optional>
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

    size_t createIndex(const std::string& table,
                       const std::string& column,
                       IndexType type);

    size_t loadIndex(const std::string& table,
                     const std::string& column,
                     IndexType type,
                     size_t rootPage);

    void insert(const std::string& table,
                const std::string& column,
                int key,
                RID rid);
    bool remove(const std::string& table,
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
    std::vector<RID> searchAll(const std::string& table,
                               const std::string& column,
                               int key);
    bool hasIndex(const std::string& table,
                  const std::string& column) const;

    void loadIndexesFromMeta();

    void updateRootPage(const std::string& table,
                        const std::string& column,
                        size_t newRootPage);
};
