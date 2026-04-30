#pragma once

#include <vector>
#include <optional>
#include <functional>
#include "../storage/pager/pager.h"
#include "../storage/rid.h"

const int ORDER = 4;

struct BPTreeHeader {
    uint8_t isLeaf;
    uint16_t numKeys;
    uint32_t nextLeaf;
};

class BPlusTree {
private:
    Pager* pager;
    size_t rootPage;
    bool allowDuplicates;

    size_t findLeaf(int key);

    void insertIntoLeaf(size_t pageNum, int key, RID rid);
    void splitLeaf(size_t pageNum);

    void initLeaf(size_t pageNum);

    
    std::function<void(size_t)> onRootChange;

public:
    // new tree
    BPlusTree(Pager* pager, bool allowDuplicates = false);

    // load existing tree
    BPlusTree(Pager* pager, size_t rootPage, bool allowDuplicates);

    size_t getRootPage() const;

    void insert(int key, RID rid);

    std::optional<RID> search(int key);
    std::vector<RID> searchAll(int key);

    std::vector<RID> rangeSearch(int left, int right);

};