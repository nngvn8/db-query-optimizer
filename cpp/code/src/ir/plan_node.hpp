#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include <jsoncpp/json/value.h> // Ensure you have this linked
#include <ir/abstract_ir.hpp>
#include <ir/ir_base.hpp>
#include <translation/item_builder.h>

class PlanNode {
public:
    // 1. State: Raw JSON Data
    std::optional<BaseType::JsonRawData> rawJson;

    // 2. State: Abstract Tree information
    using AbstractData = std::variant<std::monostate, AbstractSource, AbstractJoin, AbstractAgg, AbstractSort, AbstractResult>;
    AbstractData abstractData;

    // 3. State: IR Tree information
    using IRData = std::variant<std::monostate,
        IR::TableBaseNode,
        IR::FetchNode,
        IR::SelectNode,
        IR::UpdateNode, // NYI
        IR::InsertNode, // NYI
        IR::DeleteNode, // NYI
        IR::AggNode,
        IR::JoinNode,
        IR::FilterNode,
        IR::GroupByNode,
        IR::SortOrderNode,
        IR::LimitNode,
        IR::MapNode,
        IR::SetOperationNode,
        IR::ResultNode,
        IR::MaterializeNode,
        IR::PositionList,
        IR::Bitmap
    >;

    // 3. State: API Item Tree information
    using ApiData = std::variant<std::variant<std::monostate,
        ItemBuilder::FetchNode, // relates to Table
        ItemBuilder::FilterNode, // relates to Filter
        ItemBuilder::JoinNode, // relates to Join
        ItemBuilder::MapNode, // currently missing in IrData
        ItemBuilder::MaterializeNode, // currently missing in IrData
        ItemBuilder::MultiGroupNode, // relates to Group
        ItemBuilder::SetOperationNode, // relates to Set Operation
        ItemBuilder::SortNode, // relates to SortNode
        ItemBuilder::AggNode, // aggregate node currently missing in IR! (included in select)
        ItemBuilder::ResultNode // relates to select node
    >>;
    ApiData apiData;

    // 4. Tree Structure
    std::vector<std::unique_ptr<PlanNode>> children {};

    // Constructors
    PlanNode() = default;
    explicit PlanNode(const Json::Value& queryPlan);

private:
    // Parsing Helpers
    static BaseType::PlanParams parsePlanParams(const Json::Value& json);
    static BaseType::Estimates parseEstimates(const Json::Value& json);
    static BaseType::Measures parseMeasures(const Json::Value& json);
};

void printPlanTree(const PlanNode& node, const std::string& prefix, bool isLast);
