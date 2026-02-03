#include <memory>

#include "ir/plan_node.hpp"

void placeSemiJoins(PlanNode* node, std::set<BaseType::Table> tablesNeededLater);