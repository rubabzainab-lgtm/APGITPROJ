#include "JoinOptimizer.h"
#include "../storage/DiskManager.h"

JoinOptimizer::JoinPlan JoinOptimizer::optimize(
        const char* tableName,
        const char* joinTables[], int joinCount,
        const SystemCatalog& catalog) {

    JoinPlan plan;
    plan.tableCount = 0;
    plan.edgeCount  = 0;

    JoinGraph graph;

    // Add primary table
    TableSchema s;
    int rowCount = 0;
    if (catalog.getSchema(tableName, s))
        rowCount = DiskManager::pageCount(s.filePath) * s.recordsPerPage();
    graph.addTable(tableName, rowCount);
    snprintf(plan.orderedTables[plan.tableCount++], 64, "%s", tableName);

    // Add join tables
    for (int i = 0; i < joinCount; ++i) {
        int rc = 0;
        if (catalog.getSchema(joinTables[i], s))
            rc = DiskManager::pageCount(s.filePath) * s.recordsPerPage();
        graph.addTable(joinTables[i], rc);
        snprintf(plan.orderedTables[plan.tableCount++], 64, "%s", joinTables[i]);
    }

    // Add edges between consecutive tables (adjacency = join condition)
    for (int i = 0; i < plan.tableCount; ++i)
        for (int j = i + 1; j < plan.tableCount; ++j)
            graph.addJoin(plan.orderedTables[i], plan.orderedTables[j]);

    // Compute MST
    plan.edgeCount = graph.computeMST(plan.mstEdges);

    // Rebuild order from MST edges
    if (plan.edgeCount > 0) {
        plan.tableCount = 0;
        snprintf(plan.orderedTables[plan.tableCount++], 64, "%s", plan.mstEdges[0].tableU);
        for (int i = 0; i < plan.edgeCount; ++i)
            snprintf(plan.orderedTables[plan.tableCount++], 64, "%s", plan.mstEdges[i].tableV);
    }
    return plan;
}

void JoinOptimizer::printPlan(const JoinPlan& plan) const {
    printf("=== Join Plan (MST-Optimized) ===\n");
    printf("Execution order: ");
    for (int i = 0; i < plan.tableCount; ++i) {
        if (i > 0) printf(" -> ");
        printf("%s", plan.orderedTables[i]);
    }
    printf("\n");
    printf("MST edges:\n");
    for (int i = 0; i < plan.edgeCount; ++i)
        printf("  %s <-> %s  cost=%.0f\n",
               plan.mstEdges[i].tableU, plan.mstEdges[i].tableV, plan.mstEdges[i].weight);
}
