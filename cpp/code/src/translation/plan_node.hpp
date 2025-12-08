#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include <jsoncpp/json/value.h> // Ensure you have this linked
#include <translation/ir_transformer.hpp>
#include <translation/ir_tree.hpp>
#include <translation/item_builder.h>

// --- 1. Data Structures (Matching our agreed design) ---

struct BaseTable {
    std::string fullName;
    std::string alias;
    bool isVirtual;
    std::string schema;
};

struct PlanParams {
    std::optional<BaseTable> baseTable;
    std::optional<std::string> filterPredicate;
    std::vector<std::string> sortKeys; // Empty vector if null/empty
    int parallelWorkers = 0;
    std::string index;
    std::optional<std::string> lookupKey;
    std::optional<std::string> subplanName;
};

struct Estimates {
    float cardinality;
    float cost;
};

struct Measures {
    float cardinality;
    float executionTime;
    std::optional<int> cacheHits;
    std::optional<int> cacheMisses;
};

struct JsonRawData {
    std::string nodeType;
    std::optional<std::string> nodeOperator;
    std::optional<std::string> subPlan;

    PlanParams planParams;
    Estimates estimates;
    Measures measures;
};

// Forward decls for variants (placeholders for now)
struct AbstractSource {}; struct AbstractJoin {}; struct AbstractAgg {}; 
struct AbstractSort {}; struct AbstractResult {};
struct ApiPlaceholder {}; // Placeholder for API item

class PlanNode {
public:
    // 1. State: Raw JSON Data
    std::optional<JsonRawData> rawJson;

    // 2. State: Abstract Tree information
    using AbstractData = std::variant<std::monostate, AbstractSource, AbstractJoin, AbstractAgg, AbstractSort, AbstractResult>;
    AbstractData abstractData;

    // 3. State: IR Tree information
    using IrData = std::variant<std::monostate, IrNode::SelectNode /*insert from ir_tree.hpp here*/>;

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
    static PlanParams parsePlanParams(const Json::Value& json);
    static Estimates parseEstimates(const Json::Value& json);
    static Measures parseMeasures(const Json::Value& json);
};