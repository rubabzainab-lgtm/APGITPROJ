#pragma once
#include "../parser/Parser.h"
#include "../storage/BufferPool.h"
#include "../catalog/SystemCatalog.h"
#include "../index/AVLIndex.h"
#include "../optimizer/JoinOptimizer.h"
#include "../common/PriorityQueue.h"
#include "../common/Logger.h"
#include <cstdio>

// A pending query in the priority queue
struct QueryTask {
    char sql[1024];
    int  priority;  // 0 = admin (highest), 1 = user
};

// Value union for postfix expression evaluation
struct EvalVal {
    enum Kind { INT_K, FLOAT_K, STRING_K, BOOL_K } kind;
    int    ival;
    double fval;
    char   sval[256];
    bool   bval;

    EvalVal() : kind(INT_K), ival(0), fval(0.0), bval(false) { sval[0]='\0'; }
    static EvalVal fromInt(int v)         { EvalVal e; e.kind=INT_K;    e.ival=v; return e; }
    static EvalVal fromFloat(double v)    { EvalVal e; e.kind=FLOAT_K;  e.fval=v; return e; }
    static EvalVal fromStr(const char* s) { EvalVal e; e.kind=STRING_K; snprintf(e.sval,256,"%s",s); return e; }
    static EvalVal fromBool(bool b)       { EvalVal e; e.kind=BOOL_K;   e.bval=b; return e; }

    double asNum() const { return (kind==INT_K)?(double)ival:fval; }
};

class Executor {
public:
    explicit Executor(BufferPool& pool, SystemCatalog& catalog);
    ~Executor();

    // Submit a query to the priority queue (priority 0=admin, 1=user)
    void enqueueQuery(const char* sql, int priority = 1);

    // Execute all pending queries in the queue
    void runQueue();

    // Execute a single SQL command immediately
    void executeSQL(const char* sql);

    // Execute and measure: sequential scan vs AVL index.
    void benchmarkIndex(const char* tableName, int pkColIdx, int searchKey);

    // Execute a 3-table join using the optimizer.
    void executeJoin(const char* t1, const char* t2, const char* t3, int printLimit);

    // Stress-test: scan N records with limited buffer pool capacity.
    // Returns eviction count.
    int stressTestBufferPool(const char* tableName, int maxRecords);

private:
    // Core execute methods
    int  executeSelect(const ParsedQuery& q, bool printRows = true, int limit = 50);
    bool executeInsert(const ParsedQuery& q);
    bool executeUpdate(const ParsedQuery& q);
    bool executeDelete(const ParsedQuery& q);

    // Evaluate postfix WHERE expression against a row. Returns true if row passes.
    bool evalWhere(const ParsedQuery& q, const Row& row, const TableSchema& schema) const;

    // Sequential scan: return matching row count (and optionally print rows)
    int seqScan(const TableSchema& schema, const ParsedQuery& q,
                bool print, int limit, int maxRecs = -1);

    // AVL index scan
    int indexScan(const TableSchema& schema, const AVLIndex& idx,
                  const ParsedQuery& q, int searchKey, bool print);

    // Resolve column value from a row into EvalVal
    EvalVal fieldToVal(const Field* f) const;

    // Apply operator to two EvalVals
    EvalVal applyOp(TokenType op, const EvalVal& left, const EvalVal& right) const;

    // Get/build AVL index for a table+column (lazy build)
    AVLIndex* getOrBuildIndex(const char* tableName, int colIdx);

    BufferPool&   pool_;
    SystemCatalog& catalog_;

    // Index cache: one AVLIndex per table (on primary key by default)
    static const int MAX_TABLES = 8;
    char       indexedTable_[MAX_TABLES][64];
    int        indexedCol_  [MAX_TABLES];
    AVLIndex*  indices_     [MAX_TABLES];
    int        indexCount_;

    PriorityQueue<QueryTask> queryQueue_;
};
