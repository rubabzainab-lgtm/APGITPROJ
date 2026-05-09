#pragma once
#include "../common/Graph.h"
#include "../catalog/Schema.h"
#include "../catalog/SystemCatalog.h"
#include "../common/Logger.h"
#include <cstring>
#include <cstdio>

// Computes the optimal join order for multi-table queries using MST.
// Tables are graph nodes, join costs are edges (estimated by table size).
class JoinOptimizer {
public:
    static const int MAX_JOIN_TABLES_OPT = 8;

    struct JoinPlan {
        char orderedTables[MAX_JOIN_TABLES_OPT + 1][64];
        int  tableCount;
        Edge mstEdges[JoinGraph::MAX_NODES];
        int  edgeCount;
    };

    // Build join plan for given table list using the system catalog for row counts.
    // tableName = primary table; joinTables = additional tables joined in order.
    JoinPlan optimize(const char* tableName,
                      const char* joinTables[], int joinCount,
                      const SystemCatalog& catalog);

    void printPlan(const JoinPlan& plan) const;
};
