#include "Executor.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>

Executor::Executor(BufferPool& pool, SystemCatalog& catalog)
    : pool_(pool), catalog_(catalog), indexCount_(0), queryQueue_(512) {
    for (int i = 0; i < MAX_TABLES; ++i) {
        indices_[i]      = nullptr;
        indexedCol_[i]   = -1;
        indexedTable_[i][0] = '\0';
    }
}

Executor::~Executor() {
    for (int i = 0; i < indexCount_; ++i) delete indices_[i];
}

void Executor::enqueueQuery(const char* sql, int priority) {
    QueryTask task;
    snprintf(task.sql, 1024, "%s", sql);
    task.priority = priority;
    queryQueue_.enqueue(task, priority);
    LOG_INFO("[PQ] Enqueued query (priority=%d): %.80s", priority, sql);
}

void Executor::runQueue() {
    LOG_INFO("[PQ] Processing query queue (%d pending)...", queryQueue_.size());
    while (!queryQueue_.isEmpty()) {
        QueryTask task = queryQueue_.dequeue();
        const char* label = (task.priority == 0) ? "ADMIN" : "USER";
        LOG_INFO("[PQ] Executing %s query: %.80s", label, task.sql);
        executeSQL(task.sql);
    }
}

void Executor::executeSQL(const char* sql) {
    Parser parser;
    ParsedQuery q;
    if (!parser.parse(sql, q)) {
        LOG_ERROR("Parse failed for: %s", sql);
        return;
    }

    switch (q.type) {
        case QueryType::SELECT:
            if (q.joinCount > 0) {
                const char* jt[MAX_JOIN_TABLES];
                for (int i = 0; i < q.joinCount; ++i) jt[i] = q.joinTables[i];
                executeJoin(q.tableName, (q.joinCount > 0 ? jt[0] : nullptr),
                            (q.joinCount > 1 ? jt[1] : nullptr), 20);
            } else {
                executeSelect(q);
            }
            break;
        case QueryType::INSERT: executeInsert(q); break;
        case QueryType::UPDATE: executeUpdate(q); break;
        case QueryType::DELETE: executeDelete(q); break;
        default: LOG_WARN("Unknown query type"); break;
    }
}

int Executor::executeSelect(const ParsedQuery& q, bool printRows, int limit) {
    TableSchema schema;
    if (!catalog_.getSchema(q.tableName, schema)) {
        LOG_ERROR("Table not found: %s", q.tableName);
        return 0;
    }

    LOG_INFO("[Exec] SELECT from '%s' WHERE=%s", q.tableName, q.hasWhere ? q.whereRaw : "(none)");
    int count = seqScan(schema, q, printRows, limit);
    LOG_INFO("[Exec] SELECT complete: %d rows matched", count);
    return count;
}

bool Executor::executeInsert(const ParsedQuery& q) {
    TableSchema schema;
    if (!catalog_.getSchema(q.tableName, schema)) {
        LOG_ERROR("Table not found: %s", q.tableName);
        return false;
    }

    Row* row = new Row(schema.colCount);
    for (int i = 0; i < schema.colCount; ++i) {
        int colIdx = i;
        // Try to match by column name if cols specified
        if (q.insertColCount > 0 && i < q.insertColCount) {
            colIdx = schema.columnIndex(q.insertCols[i]);
            if (colIdx < 0) colIdx = i;
        }
        const char* val = (i < q.insertColCount || i < schema.colCount)
                          ? q.insertVals[i] : "";
        const ColumnDef& col = schema.columns[i];
        if (col.type == FieldType::INT)        row->setField(i, new IntField(atoi(val)));
        else if (col.type == FieldType::FLOAT)  row->setField(i, new FloatField(atof(val)));
        else                                    row->setField(i, new StringField(val));
    }

    int tableHash = SystemCatalog::tableHash(schema.tableName);
    int pgCount   = DiskManager::pageCount(schema.filePath);
    bool inserted = false;

    // Try last page first
    for (int pid = pgCount - 1; pid >= 0 && !inserted; --pid) {
        Page* pg = pool_.fetchPage(schema.filePath, tableHash, pid);
        if (!pg->isFull(schema)) {
            pg->appendRow(schema, row);
            pool_.markDirty(tableHash, pid);
            inserted = true;
            LOG_INFO("[Insert] Row inserted into page %d of '%s'", pid, schema.tableName);
        }
    }

    if (!inserted) {
        // Allocate new page
        int newPid = pgCount;
        Page* pg   = pool_.fetchPage(schema.filePath, tableHash, newPid);
        pg->init(newPid);
        pg->appendRow(schema, row);
        pool_.markDirty(tableHash, newPid);
        LOG_INFO("[Insert] New page %d allocated for '%s'", newPid, schema.tableName);
    }

    delete row;
    return true;
}

bool Executor::executeUpdate(const ParsedQuery& q) {
    TableSchema schema;
    if (!catalog_.getSchema(q.tableName, schema)) {
        LOG_ERROR("Table not found: %s", q.tableName);
        return false;
    }

    int colIdx = schema.columnIndex(q.updateCol);
    if (colIdx < 0) { LOG_ERROR("Column not found: %s", q.updateCol); return false; }

    int tableHash = SystemCatalog::tableHash(schema.tableName);
    int pgCount   = DiskManager::pageCount(schema.filePath);
    int updated   = 0;

    for (int pid = 0; pid < pgCount; ++pid) {
        Page* pg  = pool_.fetchPage(schema.filePath, tableHash, pid);
        int   cnt = pg->getRecordCount();
        for (int ri = 0; ri < cnt; ++ri) {
            Row* row = pg->getRow(schema, ri);
            if (!row) continue;
            bool match = !q.hasWhere || evalWhere(q, *row, schema);
            if (match) {
                const ColumnDef& col = schema.columns[colIdx];
                Field* newField = nullptr;
                if (col.type == FieldType::INT)        newField = new IntField(atoi(q.updateVal));
                else if (col.type == FieldType::FLOAT)  newField = new FloatField(atof(q.updateVal));
                else                                    newField = new StringField(q.updateVal);
                row->setField(colIdx, newField);
                // Write row back into page
                char* dst = pg->data + PAGE_HEADER + ri * schema.recordSize();
                row->serialize(schema, dst);
                pool_.markDirty(tableHash, pid);
                ++updated;
            }
            delete row;
        }
    }
    LOG_INFO("[Update] %d rows updated in '%s'", updated, schema.tableName);
    return true;
}

bool Executor::executeDelete(const ParsedQuery& q) {
    TableSchema schema;
    if (!catalog_.getSchema(q.tableName, schema)) {
        LOG_ERROR("Table not found: %s", q.tableName);
        return false;
    }

    int tableHash = SystemCatalog::tableHash(schema.tableName);
    int pgCount   = DiskManager::pageCount(schema.filePath);
    int deleted   = 0;
    int recSize   = schema.recordSize();

    for (int pid = 0; pid < pgCount; ++pid) {
        Page* pg  = pool_.fetchPage(schema.filePath, tableHash, pid);
        int   cnt = pg->getRecordCount();
        int   kept = 0;
        char* tmpBuf = new char[PAGE_SIZE];
        // memcpy retained records
        for (int ri = 0; ri < cnt; ++ri) {
            Row* row = pg->getRow(schema, ri);
            if (!row) continue;
            bool del = q.hasWhere && evalWhere(q, *row, schema);
            if (!del) {
                row->serialize(schema, tmpBuf + PAGE_HEADER + kept * recSize);
                ++kept;
            } else {
                ++deleted;
            }
            delete row;
        }
        if (deleted > 0) {
            // Write kept records back
            char* dataDst = pg->data + PAGE_HEADER;
            char* dataSrc = tmpBuf  + PAGE_HEADER;
            for (int b = 0; b < kept * recSize; ++b) dataDst[b] = dataSrc[b];
            // Update record count
            pg->data[4] = (char)(kept & 0xFF);
            pg->data[5] = (char)((kept >> 8) & 0xFF);
            pg->data[6] = (char)((kept >> 16) & 0xFF);
            pg->data[7] = (char)((kept >> 24) & 0xFF);
            pool_.markDirty(tableHash, pid);
        }
        delete[] tmpBuf;
    }
    LOG_INFO("[Delete] %d rows deleted from '%s'", deleted, schema.tableName);
    return true;
}

int Executor::seqScan(const TableSchema& schema, const ParsedQuery& q,
                      bool print, int limit, int maxRecs) {
    int tableHash = SystemCatalog::tableHash(schema.tableName);
    int pgCount   = DiskManager::pageCount(schema.filePath);
    int matched   = 0;
    int scanned   = 0;

    for (int pid = 0; pid < pgCount; ++pid) {
        Page* pg  = pool_.fetchPage(schema.filePath, tableHash, pid);
        int   cnt = pg->getRecordCount();
        for (int ri = 0; ri < cnt; ++ri) {
            if (maxRecs > 0 && scanned >= maxRecs) goto done;
            Row* row = pg->getRow(schema, ri);
            if (!row) { ++scanned; continue; }
            bool match = !q.hasWhere || evalWhere(q, *row, schema);
            if (match) {
                ++matched;
                if (print && matched <= limit) row->print(schema);
            }
            delete row;
            ++scanned;
        }
    }
done:
    return matched;
}

int Executor::indexScan(const TableSchema& schema, const AVLIndex& idx,
                        const ParsedQuery& q, int searchKey, bool print) {
    IndexEntry entry;
    if (!idx.search(searchKey, entry)) {
        LOG_INFO("[IndexScan] Key %d not found in AVL index", searchKey);
        return 0;
    }
    int tableHash = SystemCatalog::tableHash(schema.tableName);
    int matched   = 0;
    for (int i = 0; i < entry.count; ++i) {
        Page* pg  = pool_.fetchPage(schema.filePath, tableHash, entry.locs[i].pageId);
        Row*  row = pg->getRow(schema, entry.locs[i].recIdx);
        if (!row) continue;
        if (!q.hasWhere || evalWhere(q, *row, schema)) {
            ++matched;
            if (print) row->print(schema);
        }
        delete row;
    }
    return matched;
}

bool Executor::evalWhere(const ParsedQuery& q, const Row& row, const TableSchema& schema) const {
    if (!q.hasWhere || q.postfixLen == 0) return true;

    Stack<EvalVal> stk(128);

    for (int i = 0; i < q.postfixLen; ++i) {
        const Token& tok = q.postfix[i];

        if (tok.type == TokenType::INTEGER) {
            stk.push(EvalVal::fromInt(tok.intVal));
        } else if (tok.type == TokenType::FLOAT_LIT) {
            stk.push(EvalVal::fromFloat(tok.floatVal));
        } else if (tok.type == TokenType::STRING_LIT) {
            stk.push(EvalVal::fromStr(tok.lexeme));
        } else if (tok.type == TokenType::IDENTIFIER) {
            int colIdx = schema.columnIndex(tok.lexeme);
            if (colIdx >= 0) {
                const Field* f = row.getField(colIdx);
                stk.push(f ? fieldToVal(f) : EvalVal::fromInt(0));
            } else {
                // Could be a constant identifier (e.g., enum string)
                stk.push(EvalVal::fromStr(tok.lexeme));
            }
        } else if (tok.isOperator()) {
            if (stk.size() < 2) return false;
            EvalVal right = stk.pop();
            EvalVal left  = stk.pop();
            stk.push(applyOp(tok.type, left, right));
        }
    }

    if (stk.isEmpty()) return true;
    EvalVal result = stk.pop();
    if (result.kind == EvalVal::BOOL_K) return result.bval;
    return result.ival != 0 || result.fval != 0.0;
}

EvalVal Executor::fieldToVal(const Field* f) const {
    if (!f) return EvalVal::fromInt(0);
    switch (f->type()) {
        case FieldType::INT:    return EvalVal::fromInt(static_cast<const IntField*>(f)->value());
        case FieldType::FLOAT:  return EvalVal::fromFloat(static_cast<const FloatField*>(f)->value());
        case FieldType::STRING: return EvalVal::fromStr(static_cast<const StringField*>(f)->value());
    }
    return EvalVal::fromInt(0);
}

EvalVal Executor::applyOp(TokenType op, const EvalVal& L, const EvalVal& R) const {
    // Logical AND / OR
    if (op == TokenType::AND) {
        bool lb = (L.kind==EvalVal::BOOL_K) ? L.bval : (L.ival!=0||L.fval!=0.0);
        bool rb = (R.kind==EvalVal::BOOL_K) ? R.bval : (R.ival!=0||R.fval!=0.0);
        return EvalVal::fromBool(lb && rb);
    }
    if (op == TokenType::OR) {
        bool lb = (L.kind==EvalVal::BOOL_K) ? L.bval : (L.ival!=0||L.fval!=0.0);
        bool rb = (R.kind==EvalVal::BOOL_K) ? R.bval : (R.ival!=0||R.fval!=0.0);
        return EvalVal::fromBool(lb || rb);
    }

    // String comparison
    if (L.kind == EvalVal::STRING_K || R.kind == EvalVal::STRING_K) {
        const char* ls = (L.kind==EvalVal::STRING_K) ? L.sval : "";
        const char* rs = (R.kind==EvalVal::STRING_K) ? R.sval : "";
        int cmp = 0;
        int i = 0;
        while (ls[i] && rs[i]) { if (ls[i]<rs[i]){cmp=-1;break;} if(ls[i]>rs[i]){cmp=1;break;} ++i; }
        if (cmp==0) { if (ls[i]) cmp=1; else if (rs[i]) cmp=-1; }
        switch (op) {
            case TokenType::EQ:  return EvalVal::fromBool(cmp == 0);
            case TokenType::NEQ: return EvalVal::fromBool(cmp != 0);
            case TokenType::LT:  return EvalVal::fromBool(cmp <  0);
            case TokenType::GT:  return EvalVal::fromBool(cmp >  0);
            case TokenType::LTE: return EvalVal::fromBool(cmp <= 0);
            case TokenType::GTE: return EvalVal::fromBool(cmp >= 0);
            default: return EvalVal::fromBool(false);
        }
    }

    // Numeric comparison / arithmetic
    double lv = L.asNum(), rv = R.asNum();
    switch (op) {
        case TokenType::EQ:    return EvalVal::fromBool(lv == rv);
        case TokenType::NEQ:   return EvalVal::fromBool(lv != rv);
        case TokenType::LT:    return EvalVal::fromBool(lv <  rv);
        case TokenType::GT:    return EvalVal::fromBool(lv >  rv);
        case TokenType::LTE:   return EvalVal::fromBool(lv <= rv);
        case TokenType::GTE:   return EvalVal::fromBool(lv >= rv);
        case TokenType::PLUS:  return EvalVal::fromFloat(lv + rv);
        case TokenType::MINUS: return EvalVal::fromFloat(lv - rv);
        case TokenType::MUL:   return EvalVal::fromFloat(lv * rv);
        case TokenType::DIV:   return (rv!=0) ? EvalVal::fromFloat(lv/rv) : EvalVal::fromFloat(0);
        case TokenType::MOD:   {
            int li=(int)lv, ri=(int)rv;
            return EvalVal::fromInt(ri!=0 ? li%ri : 0);
        }
        default: return EvalVal::fromFloat(0);
    }
}

AVLIndex* Executor::getOrBuildIndex(const char* tableName, int colIdx) {
    for (int i = 0; i < indexCount_; ++i)
        if (strcmp(indexedTable_[i], tableName) == 0 && indexedCol_[i] == colIdx)
            return indices_[i];

    if (indexCount_ >= MAX_TABLES) return nullptr;
    TableSchema schema;
    if (!catalog_.getSchema(tableName, schema)) return nullptr;

    AVLIndex* idx = new AVLIndex();
    idx->build(schema.filePath, schema, colIdx);
    snprintf(indexedTable_[indexCount_], 64, "%s", tableName);
    indexedCol_[indexCount_] = colIdx;
    indices_[indexCount_]    = idx;
    ++indexCount_;
    return idx;
}

void Executor::benchmarkIndex(const char* tableName, int pkColIdx, int searchKey) {
    TableSchema schema;
    if (!catalog_.getSchema(tableName, schema)) return;

    printf("\n========== BENCHMARK: Sequential Scan vs AVL Index ==========\n");
    printf("Table: %s  |  Searching for key=%d\n", tableName, searchKey);

    // Build a simple WHERE with pk = searchKey
    char sql[256];
    snprintf(sql, 256, "SELECT * FROM %s WHERE %s == %d",
             tableName, schema.columns[pkColIdx].name, searchKey);
    Parser parser;
    ParsedQuery q;
    parser.parse(sql, q);

    // --- Sequential scan ---
    printf("\n--- Sequential Scan ---\n");
    clock_t t0 = clock();
    int cnt1 = seqScan(schema, q, true, 5);
    clock_t t1 = clock();
    double seqTime = 1000.0 * (t1 - t0) / CLOCKS_PER_SEC;
    printf("Sequential scan: %d row(s) found  [%.3f ms]\n", cnt1, seqTime);
    LOG_INFO("[Benchmark] Sequential scan for key=%d: %d rows, %.3f ms", searchKey, cnt1, seqTime);

    // --- AVL index scan ---
    printf("\n--- AVL Index Scan ---\n");
    clock_t t2 = clock();
    AVLIndex* idx = getOrBuildIndex(tableName, pkColIdx);
    clock_t t3 = clock();
    double buildTime = 1000.0 * (t3 - t2) / CLOCKS_PER_SEC;
    printf("AVL index build time: %.3f ms  (index size: %d keys)\n",
           buildTime, idx ? idx->size() : 0);

    if (idx) {
        clock_t t4 = clock();
        int cnt2 = indexScan(schema, *idx, q, searchKey, true);
        clock_t t5 = clock();
        double idxTime = 1000.0 * (t5 - t4) / CLOCKS_PER_SEC;
        printf("AVL index scan:   %d row(s) found  [%.3f ms]\n", cnt2, idxTime);
        LOG_INFO("[Benchmark] AVL index scan for key=%d: %d rows, %.3f ms", searchKey, cnt2, idxTime);
        printf("\nSpeedup: %.1fx  (scan=%.3fms vs index_lookup=%.3fms)\n",
               (seqTime > 0) ? seqTime/idxTime : 0, seqTime, idxTime);
    }
    printf("==============================================================\n\n");
}

void Executor::executeJoin(const char* t1, const char* t2, const char* t3, int printLimit) {
    printf("\n========== 3-TABLE JOIN OPTIMIZER ==========\n");
    const char* joinTbls[2];
    int joinCnt = 0;
    if (t2) joinTbls[joinCnt++] = t2;
    if (t3) joinTbls[joinCnt++] = t3;

    JoinOptimizer opt;
    JoinOptimizer::JoinPlan plan = opt.optimize(t1, joinTbls, joinCnt, catalog_);
    opt.printPlan(plan);

    // LOG the MST decision
    if (plan.tableCount == 3)
        LOG_INFO("[LOG] Multi-table join routed via MST: %s -> %s -> %s",
                 plan.orderedTables[0], plan.orderedTables[1], plan.orderedTables[2]);
    else if (plan.tableCount == 2)
        LOG_INFO("[LOG] Multi-table join routed via MST: %s -> %s",
                 plan.orderedTables[0], plan.orderedTables[1]);

    // Execute join in MST order (nested loop join – simple, correct)
    TableSchema s0, s1, s2;
    bool ok0 = catalog_.getSchema(plan.orderedTables[0], s0);
    bool ok1 = (plan.tableCount > 1) && catalog_.getSchema(plan.orderedTables[1], s1);
    if (plan.tableCount > 2) catalog_.getSchema(plan.orderedTables[2], s2);

    if (!ok0) { LOG_ERROR("Table not found: %s", plan.orderedTables[0]); return; }

    printf("\nSample join results (first %d rows):\n", printLimit);
    int h0 = SystemCatalog::tableHash(s0.tableName);
    int pg0Count = DiskManager::pageCount(s0.filePath);
    int printed  = 0;

    for (int pid0 = 0; pid0 < pg0Count && printed < printLimit; ++pid0) {
        Page* pg0 = pool_.fetchPage(s0.filePath, h0, pid0);
        for (int ri0 = 0; ri0 < pg0->getRecordCount() && printed < printLimit; ++ri0) {
            Row* r0 = pg0->getRow(s0, ri0);
            if (!r0) continue;

            if (ok1) {
                int h1 = SystemCatalog::tableHash(s1.tableName);
                int pg1Count = DiskManager::pageCount(s1.filePath);
                int fk0 = (r0->getField(0) && r0->getField(0)->type()==FieldType::INT)
                          ? static_cast<IntField*>(r0->getField(0))->value() : -1;

                for (int pid1 = 0; pid1 < pg1Count && printed < printLimit; ++pid1) {
                    Page* pg1 = pool_.fetchPage(s1.filePath, h1, pid1);
                    for (int ri1 = 0; ri1 < pg1->getRecordCount() && printed < printLimit; ++ri1) {
                        Row* r1 = pg1->getRow(s1, ri1);
                        if (!r1) continue;
                        int custkey = (r1->getField(1) && r1->getField(1)->type()==FieldType::INT)
                                      ? static_cast<IntField*>(r1->getField(1))->value() : -2;
                        if (custkey == fk0) {
                            char c0[32], c1[32], c2[32];
                            r0->getField(0)->toString(c0, 32);
                            r0->getField(1)->toString(c1, 32);
                            r1->getField(0)->toString(c2, 32);
                            printf("custkey=%-6s  name=%-25s  orderkey=%-6s\n", c0, c1, c2);
                            ++printed;
                        }
                        delete r1;
                    }
                }
            } else {
                char c0[32], c1[32];
                r0->getField(0)->toString(c0, 32);
                r0->getField(1)->toString(c1, 32);
                printf("custkey=%-6s  name=%-25s\n", c0, c1);
                ++printed;
            }
            delete r0;
        }
    }
    printf("============================================\n\n");
}

int Executor::stressTestBufferPool(const char* tableName, int maxRecords) {
    TableSchema schema;
    if (!catalog_.getSchema(tableName, schema)) return 0;

    LOG_INFO("[MemStress] Starting buffer pool stress test on '%s' (maxRecs=%d, pool_capacity=%d)",
             tableName, maxRecords, pool_.capacity());

    ParsedQuery q;
    q.hasWhere = false;
    int scanned = seqScan(schema, q, false, 0, maxRecords);

    int evictions = pool_.getEvictionCount();
    LOG_INFO("[MemStress] Stress test complete. Scanned %d records. "
             "LRU cache evictions: %d", scanned, evictions);
    printf("\n===== Memory Stress Test =====\n");
    printf("Table:           %s\n", tableName);
    printf("Records scanned: %d\n", scanned);
    printf("Buffer capacity: %d pages\n", pool_.capacity());
    printf("LRU evictions:   %d\n", evictions);
    printf("==============================\n\n");
    return evictions;
}
