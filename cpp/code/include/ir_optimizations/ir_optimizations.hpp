/**
 * @file ir_optimizations.hpp
 * @brief Declares various optimization functions that can be applied to the Intermediate Representation (IR) of a query plan.
 *
 * These optimizations aim to improve query performance by restructuring the query plan.
 * Examples include placing semi-joins, late materialization, and merging operations.
 */

#include <memory>
#include "ir/plan_node.hpp"

/**
 * @brief Places semi-joins in the query plan.
 *
 * This optimization can reduce the amount of data processed by filtering rows early.
 *
 * @param node The root of the query plan to optimize.
 * @param tablesNeededLater A set of tables that will be needed later in the plan.
 */
void placeSemiJoins(PlanNode* node, std::set<BaseType::Table> tablesNeededLater = {});

/**
 * @struct LateMaterializationData
 * @brief A struct to hold data for the late materialization optimization.
 *
 * Late materialization is a technique where the full tuples are materialized as late as possible.
 * This can save a significant amount of I/O and memory, especially for queries with high selectivity.
 */
// TODO: dont need MaterializationData here (just used for recursive calls). Probably write wrapper for fillMaterializes
struct LateMaterializationData {
    /// The set of tables below the current node.
    std::set<BaseType::Table> tablesBelow;
    /// A map from tables to the previously encountered position lists.
    std::map<BaseType::Table, std::shared_ptr<PlanNode>> previousPositionlists;
    /// A map from table columns to the previously materialized values.
    std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> previousMaterialValues;
};

/**
 * @brief Moves single sum aggregations into a group-by operation.
 * @param node The root of the query plan to optimize.
 * @return An integer used for internal recursive calls.
 */
int moveAggIntoGroup(PlanNode* node);

/**
 * @brief Merges a sort operation into a group-by operation if the sort is on a subset of the grouping columns.
 * @param node A pointer to a shared pointer to the root of the query plan.
 */
void mergeSortIntoGroupIfSubset(std::shared_ptr<PlanNode>* node);

/**
 * @brief Applies the late materialization optimization to the query plan.
 * @param node The root of the query plan to optimize.
 * @param columnsToMaterializeOn A set of columns to materialize on.
 * @param inputOfParent A set of columns that are input to the parent node.
 * @return A LateMaterializationData struct containing data for further optimizations.
 */
LateMaterializationData putLateMaterialization(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn = {}, const std::set<BaseType::TableColumn>& inputOfParent = {});

/**
 * @brief An alternative version of the late materialization optimization.
 * @param node The root of the query plan to optimize.
 * @param columnsToMaterializeOn A set of columns to materialize on.
 * @param inputOfParent A vector of columns that are input to the parent node.
 * @return A LateMaterializationData struct containing data for further optimizations.
 */
LateMaterializationData putLateMaterializationV2(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn = {}, const std::vector<BaseType::TableColumn>& inputOfParent = {});

/**
 * @brief A hybrid version of the late materialization optimization.
 * @param node The root of the query plan to optimize.
 * @param columnsToMaterializeOn A set of columns to materialize on.
 * @param inputOfParent A vector of columns that are input to the parent node.
 * @return A LateMaterializationData struct containing data for further optimizations.
 */
LateMaterializationData putLateMaterializationHybrid(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn = {}, const std::vector<BaseType::TableColumn>& inputOfParent = {});

/**
 * @brief Applies the grandchildren optimization to the query plan.
 * This optimization aims to pull up nodes past their grandparents to enable more optimization opportunites.
 * @param node The root of the query plan to optimize.
 */
void grandChildrenOptimization(PlanNode* node);