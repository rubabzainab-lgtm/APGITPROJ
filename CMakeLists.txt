cmake_minimum_required(VERSION 3.10)
project(NanoDB)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(src)

set(COMMON_SRCS
    src/parser/Lexer.cpp
    src/parser/Parser.cpp
    src/index/AVLIndex.cpp
    src/optimizer/JoinOptimizer.cpp
    src/executor/Executor.cpp
    src/tpch/DataGenerator.cpp
)

add_executable(nanodb src/main.cpp ${COMMON_SRCS})
add_executable(test_runner test_runner.cpp ${COMMON_SRCS})