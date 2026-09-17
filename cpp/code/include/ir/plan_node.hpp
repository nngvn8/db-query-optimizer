/**
 * @file plan_node.hpp
 * @brief Defines the main components of the query plan Intermediate Representation (IR).
 *
 * This file contains the definitions for `PlanNode`, which is the fundamental building block
 * of the query plan tree. It also defines the data structures that hold information
 * at different stages of the query processing pipeline, from raw JSON parsing to the
 * final API data representation.
 */

#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include <json/value.h>
#include "ir/abstract_ir.hpp"
#include "ir/ir_types.hpp"
#include "translation/item_builder.hpp"

/**
 * @struct IrData
 * @brief Holds the data for a plan node in the main optimization IR.
 *
 * This structure contains information about the operator, its input and output columns,
 * and whether it outputs a position list or materialized values.
 */
struct IrData {
    /// The input columns for the operation.
    std::vector<BaseType::TableColumn> inputColumns;
    /// The output columns of the operation.
    std::vector<BaseType::TableColumn> outputCols;
    /// True if the node outputs a position list.
    bool outputsPosList;
    /// True if the node outputs materialized values.
    bool outputsMatVals;

    /// A variant holding the specific information for the operator.
    using OpInfo = std::variant<std::monostate, JoinOp, SemiJoinOp, GroupOp, FetchOp, AggOp, FilterOp, SortOp, MapOp, SetOp, SelectOp, MatOp>;
    OpInfo opInfo;

    /**
     * @brief Checks if the OpInfo variant holds a specific operator type.
     * @tparam T The operator type to check for.
     * @return True if the variant holds the specified type, false otherwise.
     */
    template<typename T> bool is() const { return std::holds_alternative<T>(opInfo);};

    /**
     * @brief Gets a view of the operator-specific data if it matches the requested type.
     * @tparam ViewT The view type to get.
     * @return An optional containing the view if the type matches, std::nullopt otherwise.
     */
    template<typename ViewT> std::optional<ViewT> get_view_if() {
        if (std::holds_alternative<typename ViewT::OpType>(opInfo)) {
            return ViewT(*this);
        }
        return std::nullopt;
    }
};

/**
 * @class PlanNode
 * @brief The main class representing a node in the query plan tree.
 *
 * A PlanNode can hold data in different representations, corresponding to
 * different stages of the query processing pipeline.
 */
class PlanNode {
public:
    // Stage 1 for JSON plan plan parsing
    /// Optional raw JSON data, used in the first stage of JSON plan parsing.
    std::optional<BaseType::JsonRawData> rawJson;

    // Stage 2 for JSON plan parsing
    /// Abstract data representation, used in the second stage of JSON plan parsing.
    using AbstractData = std::variant<std::monostate, AbstractSource, AbstractJoin, AbstractAgg, AbstractSort, AbstractResult>;
    AbstractData abstractData;

    // Representation for Optimization, translated into from:
    // - AST or
    // - Stage 2 of JSON Plan Parsing
    /// The main IR data representation, used for optimization.
    IrData irData;


    // API Item Tree information: used to build workitems from
    /// API data representation, used to build work items.
    using ApiData = std::variant<std::monostate,
        ItemBuilder::FetchNode, // relates to Table
        ItemBuilder::FilterNode, // relates to Filter
        ItemBuilder::JoinNode, // relates to Join
        ItemBuilder::SemiJoinNode, // relates to SemiJoin
        ItemBuilder::MapNode, // currently missing in IrData
        ItemBuilder::MaterializeNode, // currently missing in IrData
        ItemBuilder::MultiGroupNode, // relates to Group
        ItemBuilder::SetOperationNode, // relates to Set Operation
        ItemBuilder::SortNode, // relates to SortNode
        ItemBuilder::AggNode, // aggregate node currently missing in IR! (included in select)
        ItemBuilder::ResultNode // relates to select node
    >;
    ApiData apiData;

    // Tree Structure
    /// The children of this node in the query plan tree.
    std::vector<std::shared_ptr<PlanNode>> children {};

    // Constructors
    PlanNode() = default;
    /**
     * @brief Constructs a PlanNode from a JSON object.
     * @param queryPlan The JSON object representing the query plan.
     */
    explicit PlanNode(const Json::Value& queryPlan);

private:
    // Parsing helpers for parsing of JSON plans
    static BaseType::PlanParams parsePlanParams(const Json::Value& json);
    static BaseType::Estimates parseEstimates(const Json::Value& json);
    static BaseType::Measures parseMeasures(const Json::Value& json);
};

/**
 * @brief Prints debug information for a PlanNode.
 * @param planNode The PlanNode to print.
 */
void printDebug(const PlanNode& planNode);

/**
 * @brief Prints the entire query plan tree starting from the given node.
 * @param node The root of the tree to print.
 * @param contentType The type of content to print for each node.
 */
void printPlanTree(const PlanNode& node, int contentType = 2);

/**
 * @brief Prints a sequenced plan (a vector of PlanNode pointers).
 * @param plan_seq The sequence of plan nodes to print.
 * @param contentType The type of content to print for each node.
 */
void printSequencedPlan(const std::vector<const PlanNode*> plan_seq, int contentType = 2);

/**
 * @brief Prints a single PlanNode.
 * @param node The node to print.
 * @param mode The printing mode.
 */
void printNode(const PlanNode& node, int mode = 2);
