#pragma once
#include "../catalog/Schema.h"
#include <cstdio>

// Generates synthetic TPC-H style data and writes it to the database files.
class DataGenerator {
public:
    // Generate numCustomers customer records, write to filePath.
    static void generateCustomers(const TableSchema& schema,
                                  int numCustomers,
                                  const char* filePath);

    // Generate numOrders orders records for customers 1..numCustomers.
    static void generateOrders(const TableSchema& schema,
                               int numOrders, int maxCustKey,
                               const char* filePath);

    // Generate numLineitems line items for orders 1..numOrders.
    static void generateLineitems(const TableSchema& schema,
                                  int numLineitems, int maxOrderKey,
                                  const char* filePath);

private:
    static unsigned int nextRand(unsigned int& seed);
    static float randFloat(unsigned int& seed, float lo, float hi);
    static int   randInt  (unsigned int& seed, int lo, int hi);
    static void  randStr  (unsigned int& seed, char* out, int len);
};
