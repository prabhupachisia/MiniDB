#include "bptree.h"
#include <cstring>
#include <stdexcept>

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

static RID* getRids(std::vector<char>& page) {
    return reinterpret_cast<RID*>(
        page.data() + sizeof(BPTreeHeader) + ORDER * sizeof(int)
    );
}

static bool sameRid(const RID& lhs, const RID& rhs) {
    return lhs.pageNum == rhs.pageNum && lhs.slotIndex == rhs.slotIndex;
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
}

size_t BPlusTree::findLeaf(int key) {
    size_t curr = rootPage;

    while (curr != 0) {
        auto& page = pager->getPage(curr);
        auto header = getHeader(page);

        if (header.nextLeaf == 0 || header.numKeys == 0) {
            return curr;
        }

        int* keys = getKeys(page);
        int largestKey = keys[header.numKeys - 1];

        if (key <= largestKey) {
            return curr;
        }

        curr = header.nextLeaf;
    }

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
    RID* rids = getRids(page);

    int i = 0;
    while (i < header.numKeys && keys[i] < key) i++;

    if (!allowDuplicates) {
        for (int j = 0; j < header.numKeys; j++) {
            if (keys[j] == key) {
                throw std::runtime_error("Duplicate index key");
            }
        }
    }

    for (int j = header.numKeys; j > i; j--) {
        keys[j] = keys[j - 1];
        rids[j] = rids[j - 1];
    }

    keys[i] = key;
    rids[i] = rid;

    header.numKeys++;
    setHeader(page, header);
}

bool BPlusTree::remove(int key, RID rid) {
    size_t curr = rootPage;
    size_t prev = 0;

    while (curr != 0) {
        auto& page = pager->getPage(curr);
        auto header = getHeader(page);

        int* keys = getKeys(page);
        RID* rids = getRids(page);

        for (int i = 0; i < header.numKeys; i++) {
            if (keys[i] == key && sameRid(rids[i], rid)) {
                for (int j = i; j < header.numKeys - 1; j++) {
                    keys[j] = keys[j + 1];
                    rids[j] = rids[j + 1];
                }

                keys[header.numKeys - 1] = 0;
                rids[header.numKeys - 1] = RID();
                header.numKeys--;
                setHeader(page, header);

                if (curr == rootPage && header.numKeys == 0 && header.nextLeaf != 0) {
                    auto& nextPage = pager->getPage(header.nextLeaf);
                    auto nextHeader = getHeader(nextPage);
                    int* nextKeys = getKeys(nextPage);
                    RID* nextRids = getRids(nextPage);

                    for (int j = 0; j < nextHeader.numKeys; j++) {
                        keys[j] = nextKeys[j];
                        rids[j] = nextRids[j];
                    }

                    header.numKeys = nextHeader.numKeys;
                    header.nextLeaf = nextHeader.nextLeaf;
                    setHeader(page, header);
                } else if (header.nextLeaf != 0) {
                    auto& nextPage = pager->getPage(header.nextLeaf);
                    auto nextHeader = getHeader(nextPage);

                    if (header.numKeys + nextHeader.numKeys < ORDER) {
                        int* nextKeys = getKeys(nextPage);
                        RID* nextRids = getRids(nextPage);

                        for (int j = 0; j < nextHeader.numKeys; j++) {
                            keys[header.numKeys + j] = nextKeys[j];
                            rids[header.numKeys + j] = nextRids[j];
                        }

                        header.numKeys += nextHeader.numKeys;
                        header.nextLeaf = nextHeader.nextLeaf;
                        setHeader(page, header);
                    }
                } else if (curr != rootPage && header.numKeys == 0 && prev != 0) {
                    auto& prevPage = pager->getPage(prev);
                    auto prevHeader = getHeader(prevPage);
                    prevHeader.nextLeaf = header.nextLeaf;
                    setHeader(prevPage, prevHeader);
                }

                return true;
            }
        }

        prev = curr;
        curr = header.nextLeaf;
    }

    return false;
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
    RID* rids = getRids(page);
    RID* newRids = getRids(newPageRef);

    for (int i = mid; i < header.numKeys; i++) {
        newKeys[i - mid] = keys[i];
        newRids[i - mid] = rids[i];
    }

    newHeader.numKeys = header.numKeys - mid;
    header.numKeys = mid;

    newHeader.nextLeaf = header.nextLeaf;
    header.nextLeaf = static_cast<uint32_t>(newPage);

    setHeader(page, header);
    setHeader(newPageRef, newHeader);
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

    size_t curr = rootPage;

    while (curr != 0) {
        auto& page = pager->getPage(curr);
        auto header = getHeader(page);

        int* keys = getKeys(page);
        RID* rids = getRids(page);

        for (int i = 0; i < header.numKeys; i++) {
            if (keys[i] == key) {
                result.push_back(rids[i]);
            }
        }

        curr = header.nextLeaf;
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
        RID* rids = getRids(page);

        for (int i = 0; i < header.numKeys; i++) {
            if (keys[i] >= left && keys[i] <= right) {
                result.push_back(rids[i]);
            }
        }

        curr = header.nextLeaf;
    }

    return result;
}
