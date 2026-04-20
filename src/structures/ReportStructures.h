#pragma once

#include <string>

struct ClosedOrdersTotal {
    double volume;
    double profit;
    double commission;
    double storage;
};

struct OpenOrdersTotal {
    double volume;
    double profit;
    double commission;
    double storage;
};

struct PendingOrdersTotal {
    double volume;
};

struct TransactionsTotal {
    double profit;
};