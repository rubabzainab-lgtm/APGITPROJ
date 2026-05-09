#pragma once
#include "../common/AVLTree.h"
#include "../common/Logger.h"
#include "../catalog/Schema.h"
#include <cstring>

// Each value in the index is a chain of page+record positions for that key
struct IndexEntry {
    static const int MAX_LOCS = 32;
    struct Loc { int pageId; int recIdx; };
    Loc locs[MAX_LOCS];
    int count;
    IndexEntry() : count(0) {}
    void add(int pageId, int recIdx) {
        if (count < MAX_LOCS) { locs[count++] = {pageId, recIdx}; }
    }
};

// AVL-tree based index on an integer column of a table.
// Key = column value (int), Value = IndexEntry (page/record locations)
class AVLIndex {
public:
    AVLIndex() : built_(false) {}

    // Build index by scanning all pages of the table from disk.
    void build(const char* filePath, const TableSchema& schema, int colIndex);

    // Insert a single index entry.
    void insert(int key, int pageId, int recIdx);

    // Search for a key. Returns true and fills entry on success.
    bool search(int key, IndexEntry& out) const;

    // Remove a key.
    void remove(int key);

    bool isBuilt() const { return built_; }
    int  size()    const { return tree_.size(); }

    // Range query: returns all entries for keys in [lo, hi]
    template <typename Fn>
    void rangeQuery(int lo, int hi, Fn fn) const {
        tree_.rangeQuery(lo, hi, [&](int k, const IndexEntry& e) { fn(k, e); });
    }

private:
    AVLTree<IndexEntry> tree_;
    bool built_;
};
