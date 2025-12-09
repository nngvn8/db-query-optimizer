#ifndef PARSE_QUERY_HPP
#define PARSE_QUERY_HPP

#include <string>
#include <vector>
#include <set>

// 1. For result display (preserves order)
struct SelectField {
    std::string content; // The full expression (e.g., "d_year" or "SUM(x)")
    std::string alias;   // Optional AS alias
};

// 2. For computation (redundant, but specific to aggregates)
struct Aggregation {
    std::string func;    // SUM, COUNT, etc.
    std::string mapping; // The inner expression e.g. "lo_revenue"
    std::string alias;   // Optional AS alias
};

struct SqlQueryData {
    // Both lists are now populated
    std::vector<SelectField> selections;   // All fields (cols + aggs)
    std::vector<Aggregation> aggregations; // Just the aggregates
    
    std::vector<std::string> groupBys;
    std::set<std::string> tables;
    std::set<std::string> attributes;
    std::vector<std::string> conditions;
    std::vector<std::string> sorting;
};

SqlQueryData parseQuery(std::string sql);
void print_query_data(const SqlQueryData& data);

#endif