#pragma once

#include <vector>
#include <optional>
#include <stdexcept>
#include "../storage/rid.h"

const int ORDER = 4;

struct Node {
    bool isLeaf;

    std::vector<int> keys;

    // internal nodes
    std::vector<Node*> children;

    // leaf nodes
    std::vector<std::vector<RID>> values;

    Node* next;

    Node(bool leaf);
};

struct SplitResult {
    int key;
    Node* rightChild;
};

class BPlusTree {
private:
    Node* root;
    bool allowDuplicates;

    Node* findLeaf(int key);
    std::optional<SplitResult> insertRecursive(Node* node, int key, RID rid);

public:
    BPlusTree(bool allowDuplicates = false);

    // insert
    void insert(int key, RID rid);

    // primary search
    std::optional<RID> search(int key);

    // secondary search
    std::vector<RID> searchAll(int key);

    // delete
    bool remove(int key, RID rid);

    // update
    bool update(int oldKey, int newKey, RID rid);

    // range query
    std::vector<RID> rangeSearch(int left, int right);
};