#pragma once

#include <ir/plan_node.hpp>
#include <ir/abstract_ir.hpp>
#include <ir/parse_query.hpp>
#include <ir/base_types.hpp>

PlanNode::AbstractData convertToAbstract(const BaseType::JsonRawData& rawJson);

std::unique_ptr<PlanNode> pruneTree(std::unique_ptr<PlanNode> node);

std::unique_ptr<PlanNode> enrichTree(std::unique_ptr<PlanNode> root, SqlQueryData& queryData);
