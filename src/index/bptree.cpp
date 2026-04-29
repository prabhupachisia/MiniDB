#include "bptree.h"

Node::Node(bool leaf) {
    isLeaf = leaf;
    next = nullptr;
}

BPlusTree::BPlusTree(bool allowDuplicates) {
    root = new Node(true);
    this->allowDuplicates = allowDuplicates;
}

Node* BPlusTree::findLeaf(int key) {
    Node* curr = root;

    while (!curr->isLeaf) {
        int i = 0;
        while (i < curr->keys.size() && key >= curr->keys[i]) i++;
        curr = curr->children[i];
    }

    return curr;
}

std::optional<RID> BPlusTree::search(int key) {
    auto res = searchAll(key);
    if (!res.empty()) return res[0];
    return std::nullopt;
}

std::vector<RID> BPlusTree::searchAll(int key) {
    Node* leaf = findLeaf(key);

    for (int i = 0; i < leaf->keys.size(); i++) {
        if (leaf->keys[i] == key)
            return leaf->values[i];
    }

    return {};
}

void BPlusTree::insert(int key, RID rid) {
    if (!allowDuplicates && search(key)) {
        throw std::runtime_error("Duplicate key");
    }

    auto result = insertRecursive(root, key, rid);

    if (result) {
        Node* newRoot = new Node(false);
        newRoot->keys.push_back(result->key);
        newRoot->children.push_back(root);
        newRoot->children.push_back(result->rightChild);
        root = newRoot;
    }
}

std::optional<SplitResult> BPlusTree::insertRecursive(Node* node, int key, RID rid) {
    if (node->isLeaf) {
        int i = 0;
        while (i < node->keys.size() && node->keys[i] < key) i++;

        // existing key (secondary index)
        if (i < node->keys.size() && node->keys[i] == key) {
            node->values[i].push_back(rid);
            return std::nullopt;
        }

        node->keys.insert(node->keys.begin() + i, key);
        node->values.insert(node->values.begin() + i, std::vector<RID>{rid});

        if (node->keys.size() < ORDER)
            return std::nullopt;

        // split leaf
        Node* newLeaf = new Node(true);
        int mid = ORDER / 2;

        newLeaf->keys.assign(node->keys.begin() + mid, node->keys.end());
        newLeaf->values.assign(node->values.begin() + mid, node->values.end());

        node->keys.resize(mid);
        node->values.resize(mid);

        newLeaf->next = node->next;
        node->next = newLeaf;

        return SplitResult{newLeaf->keys[0], newLeaf};
    }

    // internal node
    int i = 0;
    while (i < node->keys.size() && key >= node->keys[i]) i++;

    auto childSplit = insertRecursive(node->children[i], key, rid);

    if (!childSplit)
        return std::nullopt;

    node->keys.insert(node->keys.begin() + i, childSplit->key);
    node->children.insert(node->children.begin() + i + 1, childSplit->rightChild);

    if (node->keys.size() < ORDER)
        return std::nullopt;

    // split internal
    Node* newInternal = new Node(false);
    int mid = ORDER / 2;

    int promoteKey = node->keys[mid];

    newInternal->keys.assign(node->keys.begin() + mid + 1, node->keys.end());
    newInternal->children.assign(node->children.begin() + mid + 1, node->children.end());

    node->keys.resize(mid);
    node->children.resize(mid + 1);

    return SplitResult{promoteKey, newInternal};
}

bool BPlusTree::remove(int key, RID rid) {
    Node* leaf = findLeaf(key);

    for (int i = 0; i < leaf->keys.size(); i++) {
        if (leaf->keys[i] == key) {

            auto& vec = leaf->values[i];

            for (int j = 0; j < vec.size(); j++) {
                if (vec[j].pageNum == rid.pageNum &&
                    vec[j].slotIndex == rid.slotIndex) {

                    vec.erase(vec.begin() + j);

                    // remove key if empty
                    if (vec.empty()) {
                        leaf->keys.erase(leaf->keys.begin() + i);
                        leaf->values.erase(leaf->values.begin() + i);
                    }

                    return true;
                }
            }
        }
    }

    return false;
}

bool BPlusTree::update(int oldKey, int newKey, RID rid) {
    auto existing = searchAll(oldKey);
    if (existing.empty())
        return false;

    if (!allowDuplicates && oldKey != newKey && search(newKey)) {
        throw std::runtime_error("Duplicate key");
    }

    if (oldKey == newKey)
        return true;

    remove(oldKey, rid);
    insert(newKey, rid);

    return true;
}

std::vector<RID> BPlusTree::rangeSearch(int left, int right) {
    std::vector<RID> result;

    Node* curr = findLeaf(left);

    while (curr) {
        for (int i = 0; i < curr->keys.size(); i++) {
            if (curr->keys[i] >= left && curr->keys[i] <= right) {
                for (auto& rid : curr->values[i])
                    result.push_back(rid);
            }

            if (curr->keys[i] > right)
                return result;
        }
        curr = curr->next;
    }

    return result;
}