#pragma once

#include <ir/plan_node.hpp>
#include <ir/abstract_ir.hpp>
#include <ir/parse_query.hpp>
#include <ir/base_types.hpp>
#include <parser/generate_AST.h>

PlanNode::AbstractData convertToAbstract(const BaseType::JsonRawData& rawJson);

std::shared_ptr<PlanNode> pruneTree(std::shared_ptr<PlanNode> node);

std::shared_ptr<PlanNode> enrichTree(std::shared_ptr<PlanNode> root, SqlQueryData& queryData);

std::shared_ptr<PlanNode> astToIr(ASTNode* ast);

struct MaterializationData {
    std::set<BaseType::Table> tablesBelow;
    std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> previousMaterializations;
    MaterializationData(
        std::set<BaseType::Table> tablesBelow, 
        std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>>previousMaterializations
    ) :
        tablesBelow(tablesBelow),
        previousMaterializations(previousMaterializations)
    {}
    MaterializationData() = default;
};

MaterializationData fillMaterializes(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn = {});

void irToApiData(PlanNode* node);