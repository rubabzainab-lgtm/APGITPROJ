CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Isrc

# Source files
COMMON_SRCS  = src/parser/Lexer.cpp \
               src/parser/Parser.cpp \
               src/index/AVLIndex.cpp \
               src/optimizer/JoinOptimizer.cpp \
               src/executor/Executor.cpp \
               src/tpch/DataGenerator.cpp

MAIN_SRCS    = src/main.cpp $(COMMON_SRCS)
RUNNER_SRCS  = test_runner.cpp $(COMMON_SRCS)

# Targets
all: nanodb test_runner

nanodb: $(MAIN_SRCS)
        $(CXX) $(CXXFLAGS) -o nanodb $(MAIN_SRCS)
        @echo "✓ nanodb built"

test_runner: $(RUNNER_SRCS)
        $(CXX) $(CXXFLAGS) -o test_runner $(RUNNER_SRCS)
        @echo "✓ test_runner built"

run: nanodb
        ./nanodb --demo

run-interactive: nanodb
        ./nanodb --interactive

run-tests: test_runner
        ./test_runner

clean:
        rm -f nanodb test_runner nanodb_execution.log
        rm -rf db_files/

valgrind: nanodb
        valgrind --leak-check=full --error-exitcode=1 ./nanodb --demo

# Package source-only tarball for submission (no binaries or generated data)
TARBALL = nanodb-submission.tar.gz
zip:
        @echo "Creating $(TARBALL)..."
        @rm -f $(TARBALL)
        tar -czf $(TARBALL) \
                Makefile README.md queries.txt test_runner.cpp \
                src/main.cpp src/common/ src/storage/ src/catalog/ \
                src/parser/ src/index/ src/optimizer/ src/executor/ src/tpch/ \
                --exclude="*.o" --exclude="*.log"
        @echo ""
        @echo "✓ Submission package ready: $(TARBALL)"
        @tar -tzf $(TARBALL) | sort | sed 's/^/    /'

.PHONY: all run run-interactive run-tests clean valgrind zip
