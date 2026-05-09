#pragma once
#include "Page.h"
#include "../common/Logger.h"
#include <cstdio>
#include <cstring>

// Manages raw binary file I/O for pages.
// Each table file is an array of PAGE_SIZE-byte pages.
class DiskManager {
public:
    DiskManager() {}

    // Write page to disk. Returns true on success.
    static bool writePage(const char* filePath, const Page& page) {
        FILE* f = fopen(filePath, "r+b");
        if (!f) f = fopen(filePath, "w+b");
        if (!f) {
            LOG_ERROR("DiskManager: cannot open %s for write", filePath);
            return false;
        }
        long offset = (long)page.pageId * PAGE_SIZE;
        fseek(f, offset, SEEK_SET);
        size_t written = fwrite(page.data, 1, PAGE_SIZE, f);
        fflush(f);
        fclose(f);
        return written == PAGE_SIZE;
    }

    // Read page from disk into dest. Returns true on success.
    static bool readPage(const char* filePath, int pageId, Page& dest) {
        FILE* f = fopen(filePath, "rb");
        if (!f) return false;
        long offset = (long)pageId * PAGE_SIZE;
        fseek(f, offset, SEEK_SET);
        size_t rd = fread(dest.data, 1, PAGE_SIZE, f);
        fclose(f);
        if (rd != PAGE_SIZE) return false;
        dest.pageId  = pageId;
        dest.isDirty = false;
        dest.isValid = true;
        return true;
    }

    // Return number of pages stored in file (file_size / PAGE_SIZE).
    static int pageCount(const char* filePath) {
        FILE* f = fopen(filePath, "rb");
        if (!f) return 0;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fclose(f);
        return (int)(sz / PAGE_SIZE);
    }

    // Ensure file exists (create if absent).
    static void ensureFile(const char* filePath) {
        FILE* f = fopen(filePath, "ab");
        if (f) fclose(f);
    }
};
