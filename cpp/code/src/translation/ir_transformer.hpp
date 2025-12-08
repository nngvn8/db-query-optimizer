#include <translation/plan_node.hpp>
#include <translation/abstract_ir.hpp>


PlanNode::AbstractData convertToAbstract(const JsonRawData& rawJson);

std::unique_ptr<PlanNode> pruneTree(std::unique_ptr<PlanNode> node);