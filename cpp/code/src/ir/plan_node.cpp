#pragma once

#include "plan_node.hpp"

#include <iostream>
#include <jsoncpp/json/json.h>
#include <fstream>

#include <ir/parse_query.hpp>
#include <ir/abstract_ir.hpp>

// --- Parsing Helpers ---

BaseType::PlanParams PlanNode::parsePlanParams(const Json::Value& json) {
    BaseType::PlanParams params;

    // 1. Base Table (Handle Object or Null)
    const Json::Value& btJson = json["base_table"];
    if (!btJson.isNull() && btJson.isObject()) {
        BaseType::Table bt;
        bt.name = btJson.get("full_name", "").asString();
        bt.alias = btJson.get("alias", "").asString();
        bt.isVirtual = btJson.get("virtual", false).asBool();
        bt.schema = btJson.get("schema", "").asString();
        
        // Mismatch between table name in plan and in queries
        bt.name = bt.name == "dim_date" ? "dates" : bt.name;
        
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

BaseType::Estimates PlanNode::parseEstimates(const Json::Value& json) {
    BaseType::Estimates est;
    est.cardinality = json.get("cardinality", 0.0f).asFloat();
    est.cost = json.get("cost", 0.0f).asFloat();
    return est;
}

BaseType::Measures PlanNode::parseMeasures(const Json::Value& json) {
    BaseType::Measures mes;
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
    BaseType::JsonRawData data;
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

