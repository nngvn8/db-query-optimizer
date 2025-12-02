#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <fstream>
#include <bits/stdc++.h>
#include <sequentializer/sequentializer.hpp>
#include <translation/builder.hpp>

#include "plan_node.h"

void printDebug(const PlanNode& planNode) {
    // Check if raw data exists (it might not if we created a synthetic abstract node later)
    if (!planNode.rawJson.has_value()) {
        std::cout << "Node: [Synthetic/Abstract Node]" << std::endl;
        // You could print abstract data here if implemented
    } else {
        const JsonRawData& r = planNode.rawJson.value();
        
        // Print Header
        std::cout << "Node: " << r.nodeType;
        if (r.nodeOperator) std::cout << " | Op: " << *r.nodeOperator;
        std::cout << std::endl;

        // Print Params
        std::cout << "  Params: ";
        if (r.planParams.baseTable) {
            const auto& bt = *r.planParams.baseTable;
            std::cout << "[Table: " << bt.fullName << " (" << bt.alias << ")] ";
        }
        
        if (r.planParams.filterPredicate) 
            std::cout << "[Filter: " << *r.planParams.filterPredicate << "] ";
            
        if (!r.planParams.sortKeys.empty()) {
            std::cout << "[Sort: ";
            for(const auto& k : r.planParams.sortKeys) std::cout << k << " ";
            std::cout << "] ";
        }

        std::cout << "Workers: " << r.planParams.parallelWorkers;
        if (!r.planParams.index.empty()) std::cout << " Index: " << r.planParams.index;
        std::cout << std::endl;

        // Print Stats
        std::cout << "  Est: Card=" << r.estimates.cardinality << " Cost=" << r.estimates.cost << std::endl;
        
        std::cout << "  Mes: Card=" << r.measures.cardinality << " Time=" << r.measures.executionTime;
        if (r.measures.cacheHits) std::cout << " Hits=" << *r.measures.cacheHits;
        std::cout << std::endl;
    }

    std::cout << "--------------------------------------" << std::endl;

    // Recurse
    for (const auto& child : planNode.children) {
        if (child) printDebug(*child);
    }
}

// --- Parsing Helpers ---

PlanParams PlanNode::parsePlanParams(const Json::Value& json) {
    PlanParams params;

    // 1. Base Table (Handle Object or Null)
    const Json::Value& btJson = json["base_table"];
    if (!btJson.isNull() && btJson.isObject()) {
        BaseTable bt;
        bt.fullName = btJson.get("full_name", "").asString();
        bt.alias = btJson.get("alias", "").asString();
        bt.isVirtual = btJson.get("virtual", false).asBool();
        bt.schema = btJson.get("schema", "").asString();
        params.baseTable = bt;
    }

    // 2. Simple Fields
    if (!json["filter_predicate"].isNull()) 
        params.filterPredicate = json["filter_predicate"].asString();
    
    params.parallelWorkers = json.get("parallel_workers", 0).asInt();
    params.index = json.get("index", "").asString();
    
    if (!json["lookup_key"].isNull()) 
        params.lookupKey = json["lookup_key"].asString();

    if (!json["subplan_name"].isNull())
        params.subplanName = json["subplan_name"].asString();

    // 3. Sort Keys (Handle Array)
    const Json::Value& sortJson = json["sort_keys"];
    if (!sortJson.isNull() && sortJson.isArray()) {
        for (const auto& key : sortJson) {
            params.sortKeys.push_back(key.asString());
        }
    }

    return params;
}

Estimates PlanNode::parseEstimates(const Json::Value& json) {
    Estimates est;
    est.cardinality = json.get("cardinality", 0.0f).asFloat();
    est.cost = json.get("cost", 0.0f).asFloat();
    return est;
}

Measures PlanNode::parseMeasures(const Json::Value& json) {
    Measures mes;
    mes.cardinality = json.get("cardinality", 0.0f).asFloat();
    mes.executionTime = json.get("execution_time", 0.0f).asFloat();

    if (!json["cache_hits"].isNull())
        mes.cacheHits = json["cache_hits"].asInt();
    
    if (!json["cache_misses"].isNull())
        mes.cacheMisses = json["cache_misses"].asInt();

    return mes;
}

// --- Constructor ---

PlanNode::PlanNode(const Json::Value& queryPlan) {
    // 1. Parse Raw Data
    JsonRawData data;
    data.nodeType = queryPlan.get("node_type", "Unknown").asString();
    
    if (!queryPlan["operator"].isNull()) {
        data.nodeOperator = queryPlan["operator"].asString();
    }
    
    if (!queryPlan["subplan"].isNull()) {
        data.subPlan = queryPlan["subplan"].asString();
    }

    data.planParams = parsePlanParams(queryPlan["plan_params"]);
    data.estimates = parseEstimates(queryPlan["estimates"]);
    data.measures = parseMeasures(queryPlan["measures"]);

    // Move data into the optional slot
    this->rawJson = std::move(data);

    // 2. Recursively Parse Children
    const Json::Value& childrenJson = queryPlan["children"];
    if (!childrenJson.isNull() && childrenJson.isArray()) {
        for (const Json::Value& child : childrenJson) {
            this->children.push_back(std::make_unique<PlanNode>(child));
        }
    }
}

void test() {
    std::ifstream queryJson("q1-1-plan.json", std::ifstream::binary);
}
void printNode(const PlanNode& node){
    std::cout << node.rawJson->nodeType;
}

void printPlanTree(const PlanNode& node, const std::string& prefix, bool isLast) {
    // 1. Determine the drawing character for the current node
    std::cout << prefix;
    std::cout << (isLast ? "└── " : "├── ");

    // 2. Print node
    printNode(node);
    std::cout << std::endl;

    // 3. Prepare the new prefix for the children
    std::string newPrefix = prefix + (isLast ? "    " : "│   ");

    // 4. Recursively call the function for all children
    for (size_t i = 0; i < node.children.size(); ++i) {
        const auto& child = node.children[i];
        bool isChildLast = (i == node.children.size() - 1);
        printPlanTree(*child, newPrefix, isChildLast);
    }
}

void printSequencedPlan(const std::vector<const PlanNode*> plan_seq){
    for (const PlanNode* node : plan_seq) {
        printNode(*node);
        std::cout << "--";
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> file_names = {"q1-1-plan.json", "q1-2-plan.json", "q1-3-plan.json", "q2-1-plan.json", "q2-2-plan.json", "q2-3-plan.json", "q3-1-plan.json", "q3-2-plan.json", "q3-3-plan.json", "q3-4-plan.json", "q4-1-plan.json", "q4-2-plan.json", "q4-3-plan.json"};
    std::string base_dir = "/home/martin/University/09_KDB/ws25-optimizer-rust/pb-plans/";
    
    for (const std::string& file_name : file_names) {
    
        std::ifstream queryJson(base_dir + file_name);
        std::stringstream buffer;
        buffer << queryJson.rdbuf();
        std::string content = buffer.str();
        queryJson.close();

        std::string search = "NaN";
        std::string replace = "null";

        size_t pos = 0;
        while ((pos = content.find(search, pos)) != std::string::npos) {
            content.replace(pos, search.length(), replace);
            pos += replace.length();
        }
        std::stringstream modifiedStream(content);
        Json::Value queryPlan;
        modifiedStream >> queryPlan;

        std::unique_ptr planNodeRoot = std::make_unique<PlanNode>(queryPlan);
        // printDebug(*planNodeRoot);
        printPlanTree(*planNodeRoot, "", true);
        planNodeRoot = pruneTree(std::move(planNodeRoot));
        // printDebug(*planNodeRoot);
        printPlanTree(*planNodeRoot, "", true);
        // std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(planNodeRoot.get());
        // printSequencedPlan(sequenced_plan);
        // break;
    }
}
