#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <fstream>

#include "plan_node.h"

void printDebug(const PlanNode& planNode) {
    std::cout << "Node: " << planNode.nodeType << " " << planNode.nodeOperator.get() << std::endl;

    const PlanParams& params = planNode.planParams;
    std::cout << "Plan Params: ";
    if (params.baseTable) {
        const BaseTable* bt = params.baseTable.get();
        std::cout << "BaseTable: " << bt->fullName << " " << bt->alias << " " << bt->isVirtual << " " << bt->schema << " ";
    }
    std::cout << params.filterPredicate << " " << params.sortKeys.get() << " " << params.parallelWorkers
        << " " << params.index << " " << params.lookupKey << " " << params.suplanName << std::endl;

    const Estimates& est = planNode.estimates;
    std::cout << "Estimates: " << est.cardinality << " " << est.cost << std::endl;

    const Measures& mes = planNode.measures;
    std::cout << "Measures: " << mes.cardinality << " " << mes.executionTime << " " << mes.cacheHits << " " << mes.cacheMisses << std::endl;

    if (planNode.next) {
        printDebug(*(planNode.next));
    }
}

void PlanNode::setBaseTable(std::unique_ptr<BaseTable>& baseTable, const Json::Value& jsonData) {
    if (!jsonData) {
        baseTable = nullptr;
        return;
    }

    if (!baseTable) {
        baseTable = std::make_unique<BaseTable>();
    }

    baseTable->fullName = jsonData["full_name"].asString();
    baseTable->alias = jsonData["alias"].asString();
    baseTable->isVirtual = jsonData["virtual"].asBool();
    baseTable->schema = jsonData["schema"].asString();
}

void PlanNode::setPlanParams(PlanParams& planParams, const Json::Value& jsonData) {
    setBaseTable(planParams.baseTable, jsonData["base_table"]);

    planParams.filterPredicate = jsonData["filter_predicate"].asString();
    planParams.parallelWorkers = jsonData["parallel_workers"].asInt();
    planParams.index = jsonData["index"].asString();

    if (!planParams.sortKeys && jsonData["sort_keys"]) {
        planParams.sortKeys = std::make_unique<std::vector<std::string>>();
        planParams.sortKeys = nullptr; // TODO jsonData["sort_keys"];
    } else {
        planParams.sortKeys = nullptr;
    }
}

void PlanNode::setEstimates(Estimates& estimates, const Json::Value& jsonData) {
    estimates.cardinality = jsonData["cardinality"].asFloat();
    estimates.cost = jsonData["cost"].asFloat();
}

void PlanNode::setMeasures(Measures& measures, const Json::Value& jsonData) {
    measures.cardinality = jsonData["cardinality"].asFloat();
    measures.executionTime = jsonData["execution_time"].asFloat();
    measures.cacheHits = jsonData["cache_hits"].asInt();
    measures.cacheMisses = jsonData["cache_misses"].asInt();
}

PlanNode::PlanNode(const Json::Value& queryPlan) {
    if (queryPlan["children"]) {
        next = std::make_unique<PlanNode>(queryPlan["children"][0]);
    }

    nodeType = queryPlan["node_type"].asString();
    if (queryPlan["operator"]) {
        nodeOperator = std::make_unique<std::string>(queryPlan["operator"].asString());
    } else {
        nodeOperator = nullptr;
    }

    setPlanParams(planParams, queryPlan["plan_params"]);
    setEstimates(estimates, queryPlan["estimates"]);
    setMeasures(measures, queryPlan["measures"]);
}

int main(int argc, char* argv[]) {
    std::ifstream queryJson("q1-1-plan.json", std::ifstream::binary);
    Json::Value queryPlan;
    queryJson >> queryPlan;

    PlanNode* planNode = new PlanNode(queryPlan);

    printDebug(*planNode);
    delete planNode;

    return 0;
}
