#include "DataGenerator.h"
#include "../storage/Row.h"
#include "../storage/Page.h"
#include "../storage/DiskManager.h"
#include "../common/Logger.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

static const char* MKTSEGMENTS[] = {"BUILDING", "AUTOMOBILE", "MACHINERY", "HOUSEHOLD", "FURNITURE"};
static const char* ORDERSTATUS[] = {"O", "F", "P"};
static const char* ORDERPRIORITY[] = {"1-URGENT", "2-HIGH", "3-MEDIUM", "4-NOT SPECIFIED", "5-LOW"};
static const char* SHIPMODES[] = {"AIR", "SHIP", "TRUCK", "RAIL", "FOB", "MAIL", "REG AIR"};
static const char* SHIPINSTRUCT[] = {"DELIVER IN PERSON", "COLLECT COD", "NONE", "TAKE BACK RETURN"};
static const char* RETURNFLAGS[] = {"N", "A", "R"};
static const char* LINESTATUSES[] = {"O", "F"};

unsigned int DataGenerator::nextRand(unsigned int& seed) {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

float DataGenerator::randFloat(unsigned int& seed, float lo, float hi) {
    float r = (float)(nextRand(seed) % 10000) / 10000.0f;
    return lo + r * (hi - lo);
}

int DataGenerator::randInt(unsigned int& seed, int lo, int hi) {
    return lo + (int)(nextRand(seed) % (unsigned int)(hi - lo + 1));
}

void DataGenerator::randStr(unsigned int& seed, char* out, int len) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz     ";
    for (int i = 0; i < len - 1; ++i)
        out[i] = chars[nextRand(seed) % 31];
    out[len-1] = '\0';
}

void DataGenerator::generateCustomers(const TableSchema& schema,
                                       int numCustomers,
                                       const char* filePath) {
    DiskManager::ensureFile(filePath);
    // Truncate file
    FILE* f = fopen(filePath, "wb"); if (f) fclose(f);

    Page page;
    page.init(0);
    int pageId  = 0;
    unsigned int seed = 42;

    LOG_INFO("[DataGen] Generating %d customer records...", numCustomers);
    for (int i = 1; i <= numCustomers; ++i) {
        Row* row = new Row(schema.colCount);
        char buf[256];

        row->setField(0, new IntField(i));                               // c_custkey
        snprintf(buf, 26, "Customer#%09d", i);
        row->setField(1, new StringField(buf));                          // c_name
        randStr(seed, buf, 40); row->setField(2, new StringField(buf)); // c_address
        row->setField(3, new IntField(randInt(seed, 0, 24)));           // c_nationkey
        snprintf(buf, 16, "%02d-%03d-%04d", randInt(seed,10,99), randInt(seed,100,999), randInt(seed,1000,9999));
        row->setField(4, new StringField(buf));                          // c_phone
        row->setField(5, new FloatField(randFloat(seed, -999.99f, 9999.99f))); // c_acctbal
        row->setField(6, new StringField(MKTSEGMENTS[randInt(seed,0,4)]));     // c_mktsegment
        randStr(seed, buf, 79); row->setField(7, new StringField(buf)); // c_comment

        if (!page.appendRow(schema, row)) {
            DiskManager::writePage(filePath, page);
            ++pageId;
            page.init(pageId);
            page.appendRow(schema, row);
        }
        delete row;
    }
    // Write last partial page
    if (page.getRecordCount() > 0)
        DiskManager::writePage(filePath, page);

    LOG_INFO("[DataGen] Customer data written (%d pages).", pageId + 1);
}

void DataGenerator::generateOrders(const TableSchema& schema,
                                    int numOrders, int maxCustKey,
                                    const char* filePath) {
    DiskManager::ensureFile(filePath);
    FILE* f = fopen(filePath, "wb"); if (f) fclose(f);

    Page page;
    page.init(0);
    int pageId  = 0;
    unsigned int seed = 123;

    LOG_INFO("[DataGen] Generating %d orders records...", numOrders);
    for (int i = 1; i <= numOrders; ++i) {
        Row* row = new Row(schema.colCount);
        char buf[256];

        row->setField(0, new IntField(i));
        row->setField(1, new IntField(randInt(seed, 1, maxCustKey)));
        row->setField(2, new StringField(ORDERSTATUS[randInt(seed,0,2)]));
        row->setField(3, new FloatField(randFloat(seed, 1000.0f, 500000.0f)));
        snprintf(buf, 11, "199%01d-%02d-%02d", randInt(seed,0,8), randInt(seed,1,12), randInt(seed,1,28));
        row->setField(4, new StringField(buf));
        row->setField(5, new StringField(ORDERPRIORITY[randInt(seed,0,4)]));
        snprintf(buf, 16, "Clerk#%09d", randInt(seed,1,1000));
        row->setField(6, new StringField(buf));
        row->setField(7, new IntField(0));
        randStr(seed, buf, 79); row->setField(8, new StringField(buf));

        if (!page.appendRow(schema, row)) {
            DiskManager::writePage(filePath, page);
            ++pageId;
            page.init(pageId);
            page.appendRow(schema, row);
        }
        delete row;
    }
    if (page.getRecordCount() > 0)
        DiskManager::writePage(filePath, page);
    LOG_INFO("[DataGen] Orders data written (%d pages).", pageId + 1);
}

void DataGenerator::generateLineitems(const TableSchema& schema,
                                       int numLineitems, int maxOrderKey,
                                       const char* filePath) {
    DiskManager::ensureFile(filePath);
    FILE* f = fopen(filePath, "wb"); if (f) fclose(f);

    Page page;
    page.init(0);
    int pageId = 0;
    unsigned int seed = 789;

    LOG_INFO("[DataGen] Generating %d lineitem records...", numLineitems);
    for (int i = 0; i < numLineitems; ++i) {
        Row* row = new Row(schema.colCount);
        char buf[256];
        int orderkey = randInt(seed, 1, maxOrderKey);

        row->setField(0,  new IntField(orderkey));
        row->setField(1,  new IntField(randInt(seed, 1, 200000)));
        row->setField(2,  new IntField(randInt(seed, 1, 10000)));
        row->setField(3,  new IntField((i % 7) + 1));
        row->setField(4,  new FloatField(randFloat(seed, 1.0f, 50.0f)));
        row->setField(5,  new FloatField(randFloat(seed, 100.0f, 100000.0f)));
        row->setField(6,  new FloatField(randFloat(seed, 0.0f, 0.10f)));
        row->setField(7,  new FloatField(randFloat(seed, 0.0f, 0.08f)));
        row->setField(8,  new StringField(RETURNFLAGS[randInt(seed,0,2)]));
        row->setField(9,  new StringField(LINESTATUSES[randInt(seed,0,1)]));
        snprintf(buf, 11, "199%01d-%02d-%02d", randInt(seed,0,8), randInt(seed,1,12), randInt(seed,1,28));
        row->setField(10, new StringField(buf));
        snprintf(buf, 11, "199%01d-%02d-%02d", randInt(seed,0,8), randInt(seed,1,12), randInt(seed,1,28));
        row->setField(11, new StringField(buf));
        snprintf(buf, 11, "199%01d-%02d-%02d", randInt(seed,0,8), randInt(seed,1,12), randInt(seed,1,28));
        row->setField(12, new StringField(buf));
        row->setField(13, new StringField(SHIPINSTRUCT[randInt(seed,0,3)]));
        row->setField(14, new StringField(SHIPMODES[randInt(seed,0,6)]));
        randStr(seed, buf, 44); row->setField(15, new StringField(buf));

        if (!page.appendRow(schema, row)) {
            DiskManager::writePage(filePath, page);
            ++pageId;
            page.init(pageId);
            page.appendRow(schema, row);
        }
        delete row;
    }
    if (page.getRecordCount() > 0)
        DiskManager::writePage(filePath, page);
    LOG_INFO("[DataGen] Lineitem data written (%d pages).", pageId + 1);
}
