#include "AVLIndex.h"
#include "../storage/Page.h"
#include "../storage/DiskManager.h"

void AVLIndex::build(const char* filePath, const TableSchema& schema, int colIndex) {
    int pgCount = DiskManager::pageCount(filePath);
    LOG_INFO("[Index] Building AVL index on col %d of '%s' (%d pages)...",
             colIndex, schema.tableName, pgCount);

    for (int pid = 0; pid < pgCount; ++pid) {
        Page pg;
        if (!DiskManager::readPage(filePath, pid, pg)) continue;
        int recCnt = pg.getRecordCount();
        for (int ri = 0; ri < recCnt; ++ri) {
            Row* row = pg.getRow(schema, ri);
            if (!row) continue;
            Field* f = row->getField(colIndex);
            if (f && f->type() == FieldType::INT) {
                int key = static_cast<IntField*>(f)->value();
                IndexEntry existing;
                if (tree_.search(key, existing)) {
                    existing.add(pid, ri);
                    tree_.insert(key, existing);
                } else {
                    IndexEntry entry;
                    entry.add(pid, ri);
                    tree_.insert(key, entry);
                }
            }
            delete row;
        }
    }
    built_ = true;
    LOG_INFO("[Index] AVL index built: %d unique keys indexed", tree_.size());
}

void AVLIndex::insert(int key, int pageId, int recIdx) {
    IndexEntry existing;
    if (tree_.search(key, existing)) {
        existing.add(pageId, recIdx);
        tree_.insert(key, existing);
    } else {
        IndexEntry entry;
        entry.add(pageId, recIdx);
        tree_.insert(key, entry);
    }
}

bool AVLIndex::search(int key, IndexEntry& out) const {
    return tree_.search(key, out);
}

void AVLIndex::remove(int key) {
    tree_.remove(key);
}
