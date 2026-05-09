# NanoDB — Mini Database Engine

A from-scratch mini database engine implementing core systems programming concepts for the NanoDB Architecture & Query Optimizer project.

**GitHub**: _[Add your repository URL here]_

---

## Architecture

### Core Components

| Layer | Component | Data Structure | Complexity |
|-------|-----------|----------------|------------|
| Memory | Buffer Pool (Pager) | Doubly Linked List + Hash Map | O(1) evict/access |
| Types | Polymorphic Field | Base class + virtual dispatch | N/A |
| Catalog | System Catalog | Custom Hash Map (djb2 + chaining) | O(1) lookup |
| Parsing | WHERE Evaluator | Custom Stack (Shunting-Yard) | O(N) |
| Indexing | AVL Tree Index | Self-balancing BST | O(log N) |
| Optimizer | Join Optimizer | Graph + Kruskal's MST | O(E log E) |
| Concurrency | Query Scheduler | Min-Heap Priority Queue | O(log N) |

### No STL Used
Every data structure is built from scratch:
- `DoublyLinkedList<T>` — for LRU eviction chain
- `Stack<T>` — for Shunting-Yard infix-to-postfix conversion
- `PriorityQueue<T>` — min-heap for admin query scheduling
- `HashMap<V>` / `IntHashMap<V>` — for O(1) catalog and LRU lookups
- `AVLTree<T>` — self-balancing BST with rotation logic

---

## Building

### Prerequisites
- `g++` with C++17 support
- `make`
- Linux/macOS (or WSL on Windows)

### Build All Targets
```bash
cd nanodb
make all
```

### Build Specific Targets
```bash
make nanodb        # Main engine binary
make test_runner   # Automated test runner
```

---

## Running

### Run Demo (All 7 Test Cases)
```bash
./nanodb --demo
```

### Run Automated Test Runner
```bash
./test_runner
```
This reads `queries.txt` and executes all 50 queries, generating `nanodb_execution.log`.

### Interactive Mode
```bash
./nanodb --interactive
```
Then type SQL at the prompt:
```
nanodb> SELECT * FROM customer WHERE c_acctbal > 9000
nanodb> exit
```

### Verify No Memory Leaks (Valgrind)
```bash
make valgrind
```

---

## Demo Test Cases

| Test | Feature | Query |
|------|---------|-------|
| A | Parser & Evaluator | Complex WHERE with AND/OR |
| B | AVL Index vs Sequential Scan | Timing comparison on 20,000 records |
| C | 3-Table Join (MST Optimizer) | `customer JOIN orders JOIN lineitem` |
| D | LRU Memory Stress | 50-page pool, 5,000 lineitem records |
| E | Priority Queue | Admin UPDATE jumps ahead of 50 user queries |
| F | Deep Expression Tree | Nested arithmetic + comparison |
| G | Durability & Persistence | Insert 5 rows, flush, verify on reload |

---

## Dataset

TPC-H synthetic data generated at startup (if not present):
- `customer` — 20,000 records
- `orders` — 30,000 records  
- `lineitem` — 50,000 records

Data files stored in `db_files/` as binary page files (4,096 bytes/page).

---

## Execution Log

All engine decisions are logged to `nanodb_execution.log`:

```
[HH:MM:SS][INFO] [LOG] Page 42 evicted via LRU, written to disk
[HH:MM:SS][INFO] [LOG] Infix "c_acctbal > 5000" converted to Postfix "c_acctbal 5000 >"
[HH:MM:SS][INFO] [LOG] Multi-table join routed via MST: customer -> orders -> lineitem
```

---

## File Structure

```
nanodb/
├── src/
│   ├── common/          # Generic data structures (no STL)
│   │   ├── Logger.h
│   │   ├── DoublyLinkedList.h
│   │   ├── Stack.h
│   │   ├── PriorityQueue.h
│   │   ├── HashMap.h
│   │   ├── AVLTree.h
│   │   └── Graph.h
│   ├── storage/         # Page, BufferPool, DiskManager, Field, Row
│   ├── catalog/         # Schema definitions, SystemCatalog
│   ├── parser/          # Lexer, Token, Parser (Shunting-Yard)
│   ├── index/           # AVL-tree based column index
│   ├── optimizer/       # Graph + Kruskal's MST join optimizer
│   ├── executor/        # Query execution engine + postfix evaluator
│   ├── tpch/            # TPC-H synthetic data generator
│   └── main.cpp
├── test_runner.cpp      # Automated workload runner
├── queries.txt          # 50 SQL workload queries
├── Makefile
└── README.md
```

---

## Complexity Summary

| Structure | Insert | Search | Delete | Space |
|-----------|--------|--------|--------|-------|
| Buffer Pool (LRU) | O(1) | O(1) | O(1) | O(P) |
| AVL Tree Index | O(log N) | O(log N) | O(log N) | O(N) |
| Hash Map (Catalog) | O(1) avg | O(1) avg | O(1) avg | O(N) |
| Priority Queue | O(log N) | O(1) peek | O(log N) | O(N) |
| Shunting-Yard Parser | — | — | — | O(N) |
| Kruskal's MST | — | — | — | O(E log E) |

---

## Clean Up
```bash
make clean     # Remove binaries, log, and db_files/
```
