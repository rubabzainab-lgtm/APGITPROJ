#pragma once
#include "../common/Logger.h"
#include <cstring>
#include <cstdio>

// Edge for graph representation of table join costs
struct Edge {
    int   u, v;      // table indices
    float weight;    // join cost estimate
    char  tableU[64];
    char  tableV[64];
};

// Union-Find (Disjoint Set) for Kruskal's MST
struct UnionFind {
    int* parent;
    int* rank;
    int  n;

    explicit UnionFind(int n) : n(n) {
        parent = new int[n];
        rank   = new int[n];
        for (int i = 0; i < n; ++i) { parent[i] = i; rank[i] = 0; }
    }
    ~UnionFind() { delete[] parent; delete[] rank; }

    int find(int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    }
    bool unite(int x, int y) {
        int px = find(x), py = find(y);
        if (px == py) return false;
        if (rank[px] < rank[py]) { int t = px; px = py; py = t; }
        parent[py] = px;
        if (rank[px] == rank[py]) ++rank[px];
        return true;
    }
};

// Sorts edges ascending by weight (selection sort — no STL)
static void sortEdges(Edge* edges, int n) {
    for (int i = 0; i < n - 1; ++i) {
        int minIdx = i;
        for (int j = i + 1; j < n; ++j)
            if (edges[j].weight < edges[minIdx].weight) minIdx = j;
        if (minIdx != i) {
            Edge tmp = edges[i]; edges[i] = edges[minIdx]; edges[minIdx] = tmp;
        }
    }
}

class JoinGraph {
public:
    static const int MAX_NODES = 16;
    static const int MAX_EDGES = MAX_NODES * MAX_NODES;

    JoinGraph() : nodeCount_(0), edgeCount_(0) {}

    void addTable(const char* name, int rowCount) {
        if (nodeCount_ >= MAX_NODES) return;
        snprintf(nodeNames_[nodeCount_], 64, "%s", name);
        rowCounts_[nodeCount_] = rowCount;
        ++nodeCount_;
    }

    int tableIndex(const char* name) const {
        for (int i = 0; i < nodeCount_; ++i)
            if (strcmp(nodeNames_[i], name) == 0) return i;
        return -1;
    }

    // Add join edge; cost heuristic: smaller table drives the join
    void addJoin(const char* tableA, const char* tableB) {
        int u = tableIndex(tableA);
        int v = tableIndex(tableB);
        if (u < 0 || v < 0 || edgeCount_ >= MAX_EDGES) return;
        float cost = (float)(rowCounts_[u] < rowCounts_[v] ? rowCounts_[u] : rowCounts_[v]);
        edges_[edgeCount_] = {u, v, cost, {}, {}};
        snprintf(edges_[edgeCount_].tableU, 64, "%s", tableA);
        snprintf(edges_[edgeCount_].tableV, 64, "%s", tableB);
        ++edgeCount_;
    }

    // Kruskal's MST — returns ordered join path in mstEdges (caller-owned)
    // Returns count of MST edges (nodeCount - 1 for connected graph)
    int computeMST(Edge* mstEdges) {
        sortEdges(edges_, edgeCount_);
        UnionFind uf(nodeCount_);
        int mstCount = 0;

        LOG_INFO("=== JOIN OPTIMIZER: Computing MST via Kruskal's Algorithm ===");
        for (int i = 0; i < edgeCount_ && mstCount < nodeCount_ - 1; ++i) {
            Edge& e = edges_[i];
            if (uf.unite(e.u, e.v)) {
                mstEdges[mstCount++] = e;
                LOG_INFO("[MST] Added edge: %s <-> %s  (cost=%.0f)",
                         e.tableU, e.tableV, e.weight);
            }
        }

        LOG_INFO("[LOG] Multi-table join routed via MST: ", "");
        char path[256] = {};
        for (int i = 0; i < mstCount; ++i) {
            if (i == 0) snprintf(path, sizeof(path), "%s -> %s", mstEdges[i].tableU, mstEdges[i].tableV);
            else {
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s -> %s", path, mstEdges[i].tableV);
                snprintf(path, sizeof(path), "%s", tmp);
            }
        }
        LOG_INFO("[LOG] Multi-table join routed via MST: %s", path);
        return mstCount;
    }

    int nodeCount() const { return nodeCount_; }
    const char* nodeName(int i) const { return nodeNames_[i]; }
    int rowCount(int i) const { return rowCounts_[i]; }

private:
    char nodeNames_[MAX_NODES][64];
    int  rowCounts_[MAX_NODES];
    int  nodeCount_;
    Edge edges_[MAX_EDGES];
    int  edgeCount_;
};
