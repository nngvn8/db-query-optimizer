/**
 * @file abstract_ir.hpp
 * @brief Defines the data structures for the abstract query plan IR.
 *
 * This IR is used as an intermediate step when parsing query plans from JSON.
 * It represents the query plan in a more abstract way than the main optimization IR,
 * focusing on the high-level structure of the query.
 */

#pragma once
#include <string>
#include <vector>
#include <variant>
#include "ir/plan_node.hpp"

/**
 * @struct AbstractSource
 * @brief Represents a data source (a base table with optional filters) in the abstract IR.
 */
struct AbstractSource {
    /// The name of the base table.
    std::string basetable;
    /// A list of filter conditions.
    std::vector<std::string> filters;
};

/**
 * @struct AbstractJoin
 * @brief Represents a join operation in the abstract IR.
 */
struct AbstractJoin {
    /// The name of the left table.
    std::string left_table;
    /// The name of the right table.
    std::string right_table;
    /// The join condition.
    std::string condition;
};

/**
 * @struct AbstractAgg
 * @brief Represents an aggregation operation in the abstract IR.
 */
struct AbstractAgg {
    /// The type of aggregation (e.g., "SUM", "AVG").
    std::string agg_type;
    /// The column or expression to aggregate.
    std::string agg_mapping;
    /// The alias for the aggregation result.
    std::string agg_alias;
    /// The list of columns to group by.
    std::vector<std::string> grouping_cols;
};

/**
 * @struct AbstractSort
 * @brief Represents a sort operation in the abstract IR.
 */
struct AbstractSort {
    /// The names of the columns to sort by.
    std::vector<std::string> column_names;
    /// The aliases of the columns to sort by.
    std::vector<std::string> aliases;
    /// The sort order for each column (true for ASC, false for DESC).
    std::vector<bool> asc;
};

/**
 * @struct AbstractResult
 * @brief Represents the final result output of the query in the abstract IR.
 */
struct AbstractResult {
    /// The list of output columns.
    std::vector<std::string> output_cols;
};

/**
 * @struct GetNodeName
 * @brief A visitor for getting the name of an abstract IR node type.
 */
struct GetNodeName {
    std::string operator()(const std::monostate&) { return "Empty"; }
    std::string operator()(const AbstractSource&) { return "AbstractSource"; }
    std::string operator()(const AbstractJoin&)   { return "AbstractJoin"; }
    std::string operator()(const AbstractAgg&)    { return "AbstractAgg"; }
    std::string operator()(const AbstractSort&)   { return "AbstractSort"; }
    std::string operator()(const AbstractResult&) { return "AbstractResult"; }
};
