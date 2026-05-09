// NanoDB Automated Test Runner
// Reads queries.txt line by line and executes them through the engine.
// Outputs nanodb_execution.log with full decision trace.

#include "src/common/Logger.h"
#include "src/catalog/Schema.h"
#include "src/catalog/SystemCatalog.h"
#include "src/storage/BufferPool.h"
#include "src/storage/DiskManager.h"
#include "src/executor/Executor.h"
#include "src/tpch/DataGenerator.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static const int CUSTOMER_COUNT = 20000;
static const int ORDER_COUNT    = 30000;
static const int LINEITEM_COUNT = 50000;
static const char* QUERIES_FILE = "queries.txt";

int main() {
    // Open log
    Logger::instance().open("nanodb_execution.log");
    LOG_INFO("=== NanoDB Test Runner Starting ===");
    LOG_INFO("Reading workload from: %s", QUERIES_FILE);

    // Schemas
    TableSchema custSchema  = makeCustomerSchema();
    TableSchema ordSchema   = makeOrdersSchema();
    TableSchema lineSchema  = makeLineitemSchema();

    system("mkdir -p db_files");

    // Catalog
    SystemCatalog catalog;
    catalog.registerTable(custSchema);
    catalog.registerTable(ordSchema);
    catalog.registerTable(lineSchema);

    // Generate data if needed
    if (DiskManager::pageCount(custSchema.filePath) == 0) {
        printf("[TestRunner] Generating TPC-H dataset...\n");
        DataGenerator::generateCustomers(custSchema,  CUSTOMER_COUNT, custSchema.filePath);
        DataGenerator::generateOrders   (ordSchema,   ORDER_COUNT,    ORDER_COUNT,  ordSchema.filePath);
        DataGenerator::generateLineitems(lineSchema,  LINEITEM_COUNT, ORDER_COUNT,  lineSchema.filePath);
    }

    BufferPool pool(MAX_POOL_SIZE);
    Executor exec(pool, catalog);

    // Open workload file
    FILE* f = fopen(QUERIES_FILE, "r");
    if (!f) {
        fprintf(stderr, "[TestRunner] ERROR: Cannot open %s\n", QUERIES_FILE);
        return 1;
    }

    char line[1024];
    int queryNum = 0;
    int passed   = 0;
    int failed   = 0;

    printf("\n============================================================\n");
    printf("   NanoDB Automated Test Runner — TPC-H Workload\n");
    printf("============================================================\n\n");

    while (fgets(line, sizeof(line), f)) {
        // Strip newline
        int len = 0;
        while (line[len]) ++len;
        while (len > 0 && (line[len-1]=='\n'||line[len-1]=='\r')) line[--len]='\0';

        // Skip blank lines and comments
        if (len == 0 || line[0] == '#' || line[0] == '-') continue;

        ++queryNum;
        printf("────────────────────────────────────────────────────────────\n");
        printf("[Q%03d] %s\n", queryNum, line);
        printf("────────────────────────────────────────────────────────────\n");
        LOG_INFO("[TestRunner] Executing Q%03d: %s", queryNum, line);

        exec.executeSQL(line);
        ++passed;
        printf("\n");
    }
    fclose(f);

    pool.flushAll();

    printf("============================================================\n");
    printf("Test Runner Complete: %d queries executed, %d errors\n", passed, failed);
    printf("Execution log: nanodb_execution.log\n");
    printf("============================================================\n\n");

    LOG_INFO("=== Test Runner Finished: %d queries executed ===", queryNum);
    return 0;
}
