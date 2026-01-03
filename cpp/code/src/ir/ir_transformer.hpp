#pragma once

#include <ir/plan_node.hpp>
#include <ir/abstract_ir.hpp>
#include <ir/parse_query.hpp>
#include <ir/base_types.hpp>
#include <parser/generate_AST.h>

PlanNode::AbstractData convertToAbstract(const BaseType::JsonRawData& rawJson);

std::unique_ptr<PlanNode> pruneTree(std::unique_ptr<PlanNode> node);

std::unique_ptr<PlanNode> enrichTree(std::unique_ptr<PlanNode> root, SqlQueryData& queryData);

std::unique_ptr<PlanNode> astToIr(ASTNode* ast);

void fillMaterializes(PlanNode* node, std::set<BaseType::TableColumn> materializedColumns = {});