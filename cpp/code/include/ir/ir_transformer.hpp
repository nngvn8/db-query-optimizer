/**
 * @file ir_transformer.hpp
 * @brief Declares functions and classes for transforming the query plan IR.
 *
 * This file provides the interface for various transformations on the IR,
 * such as converting from different representations (JSON, AST), pruning the tree,
 * enriching it with more information, and preparing it for code generation.
 */

#pragma once

#include "ir/plan_node.hpp"
#include "ir/abstract_ir.hpp"
#include "ir/get_query_info.hpp"
#include "ir/base_types.hpp"
#include "parser/generate_AST.hpp"
#include <map>

/**
 * @brief Converts raw JSON data into an abstract data representation for a plan node.
 * @param rawJson The raw JSON data.
 * @return The abstract data representation.
 */
PlanNode::AbstractData convertToAbstract(const BaseType::JsonRawData& rawJson);

/**
 * @brief Prunes unnecessary nodes from the query plan tree.
 * @param node The root of the query plan tree.
 * @return The pruned query plan tree.
 */
std::shared_ptr<PlanNode> pruneTree(std::shared_ptr<PlanNode> node);

/**
 * @brief Enriches the query plan tree with additional information from the SQL query data.
 * @param root The root of the query plan tree.
 * @param queryData The SQL query data.
 * @return The enriched query plan tree.
 */
std::shared_ptr<PlanNode> enrichTree(std::shared_ptr<PlanNode> root, SqlQueryData& queryData);

/**
 * @brief Converts an Abstract Syntax Tree (AST) to the query plan IR.
 * This version uses a refined parsing strategy.
 * @param ast The root of the AST.
 * @return The corresponding query plan IR.
 */
// Update OR to be parsed into set operations
// Use refined parsing of optimizer one
std::shared_ptr<PlanNode> astToIr(ASTNode* ast);

/**
 * @class AbstractToIr
 * @brief A class for converting the abstract IR to the full query plan IR.
 */
class AbstractToIr {
public:
    /**
     * @brief Converts an abstract IR node to a full query plan IR node.
     * @param node The abstract IR node.
     * @return The full query plan IR node.
     */
    static std::shared_ptr<PlanNode> abstractToIr(std::shared_ptr<PlanNode> node);
};

/**
 * @brief Ensures that the columns in the query plan are set up correctly, handling aliases.
 * @param node The root of the query plan.
 * @param aliasToName A map from aliases to column names.
 * @param nameToAlias A map from column names to aliases.
 */
void ensureCorrectColumnSetup(std::shared_ptr<PlanNode> node,
                              std::map<std::string, std::string> aliasToName = {},
                              std::map<std::string, std::string> nameToAlias = {});

/**
 * @struct MaterializationData
 * @brief A struct to hold data for the materialization process.
 */
// TODO: dont need MaterializationData here (just used for recursive calls). Probably write wrapper for fillMaterializes
struct MaterializationData {
    /// The set of tables below the current node.
    std::set<BaseType::Table> tablesBelow;
    /// A map from table columns to their previous materializations.
    std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> previousMaterializations;
};

/**
 * @brief Fills in the materialization information in the query plan.
 * @param node The root of the query plan.
 * @param columnsToMaterializeOn The set of columns to materialize on.
 * @param inputOfParent The set of input columns of the parent node.
 * @return A MaterializationData struct.
 */
MaterializationData fillMaterializes(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn = {}, const std::set<BaseType::TableColumn>& inputOfParent = {});

/**
 * @brief Converts the query plan IR to the API data format.
 * @param node The root of the query plan.
 */
void irToApiData(PlanNode* node);
