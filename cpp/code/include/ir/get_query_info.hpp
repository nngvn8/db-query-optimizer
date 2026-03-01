/**
 * @file get_query_info.hpp
 * @brief Defines data structures for holding parsed SQL query information and the parsing function.
 *
 * This file provides the structures that represent the different parts of a SQL query,
 * such as selections, aggregations, groupings, and sorting. It also declares the
 * function to parse a raw SQL string into these structures.
 */

#ifndef GET_QUERY_INFO_HPP
#define GET_QUERY_INFO_HPP

#include <string>
#include <vector>
#include <set>

/**
 * @struct Selection
 * @brief Represents a selection in a SQL query.
 */
struct Selection {
    /// The full expression (e.g., "d_year" or "SUM(x)").
    std::string content;
    /// Optional AS alias.
    std::string alias;
};

/**
 * @struct Aggregation
 * @brief Represents an aggregation in a SQL query.
 */
struct Aggregation {
    /// The aggregation function (e.g., "SUM", "COUNT").
    std::string func;
    /// The inner expression (e.g., "lo_revenue").
    std::string mapping;
    /// Optional AS alias.
    std::string alias;
};

/**
 * @struct Sort
 * @brief Represents a sorting condition in a SQL query.
 */
struct Sort {
    /// The field to sort by.
    std::string field;
    /// The sorting order: true for ASC, false for DESC.
    bool asc;
};

/**
 * @struct SqlQueryData
 * @brief A struct to hold all the parsed data from a SQL query.
 */
struct SqlQueryData {
    /// The list of selections.
    std::vector<Selection> selections;
    /// The list of aggregations.
    std::vector<Aggregation> aggregations;
    /// The list of group-by columns.
    std::vector<std::string> groupBys;
    /// The set of tables in the query.
    std::set<std::string> tables;
    /// The set of attributes (columns) in the query.
    std::set<std::string> attributes;
    /// The list of conditions (WHERE clause).
    std::vector<std::string> conditions;
    /// The list of sorting conditions (ORDER BY clause).
    std::vector<Sort> sorting;
};

/**
 * @brief Parses a raw SQL query string into a SqlQueryData struct.
 * @param sql The SQL query string.
 * @return A SqlQueryData struct containing the parsed information.
 */
SqlQueryData parseQuery(std::string sql);

/**
 * @brief Prints the contents of a SqlQueryData struct to the console.
 * @param data The SqlQueryData struct to print.
 */
void print_query_data(const SqlQueryData& data);

#endif
