#pragma once

#include <translation/plan_node.hpp>
#include <translation/abstract_ir.hpp>
#include <translation/parse_query.hpp>

PlanNode::AbstractData convertToAbstract(const JsonRawData& rawJson);

std::unique_ptr<PlanNode> pruneTree(std::unique_ptr<PlanNode> node);

std::set<std::string> enrichTree(PlanNode* node, QueryMetadata& meta);