#pragma once
#include "Schema.h"
#include "../common/HashMap.h"
#include "../common/Logger.h"
#include <cstdio>

// System catalog: a hash map of table name -> TableSchema.
// Provides O(1) schema lookups for any registered table.
class SystemCatalog {
public:
    SystemCatalog() {}

    void registerTable(const TableSchema& schema) {
        map_.put(schema.tableName, schema);
        LOG_INFO("[Catalog] Registered table '%s' (record_size=%d, rpp=%d)",
                 schema.tableName, schema.recordSize(), schema.recordsPerPage());
    }

    bool getSchema(const char* tableName, TableSchema& out) const {
        return map_.get(tableName, out);
    }

    bool hasTable(const char* tableName) const {
        return map_.contains(tableName);
    }

    void listTables() const {
        printf("=== System Catalog ===\n");
        map_.forEach([](const char* name, const TableSchema& s) {
            printf("  Table: %-12s  columns=%d  rec_size=%d  rpp=%d\n",
                   name, s.colCount, s.recordSize(), s.recordsPerPage());
        });
    }

    // Simple djb2 hash for a table name (used as tableHash in buffer pool keys)
    static int tableHash(const char* name) {
        unsigned long h = 5381;
        while (*name) { h = ((h << 5) + h) + (unsigned char)(*name); ++name; }
        return (int)(h % 99991);
    }

private:
    HashMap<TableSchema> map_;
};
