#include "bptree.h"
#include <cstring>

// ===== Helpers =====

static BPTreeHeader getHeader(std::vector<char>& page) {
    BPTreeHeader h;
    memcpy(&h, page.data(), sizeof(h));
    return h;
}

static void setHeader(std::vector<char>& page, const BPTreeHeader& h) {
    memcpy(page.data(), &h, sizeof(h));
}

static int* getKeys(std::vector<char>& page) {
    return reinterpret_cast<int*>(page.data() + sizeof(BPTreeHeader));
}

static char* getDataStart(std::vector<char>& page, int numKeys) {
    return page.data() + sizeof(BPTreeHeader) + numKeys * sizeof(int);
}

// ===== Constructors =====

BPlusTree::BPlusTree(Pager* pager, bool allowDuplicates)
    : pager(pager), allowDuplicates(allowDuplicates) {

    rootPage = pager->allocatePage();
    onRootChange = nullptr;
    initLeaf(rootPage);
}

BPlusTree::BPlusTree(Pager* pager, size_t rootPage, bool allowDuplicates)
    : pager(pager), rootPage(rootPage), allowDuplicates(allowDuplicates) {}

// ===== Getter =====

size_t BPlusTree::getRootPage() const {
    return rootPage;
}

// ===== Init Leaf =====

void BPlusTree::initLeaf(size_t pageNum) {
    auto& page = pager->getPage(pageNum);

    BPTreeHeader h{};
    h.isLeaf = 1;
    h.numKeys = 0;
    h.nextLeaf = 0;

    setHeader(page, h);
    pager->flush(pageNum);
}

// ===== Find Leaf (only root for now) =====

size_t BPlusTree::findLeaf(int key) {
    return rootPage;
}

// ===== Insert =====

void BPlusTree::insert(int key, RID rid) {
    auto leaf = findLeaf(key);
    insertIntoLeaf(leaf, key, rid);

    auto& page = pager->getPage(leaf);
    auto header = getHeader(page);

    if (header.numKeys >= ORDER) {
        splitLeaf(leaf);
    }
}

// ===== Insert Into Leaf =====

void BPlusTree::insertIntoLeaf(size_t pageNum, int key, RID rid) {
    auto& page = pager->getPage(pageNum);
    auto header = getHeader(page);

    int* keys = getKeys(page);

    int i = 0;
    while (i < header.numKeys && keys[i] < key) i++;

    // shift keys
    for (int j = header.numKeys; j > i; j--) {
        keys[j] = keys[j - 1];
    }

    keys[i] = key;

    // store RID
    char* data = getDataStart(page, header.numKeys + 1);

    memcpy(data + i * sizeof(RID), &rid, sizeof(RID));

    header.numKeys++;
    setHeader(page, header);

    pager->flush(pageNum);
}

// ===== Split Leaf =====

void BPlusTree::splitLeaf(size_t pageNum) {
    auto& page = pager->getPage(pageNum);
    auto header = getHeader(page);

    int* keys = getKeys(page);

    size_t newPage = pager->allocatePage();
    initLeaf(newPage);

    auto& newPageRef = pager->getPage(newPage);
    auto newHeader = getHeader(newPageRef);

    int mid = header.numKeys / 2;

    int* newKeys = getKeys(newPageRef);

    for (int i = mid; i < header.numKeys; i++) {
        newKeys[i - mid] = keys[i];
    }

    newHeader.numKeys = header.numKeys - mid;
    header.numKeys = mid;

    newHeader.nextLeaf = header.nextLeaf;
    header.nextLeaf = newPage;

    setHeader(page, header);
    setHeader(newPageRef, newHeader);

    pager->flush(pageNum);
    pager->flush(newPage);
}

// ===== Search =====

std::optional<RID> BPlusTree::search(int key) {
    auto res = searchAll(key);
    if (!res.empty()) return res[0];
    return std::nullopt;
}

// ===== SearchAll =====

std::vector<RID> BPlusTree::searchAll(int key) {
    std::vector<RID> result;

    auto& page = pager->getPage(rootPage);
    auto header = getHeader(page);

    int* keys = getKeys(page);
    char* data = getDataStart(page, header.numKeys);

    for (int i = 0; i < header.numKeys; i++) {
        if (keys[i] == key) {
            RID rid;
            memcpy(&rid, data + i * sizeof(RID), sizeof(RID));
            result.push_back(rid);
        }
    }

    return result;
}

// ===== Range =====

std::vector<RID> BPlusTree::rangeSearch(int left, int right) {
    std::vector<RID> result;

    size_t curr = rootPage;

    while (curr != 0) {
        auto& page = pager->getPage(curr);
        auto header = getHeader(page);

        int* keys = getKeys(page);
        char* data = getDataStart(page, header.numKeys);

        for (int i = 0; i < header.numKeys; i++) {
            if (keys[i] >= left && keys[i] <= right) {
                RID rid;
                memcpy(&rid, data + i * sizeof(RID), sizeof(RID));
                result.push_back(rid);
            }
        }

        curr = header.nextLeaf;
    }

    return result;
}