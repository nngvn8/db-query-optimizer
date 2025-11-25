#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <fstream>
#include <sequentializer/sequentializer.hpp>

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

    if (!planNode.children.empty()) {
        for (const std::unique_ptr<PlanNode>& child : planNode.children){
            printDebug(*child);
        }
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
    const Json::Value& children = queryPlan["children"];
    if (children) {
        for (const Json::Value& child : children) {
            this->children.push_back(std::make_unique<PlanNode>(child));
        }
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

void printNode(const PlanNode& node){
    std::cout << node.nodeType;
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
    std::ifstream queryJson("/home/martin/University/09_KDB/ws25-optimizer-rust/pb-plans/q1-1-plan.json");
    Json::Value queryPlan;
    queryJson >> queryPlan;

    PlanNode* planNode = new PlanNode(queryPlan);
    // printDebug(*planNode);
    printPlanTree(*planNode, "", true);

    std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(planNode);
    printSequencedPlan(sequenced_plan);

    delete planNode;

    return 0;
}
