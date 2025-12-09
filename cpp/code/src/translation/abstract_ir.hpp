#pragma once
#include <string>
#include <vector>
#include <variant>

struct AbstractSource {
    std::string basetable;
    std::vector<std::string> filters;
}; 
struct AbstractJoin {
    std::string left_table;
    std::string right_table;
    std::string condition;

}; 
struct AbstractAgg {
    std::string agg_type;
}; 
struct AbstractSort {
    std::string column_names;
    bool asc;

}; 
struct AbstractResult {};


struct GetNodeName {
    std::string operator()(const std::monostate&) { return "Empty"; }
    std::string operator()(const AbstractSource&) { return "AbstractSource"; }
    std::string operator()(const AbstractJoin&)   { return "AbstractJoin"; }
    std::string operator()(const AbstractAgg&)    { return "AbstractAgg"; }
    std::string operator()(const AbstractSort&)   { return "AbstractSort"; }
    std::string operator()(const AbstractResult&) { return "AbstractResult"; }
};
// #include <variant>
// #include <vector>
// #include <string>
// #include <memory>
// #include <optional>

// // --- Abstract Node Definitions ---

// struct AbstractSource {
//     std::string tableName;
//     std::string alias;
//     std::optional<std::string> filterPredicate; // Captured from Seq Scan or Index Cond
// };

// struct AbstractJoin {
//     std::string joinType; // "Inner", "Left", etc.
//     // We don't need 'Hash' logic here, just the join keys
//     std::string leftKey; 
//     std::string rightKey;
//     std::string op; // "="
// };

// struct AbstractAgg {
//     std::vector<std::string> groupKeys;
//     std::vector<std::string> aggFunctions; // e.g., "SUM(lo_revenue)"
// };

// struct AbstractSort {
//     std::vector<std::string> sortKeys;
//     std::vector<bool> sortOrders; 
// };

// struct AbstractResult {
//     std::vector<std::string> outputColumns;
// };




