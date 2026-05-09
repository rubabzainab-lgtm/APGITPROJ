#pragma once
#include "Page.h"
#include "DiskManager.h"
#include "../common/DoublyLinkedList.h"
#include "../common/HashMap.h"
#include "../common/Logger.h"
#include <cstring>

// Encodes page identity as a single int: tableHash * 100000 + pageId
// (allows pages from different tables in the same pool)
inline int makePageKey(int tableHash, int pageId) {
    return tableHash * 100000 + pageId;
}

struct FrameInfo {
    int  frameIndex;  // slot in pages_ array
    int  tableHash;
    int  pageId;
    char filePath[256];
};

// Fixed-size buffer pool with O(1) LRU eviction.
// Uses a Doubly Linked List for LRU order and an IntHashMap for O(1) lookup.
class BufferPool {
public:
    explicit BufferPool(int capacity = MAX_POOL_SIZE)
        : capacity_(capacity), frameMap_(capacity * 4), evictionCount_(0) {
        pages_      = new Page[capacity_];
        frameInfos_ = new FrameInfo[capacity_];
        lruNodes_   = new DLLNode<int>*[capacity_];
        freeFrames_ = new int[capacity_];
        freeCount_  = capacity_;

        for (int i = 0; i < capacity_; ++i) {
            freeFrames_[i] = capacity_ - 1 - i;  // stack of free frames
            lruNodes_[i]   = nullptr;
            frameInfos_[i] = {};
            pages_[i]      = Page();
        }
    }

    ~BufferPool() {
        flushAll();
        delete[] pages_;
        delete[] frameInfos_;
        delete[] lruNodes_;
        delete[] freeFrames_;
    }

    // Fetch a page (from cache or disk). Returns pointer to internal Page (valid until next fetch/evict).
    Page* fetchPage(const char* filePath, int tableHash, int pageId) {
        int key = makePageKey(tableHash, pageId);
        int frameIdx = -1;
        if (frameMap_.get(key, frameIdx)) {
            // Cache HIT: move to front of LRU list
            lruList_.moveToFront(lruNodes_[frameIdx]);
            return &pages_[frameIdx];
        }

        // Cache MISS: allocate a frame
        if (freeCount_ > 0) {
            frameIdx = freeFrames_[--freeCount_];
        } else {
            // Evict LRU page
            frameIdx = evictLRU();
        }

        // Load from disk
        Page& pg = pages_[frameIdx];
        pg.init(pageId);
        if (DiskManager::pageCount(filePath) > pageId) {
            DiskManager::readPage(filePath, pageId, pg);
        }

        // Register in map and LRU list
        FrameInfo fi;
        fi.frameIndex = frameIdx;
        fi.tableHash  = tableHash;
        fi.pageId     = pageId;
        snprintf(fi.filePath, 256, "%s", filePath);
        frameInfos_[frameIdx] = fi;

        frameMap_.put(key, frameIdx);
        lruNodes_[frameIdx] = lruList_.pushFront(frameIdx);

        return &pages_[frameIdx];
    }

    // Mark a page as dirty (it will be written to disk on eviction).
    void markDirty(int tableHash, int pageId) {
        int key = makePageKey(tableHash, pageId);
        int frameIdx = -1;
        if (frameMap_.get(key, frameIdx))
            pages_[frameIdx].isDirty = true;
    }

    // Flush all dirty pages to disk.
    void flushAll() {
        for (int i = 0; i < capacity_; ++i) {
            if (pages_[i].isValid && pages_[i].isDirty) {
                DiskManager::writePage(frameInfos_[i].filePath, pages_[i]);
                pages_[i].isDirty = false;
                LOG_INFO("[LOG] Page %d flushed to disk (table hash %d)",
                         pages_[i].pageId, frameInfos_[i].tableHash);
            }
        }
    }

    void flushTable(int tableHash) {
        for (int i = 0; i < capacity_; ++i) {
            if (pages_[i].isValid && pages_[i].isDirty && frameInfos_[i].tableHash == tableHash) {
                DiskManager::writePage(frameInfos_[i].filePath, pages_[i]);
                pages_[i].isDirty = false;
            }
        }
    }

    int getEvictionCount() const { return evictionCount_; }
    int capacity()         const { return capacity_; }

private:
    int evictLRU() {
        // Tail of DLL = least recently used
        int frameIdx = lruList_.popTail();
        FrameInfo& fi = frameInfos_[frameIdx];

        int key = makePageKey(fi.tableHash, fi.pageId);

        if (pages_[frameIdx].isDirty) {
            LOG_INFO("[LOG] Page %d evicted via LRU, written to disk (file: %s)",
                     pages_[frameIdx].pageId, fi.filePath);
            DiskManager::writePage(fi.filePath, pages_[frameIdx]);
        } else {
            LOG_INFO("[LOG] Page %d evicted via LRU (clean, no write needed)",
                     pages_[frameIdx].pageId);
        }

        frameMap_.remove(key);
        lruNodes_[frameIdx]  = nullptr;
        pages_[frameIdx]     = Page();
        ++evictionCount_;
        return frameIdx;
    }

    int                     capacity_;
    Page*                   pages_;
    FrameInfo*              frameInfos_;
    DLLNode<int>**          lruNodes_;
    DoublyLinkedList<int>   lruList_;
    IntHashMap<int>         frameMap_;
    int*                    freeFrames_;
    int                     freeCount_;
    int                     evictionCount_;
};
