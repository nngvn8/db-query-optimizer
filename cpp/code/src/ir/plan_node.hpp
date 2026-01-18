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
    using IrData = std::variant<std::monostate,
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
    IrData irData;


    // 3. State: API Item Tree information
    using ApiData = std::variant<std::monostate,
        std::vector<ItemBuilder::FetchNode>, // relates to Table
        ItemBuilder::FilterNode, // relates to Filter
        ItemBuilder::JoinNode, // relates to Join
        ItemBuilder::MapNode, // currently missing in IrData
        ItemBuilder::MaterializeNode, // currently missing in IrData
        ItemBuilder::MultiGroupNode, // relates to Group
        ItemBuilder::SetOperationNode, // relates to Set Operation
        ItemBuilder::SortNode, // relates to SortNode
        ItemBuilder::AggNode, // aggregate node currently missing in IR! (included in select)
        ItemBuilder::ResultNode // relates to select node
    >;
    ApiData apiData;

    // 4. Tree Structure
    std::vector<std::shared_ptr<PlanNode>> children {};

    // Constructors
    PlanNode() = default;
    explicit PlanNode(const Json::Value& queryPlan);

    // IrData IrBaseNode getters
    std::vector<BaseType::TableColumn>* getIrDataInputColumns() {
        return std::visit([](auto& n) -> std::vector<BaseType::TableColumn>* {
            if constexpr (requires { n.inputColumns; }) {
                return &n.inputColumns;
            } else {
                return nullptr;
            }
        }, irData);
    }

    BaseType::TableColumn* getIrDataOutputColumn() {
        return std::visit([](auto& n) -> BaseType::TableColumn* {
            if constexpr (requires { n.outputColumn; }) {
                return &n.outputColumn;
            } else {
                return nullptr;
            }
        }, irData);
    }


    std::vector<BaseType::TableColumn>* getIrDataInputColumnsC17() {
        return std::visit([](auto& n) -> std::vector<BaseType::TableColumn>* {
            if constexpr (!std::is_same_v<std::decay_t<decltype(n)>, std::monostate>) {
                return &n.inputColumns;
            } else {
                return nullptr;
            }
        }, irData);
    }

private:
    // Parsing Helpers
    static BaseType::PlanParams parsePlanParams(const Json::Value& json);
    static BaseType::Estimates parseEstimates(const Json::Value& json);
    static BaseType::Measures parseMeasures(const Json::Value& json);
};

void printDebug(const PlanNode& planNode);
void printPlanTree(const PlanNode& node, int contentType = 2);
void printSequencedPlan(const std::vector<const PlanNode*> plan_seq, int contentType = 2);
void printNode(const PlanNode& node, int mode = 2);
