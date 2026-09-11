/**
 * @file ir_types.hpp
 * @brief Defines the specific operator types for the query plan IR.
 *
 * This file contains the definitions for the various operator-specific data structures
 * that are used within the `IrData` struct. Each struct holds the parameters
 * for a particular type of query operation, such as a join, filter, or aggregation.
 */

#pragma once

#include <vector>
#include <variant>
#include <optional>
#include <string>
#include <set>
#include <type_traits>

#include "ir/base_types.hpp"

/**
 * @struct GroupOp
 * @brief Represents a group-by operation.
 */
struct GroupOp {
    /// The sort order for each grouping column.
    std::vector<bool> sortOrders;
    /// Whether the group-by has an aggregation.
    bool hasAgg = false;
    /// Whether to store the extends of the groups.
    bool storeExtends = true;
};

/**
 * @struct FetchOp
 * @brief Represents a fetch operation.
 */
struct FetchOp {
    /// Whether the fetch was from a base table node.
    bool wasTableBaseNode;
};

/**
 * @struct AggOp
 * @brief Represents an aggregation operation.
 */
struct AggOp {
    /// The aggregation function to apply.
    AggFunc aggFunc;
};

/**
 * @struct JoinOp
 * @brief Represents a join operation.
 */
struct JoinOp {
    /// The type of join.
    BaseType::Join joinType; // currently not processed by the system
    /// The join predicate.
    CompType joinPredicate;
    /// The output column of the join.
    BaseType::TableColumn outputCol;
};

/**
 * @struct SemiJoinOp
 * @brief Represents a semi-join operation.
 */
struct SemiJoinOp {
    /// The type of join.
    BaseType::Join joinType; // currently not processed by the system
    /// The join predicate.
    CompType joinPredicate;
    /// The output column of the semi-join.
    BaseType::TableColumn outputCol;
};

/**
 * @struct FilterOp
 * @brief Represents a filter operation.
 */
struct FilterOp {
    /// The type of comparison in the filter.
    CompType filterType;
    /// The arguments for the filter.
    std::vector<std::variant<uint64_t, float, std::string>> filterArgs;
};

/**
 * @struct SortOp
 * @brief Represents a sort operation.
 */
struct SortOp {
    /// The list of columns to sort by, with their ordering.
    std::vector<BaseType::OrderDescription> columnList;
    /// An optional existing index to use for sorting.
    std::optional<BaseType::TableColumn> existingIdx;
};

// struct LimitOp currently not supported by system

/**
 * @struct MapOp
 * @brief Represents a map operation (e.g., arithmetic calculation).
 */
struct MapOp {
    /// The arithmetic operator to apply.
    ArithOp operatorType;
    /// The value to apply the operator with (can be a column or a literal).
    std::variant<std::monostate, uint64_t, float, std::string> partnerVal;
};

/**
 * @struct SetOp
 * @brief Represents a set operation (e.g., UNION, INTERSECT, EXCEPT).
 */
struct SetOp {
    /// The type of set operation.
    RelOp operation;
};

/**
 * @struct SelectOp
 * @brief Represents the final select operation that produces the query result.
 */
struct SelectOp {
    /// An optional index for the result.
    std::optional<BaseType::TableColumn> resultIdx;
    /// The headers for the result columns.
    std::vector<std::string> resultHeaders;
};

/**
 * @struct MatOp
 * @brief Represents a materialization operation.
 */
struct MatOp {

};

// currently not supported by the system
// struct BitmapOp {
//     std::vector<bool> map;
// };
