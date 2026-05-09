#pragma once
#include "../storage/Field.h"
#include <cstring>
#include <cstdio>

static const int MAX_COLUMNS   = 32;
static const int PAGE_SIZE     = 4096;
static const int PAGE_HEADER   = 16;   // bytes reserved for page header
static const int MAX_POOL_SIZE = 200;  // buffer pool capacity (pages)

struct ColumnDef {
    char      name[64];
    FieldType type;
    int       serSize;  // serialized bytes: INT=4, FLOAT=8, STRING=custom
};

struct TableSchema {
    char      tableName[64];
    char      filePath[256];
    ColumnDef columns[MAX_COLUMNS];
    int       colCount;
    int       pkIndex;   // which column is the primary key (usually 0)

    int recordSize() const {
        int total = 0;
        for (int i = 0; i < colCount; ++i) total += columns[i].serSize;
        return total;
    }

    int recordsPerPage() const {
        return (PAGE_SIZE - PAGE_HEADER) / recordSize();
    }

    int columnIndex(const char* name) const {
        for (int i = 0; i < colCount; ++i)
            if (strcmp(columns[i].name, name) == 0) return i;
        return -1;
    }

    void addColumn(const char* nm, FieldType t, int sz) {
        if (colCount >= MAX_COLUMNS) return;
        snprintf(columns[colCount].name, 64, "%s", nm);
        columns[colCount].type    = t;
        columns[colCount].serSize = sz;
        ++colCount;
    }
};

// Build the three TPC-H schemas
inline TableSchema makeCustomerSchema() {
    TableSchema s;
    snprintf(s.tableName, 64, "customer");
    snprintf(s.filePath, 256, "db_files/customer.db");
    s.colCount = 0;
    s.pkIndex  = 0;
    s.addColumn("c_custkey",    FieldType::INT,    4);
    s.addColumn("c_name",       FieldType::STRING, 25);
    s.addColumn("c_address",    FieldType::STRING, 40);
    s.addColumn("c_nationkey",  FieldType::INT,    4);
    s.addColumn("c_phone",      FieldType::STRING, 15);
    s.addColumn("c_acctbal",    FieldType::FLOAT,  8);
    s.addColumn("c_mktsegment", FieldType::STRING, 10);
    s.addColumn("c_comment",    FieldType::STRING, 79);
    return s;
}

inline TableSchema makeOrdersSchema() {
    TableSchema s;
    snprintf(s.tableName, 64, "orders");
    snprintf(s.filePath, 256, "db_files/orders.db");
    s.colCount = 0;
    s.pkIndex  = 0;
    s.addColumn("o_orderkey",    FieldType::INT,    4);
    s.addColumn("o_custkey",     FieldType::INT,    4);
    s.addColumn("o_orderstatus", FieldType::STRING, 1);
    s.addColumn("o_totalprice",  FieldType::FLOAT,  8);
    s.addColumn("o_orderdate",   FieldType::STRING, 10);
    s.addColumn("o_orderpriority", FieldType::STRING, 15);
    s.addColumn("o_clerk",       FieldType::STRING, 15);
    s.addColumn("o_shippriority",FieldType::INT,    4);
    s.addColumn("o_comment",     FieldType::STRING, 79);
    return s;
}

inline TableSchema makeLineitemSchema() {
    TableSchema s;
    snprintf(s.tableName, 64, "lineitem");
    snprintf(s.filePath, 256, "db_files/lineitem.db");
    s.colCount = 0;
    s.pkIndex  = 0;
    s.addColumn("l_orderkey",     FieldType::INT,    4);
    s.addColumn("l_partkey",      FieldType::INT,    4);
    s.addColumn("l_suppkey",      FieldType::INT,    4);
    s.addColumn("l_linenumber",   FieldType::INT,    4);
    s.addColumn("l_quantity",     FieldType::FLOAT,  8);
    s.addColumn("l_extendedprice",FieldType::FLOAT,  8);
    s.addColumn("l_discount",     FieldType::FLOAT,  8);
    s.addColumn("l_tax",          FieldType::FLOAT,  8);
    s.addColumn("l_returnflag",   FieldType::STRING, 1);
    s.addColumn("l_linestatus",   FieldType::STRING, 1);
    s.addColumn("l_shipdate",     FieldType::STRING, 10);
    s.addColumn("l_commitdate",   FieldType::STRING, 10);
    s.addColumn("l_receiptdate",  FieldType::STRING, 10);
    s.addColumn("l_shipinstruct", FieldType::STRING, 25);
    s.addColumn("l_shipmode",     FieldType::STRING, 10);
    s.addColumn("l_comment",      FieldType::STRING, 44);
    return s;
}
