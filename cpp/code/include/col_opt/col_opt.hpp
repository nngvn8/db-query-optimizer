#include <memory>

#include "ir/plan_node.hpp"

void placeSemiJoins(PlanNode* node, std::set<BaseType::Table> tablesNeededLater = {});

// TODO: dont need MaterializationData here (just used for recursive calls). Probably write wrapper for fillMaterializes
struct LateMaterializationData {
    std::set<BaseType::Table> tablesBelow;
    std::map<BaseType::Table, std::shared_ptr<PlanNode>> previousPositionlists;
    std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> previousMaterialValues;

};

// Moves single sum aggregations into group, return value only used internally
int moveAggIntoGroup(PlanNode* node);

// Remove sort if subset group
void removeSortIfSubsetGroup(std::shared_ptr<PlanNode>* node);

LateMaterializationData putLateMaterialization(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn = {}, const std::set<BaseType::TableColumn>& inputOfParent = {});

LateMaterializationData putLateMaterializationV2(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn = {}, const std::vector<BaseType::TableColumn>& inputOfParent = {});

LateMaterializationData putLateMaterializationHybrid(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn = {}, const std::vector<BaseType::TableColumn>& inputOfParent = {});

void grandChildrenOptimization(PlanNode* node);