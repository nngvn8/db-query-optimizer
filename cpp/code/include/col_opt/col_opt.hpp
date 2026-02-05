#include <memory>

#include "ir/plan_node.hpp"

void placeSemiJoins(PlanNode* node, std::set<BaseType::Table> tablesNeededLater);

// TODO: dont need MaterializationData here (just used for recursive calls). Probably write wrapper for fillMaterializes
struct LateMaterializationData {
    std::set<BaseType::Table> tablesBelow;
    std::map<BaseType::Table, std::shared_ptr<PlanNode>> previousPositionlists;
};

LateMaterializationData putLateMaterialization(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn = {}, const std::set<BaseType::TableColumn>& inputOfParent = {});