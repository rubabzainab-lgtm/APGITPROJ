#pragma once
#include "../catalog/Schema.h"
#include "Row.h"
#include <cstring>

// Page header layout (PAGE_HEADER = 16 bytes):
//   [0..3]  page_id     (int32)
//   [4..7]  rec_count   (int32)
//   [8..11] next_page   (int32, -1 = none)
//   [12]    reserved
//   [13..15] padding

class Page {
public:
    int  pageId;
    bool isDirty;
    bool isValid;
    char data[PAGE_SIZE];

    Page() : pageId(-1), isDirty(false), isValid(false) {
        memset(data, 0, PAGE_SIZE);
    }

    void init(int pid) {
        pageId  = pid;
        isDirty = false;
        isValid = true;
        memset(data, 0, PAGE_SIZE);
        writeInt(0, pid);
        writeInt(4, 0);
        writeInt(8, -1);
    }

    int  getPageId()      const { return readInt(0); }
    int  getRecordCount() const { return readInt(4); }
    int  getNextPageId()  const { return readInt(8); }
    void setNextPageId(int id)  { writeInt(8, id); }

    bool appendRow(const TableSchema& schema, Row* row) {
        int rpp = schema.recordsPerPage();
        int cnt = getRecordCount();
        if (cnt >= rpp) return false;
        int offset = PAGE_HEADER + cnt * schema.recordSize();
        if (offset + schema.recordSize() > PAGE_SIZE) return false;
        row->serialize(schema, data + offset);
        writeInt(4, cnt + 1);
        isDirty = true;
        return true;
    }

    Row* getRow(const TableSchema& schema, int index) const {
        int cnt = getRecordCount();
        if (index < 0 || index >= cnt) return nullptr;
        int offset = PAGE_HEADER + index * schema.recordSize();
        return Row::deserialize(schema, data + offset);
    }

    bool isFull(const TableSchema& schema) const {
        return getRecordCount() >= schema.recordsPerPage();
    }

private:
    int readInt(int off) const {
        return (unsigned char)data[off]
             | ((unsigned char)data[off+1] << 8)
             | ((unsigned char)data[off+2] << 16)
             | ((unsigned char)data[off+3] << 24);
    }
    void writeInt(int off, int val) {
        data[off+0] = (char)( val        & 0xFF);
        data[off+1] = (char)((val >>  8) & 0xFF);
        data[off+2] = (char)((val >> 16) & 0xFF);
        data[off+3] = (char)((val >> 24) & 0xFF);
    }
};
