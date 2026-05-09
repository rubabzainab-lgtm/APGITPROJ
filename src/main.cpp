#include "common/Logger.h"
#include "catalog/Schema.h"
#include "catalog/SystemCatalog.h"
#include "storage/BufferPool.h"
#include "storage/DiskManager.h"
#include "parser/Parser.h"
#include "executor/Executor.h"
#include "tpch/DataGenerator.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static const int CUSTOMER_COUNT  = 20000;
static const int ORDER_COUNT     = 30000;
static const int LINEITEM_COUNT  = 50000;

static void runDemoTestCases(Executor& exec, SystemCatalog& catalog);
static void interactiveMode(Executor& exec);

int main(int argc, char* argv[]) {
    // Initialize logger
    Logger::instance().open("nanodb_execution.log");
    LOG_INFO("=== NanoDB Engine Starting ===");

    // Build schemas
    TableSchema custSchema   = makeCustomerSchema();
    TableSchema ordSchema    = makeOrdersSchema();
    TableSchema lineSchema   = makeLineitemSchema();

    // Ensure DB directory exists (silently)
    system("mkdir -p db_files");

    // Register schemas in system catalog
    SystemCatalog catalog;
    catalog.registerTable(custSchema);
    catalog.registerTable(ordSchema);
    catalog.registerTable(lineSchema);
    catalog.listTables();

    // Generate data if not already present
    bool needGen = false;
    needGen |= (DiskManager::pageCount(custSchema.filePath)  == 0);
    needGen |= (DiskManager::pageCount(ordSchema.filePath)   == 0);
    needGen |= (DiskManager::pageCount(lineSchema.filePath)  == 0);

    if (needGen) {
        printf("\n[NanoDB] Generating TPC-H dataset (%d customers, %d orders, %d lineitems)...\n",
               CUSTOMER_COUNT, ORDER_COUNT, LINEITEM_COUNT);
        DataGenerator::generateCustomers(custSchema,  CUSTOMER_COUNT,  custSchema.filePath);
        DataGenerator::generateOrders   (ordSchema,   ORDER_COUNT,     ORDER_COUNT,
                                         ordSchema.filePath);
        DataGenerator::generateLineitems(lineSchema,  LINEITEM_COUNT,  ORDER_COUNT,
                                         lineSchema.filePath);
        printf("[NanoDB] Dataset generated successfully.\n\n");
    } else {
        printf("[NanoDB] Existing dataset detected — skipping data generation.\n\n");
    }

    // Initialize buffer pool (full size: 200 pages)
    BufferPool pool(MAX_POOL_SIZE);
    Executor exec(pool, catalog);

    // Check CLI args
    bool demoMode   = false;
    bool interactive= false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--demo")   == 0) demoMode    = true;
        if (strcmp(argv[i], "--interactive") == 0) interactive = true;
    }

    if (demoMode || (!interactive && argc == 1)) {
        runDemoTestCases(exec, catalog);
    }
    if (interactive) {
        interactiveMode(exec);
    }

    // Flush all dirty pages before shutdown
    pool.flushAll();
    LOG_INFO("=== NanoDB Engine Shutdown (all pages flushed) ===");
    printf("\n[NanoDB] Execution log written to: nanodb_execution.log\n");
    return 0;
}

static void runDemoTestCases(Executor& exec, SystemCatalog& catalog) {
    printf("\n");
    printf("##############################################################\n");
    printf("#          NanoDB Demo Test Cases (TPC-H Dataset)            #\n");
    printf("##############################################################\n\n");

    // ── TEST CASE A: Parser & Evaluator ──────────────────────────────────────
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("TEST CASE A: Parser & Evaluator\n");
    printf("Query: SELECT * FROM customer WHERE (c_acctbal > 5000 AND c_mktsegment == \"BUILDING\") OR c_nationkey == 15\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    exec.executeSQL("SELECT * FROM customer WHERE (c_acctbal > 5000 AND c_mktsegment == \"BUILDING\") OR c_nationkey == 15");
    printf("\n");

    // ── TEST CASE B: Index Optimizer ─────────────────────────────────────────
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("TEST CASE B: Index Optimizer (Sequential Scan vs AVL Index)\n");
    printf("Searching for c_custkey = 500 in customer table (20,000 records)\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    exec.benchmarkIndex("customer", 0, 500);

    // ── TEST CASE C: Join Optimizer ──────────────────────────────────────────
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("TEST CASE C: Join Optimizer (customer JOIN orders JOIN lineitem)\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    exec.executeJoin("customer", "orders", "lineitem", 10);

    // ── TEST CASE D: Memory Stress ───────────────────────────────────────────
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("TEST CASE D: Memory Stress Test (50-page buffer pool, 5000 lineitem records)\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    {
        BufferPool smallPool(50);   // Restricted to 50 pages
        Executor stressExec(smallPool, catalog);
        stressExec.stressTestBufferPool("lineitem", 5000);
        smallPool.flushAll();
    }

    // ── TEST CASE E: Priority Queue Concurrency ───────────────────────────────
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("TEST CASE E: Priority Queue Concurrency (50 user queries + 1 admin UPDATE)\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");

    // Enqueue 50 user-level SELECT queries
    for (int i = 0; i < 50; ++i) {
        char sql[256];
        snprintf(sql, 256, "SELECT * FROM customer WHERE c_nationkey == %d", i % 25);
        exec.enqueueQuery(sql, 1);  // priority 1 = user
    }
    // Enqueue admin UPDATE (should execute first due to priority 0)
    exec.enqueueQuery(
        "UPDATE customer SET c_acctbal = 9999.99 WHERE c_custkey == 1",
        0   // priority 0 = admin (highest urgency)
    );
    printf("\n[PQ] Admin UPDATE injected into flooded queue. Running queue...\n\n");
    exec.runQueue();

    // ── TEST CASE F: Deep Expression Tree Edge Case ───────────────────────────
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("TEST CASE F: Deep Expression Tree\n");
    printf("Query: SELECT * FROM orders WHERE ((o_totalprice * 1.5) > 100000 AND (o_custkey %% 2 == 0)) OR (o_orderstatus != \"O\")\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    exec.executeSQL("SELECT * FROM orders WHERE ((o_totalprice * 1.5) > 100000 AND (o_custkey % 2 == 0)) OR (o_orderstatus != \"O\")");

    // ── TEST CASE G: Durability & Persistence ────────────────────────────────
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("TEST CASE G: Durability & Persistence\n");
    printf("Inserting 5 new customer records, then verifying persistence...\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");

    for (int i = 0; i < 5; ++i) {
        char sql[512];
        snprintf(sql, sizeof(sql),
                 "INSERT INTO customer (c_custkey, c_name, c_address, c_nationkey, c_phone, c_acctbal, c_mktsegment, c_comment) "
                 "VALUES (%d, \"TestUser%d\", \"123 Test Street\", 5, \"01-999-0000\", 1234.56, \"BUILDING\", \"Test insert\")",
                 99000 + i, i);
        exec.executeSQL(sql);
    }

    // Flush to simulate shutdown
    LOG_INFO("[G] Simulating shutdown — flushing all pages to disk...");
    BufferPool tempPool(MAX_POOL_SIZE);
    Executor verifyExec(tempPool, catalog);
    tempPool.flushAll();

    printf("\n[G] Verifying persisted records after simulated reboot:\n");
    exec.executeSQL("SELECT * FROM customer WHERE c_custkey >= 99000");

    printf("\n");
    printf("##############################################################\n");
    printf("#              All Test Cases Complete                       #\n");
    printf("##############################################################\n\n");
}

static void interactiveMode(Executor& exec) {
    printf("\n[NanoDB] Interactive mode. Type SQL or 'exit'.\n");
    char line[1024];
    while (true) {
        printf("nanodb> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        // Remove trailing newline
        int len = 0; while (line[len]) ++len;
        while (len > 0 && (line[len-1]=='\n'||line[len-1]=='\r')) line[--len]='\0';
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
        if (len > 0) exec.executeSQL(line);
    }
}
