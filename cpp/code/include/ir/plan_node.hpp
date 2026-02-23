#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include "jsoncpp/json/value.h" // Ensure you have this linked
#include "ir/abstract_ir.hpp"
#include "ir/ir_types.hpp"
#include "translation/item_builder.hpp"


struct IrData {
    std::vector<BaseType::TableColumn> inputColumns;
    std::vector<BaseType::TableColumn> outputCols;
    bool outputsPosList;
    bool outputsMatVals;

    using OpInfo = std::variant<std::monostate, JoinOp, SemiJoinOp, GroupOp, FetchOp, AggOp, FilterOp, SortOp, MapOp, SetOp, SelectOp, MatOp>;
    OpInfo opInfo;

    template<typename T> bool is() const { return std::holds_alternative<T>(opInfo);};
    template<typename ViewT> std::optional<ViewT> get_view_if() {
        if (std::holds_alternative<typename ViewT::OpType>(opInfo)) {
            return ViewT(*this);
        }
        return std::nullopt;
    }
};

class PlanNode {
public:
    // Stage 1 for JSON plan plan parsing
    std::optional<BaseType::JsonRawData> rawJson;

    // Stage 2 for JSON plan parsing
    using AbstractData = std::variant<std::monostate, AbstractSource, AbstractJoin, AbstractAgg, AbstractSort, AbstractResult>;
    AbstractData abstractData;

    // Representation for Optimization, translated into from:
    // - AST or
    // - Stage 2 of JSON Plan Parsing
    IrData irData;


    // API Item Tree information: used to build workitems from
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
    std::vector<std::shared_ptr<PlanNode>> children {};

    // Constructors
    PlanNode() = default;
    explicit PlanNode(const Json::Value& queryPlan);

private:
    // Parsing helpers for parsing of JSON plans
    static BaseType::PlanParams parsePlanParams(const Json::Value& json);
    static BaseType::Estimates parseEstimates(const Json::Value& json);
    static BaseType::Measures parseMeasures(const Json::Value& json);
};

void printDebug(const PlanNode& planNode);
void printPlanTree(const PlanNode& node, int contentType = 2);
void printSequencedPlan(const std::vector<const PlanNode*> plan_seq, int contentType = 2);
void printNode(const PlanNode& node, int mode = 2);
