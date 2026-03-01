#ifndef GET_QUERY_INFO_HPP
#define GET_QUERY_INFO_HPP

#include <string>
#include <vector>
#include <set>

struct Selection {
    std::string content; // The full expression (e.g., "d_year" or "SUM(x)")
    std::string alias;   // Optional AS alias
};

struct Aggregation {
    std::string func;    // SUM, COUNT, etc.
    std::string mapping; // The inner expression e.g. "lo_revenue"
    std::string alias;   // Optional AS alias
};

struct Sort {
    std::string field;
    bool asc; // true for ASC, false for DESC
};

struct SqlQueryData {
    std::vector<Selection> selections;
    std::vector<Aggregation> aggregations;
    std::vector<std::string> groupBys;
    std::set<std::string> tables;
    std::set<std::string> attributes;
    std::vector<std::string> conditions;
    std::vector<Sort> sorting;
};

SqlQueryData parseQuery(std::string sql);
void print_query_data(const SqlQueryData& data);

#endif