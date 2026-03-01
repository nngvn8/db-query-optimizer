#include "ir/ir_transformer.hpp"
#include "ir/transform_helpers.hpp"
#include "ir/ir_views.hpp"
#include <iostream>
#include <deque>
#include <unordered_set>

void ensureCorrectColumnSetup(std::shared_ptr<PlanNode> node, std::map<std::string, std::string> aliasToName, std::map<std::string, std::string> nameToAlias) {
    if (!node) return;

    // Collect aliases from current node 
    if (auto selectV = node->irData.get_view_if<SelectView>()) {
        for (const auto& col : selectV->resultCols()) {
            if (!col.alias.value_or("").empty()) {
                std::string alias = *col.alias;
                aliasToName[alias] = col.columnName;
                nameToAlias[col.columnName] = alias;
            }
        }
    }

    // Helper to apply aliases to a column
    auto applyAlias = [&](BaseType::TableColumn& col) {
        // If column name is an alias, resolve it
        if (aliasToName.contains(col.columnName)) {
            std::cout << "Alias found in: " << col.columnName << std::endl;
            col.alias = col.columnName;
            col.columnName = aliasToName[col.columnName];
            std::cout << "New Column Name: " << col.columnName << " with Alias: " << col.alias.value() << std::endl << std::endl;
        }
        // If column name has a known alias, set it
        if (nameToAlias.contains(col.columnName)) {
            std::string prevAlias = col.alias.value_or("");
            col.alias = nameToAlias[col.columnName];
            if (prevAlias != col.alias.value())
                std::cout << "Changed alias from " << (prevAlias.empty() ? "empty" : prevAlias) << " to " << col.alias.value() << std::endl << std::endl;
        }

        // Fix Table Names for Default/Empty tables if content looks like operation
        bool isDefaultTable = col.table.name.empty() || col.table.name == "Default";
        if (isDefaultTable) {
            std::cout << "Default Table found in: " << col.columnName << std::endl;
            if (IrTransformHelpers::mapStringToAggFunc(col.columnName)) {
                col.table.name = "AGG";
            } else if (col.columnName.find_first_of("+-*/%") != std::string::npos) {
                col.table.name = "MAP";
            }
            std::cout << "New Table Name: " << col.table.name << std::endl << std::endl;
        }
    };

    // Ensure correct table names (comment in if needed but should be fine without it)
    // if (auto aggV = node->irData.get_view_if<AggView>()) {
    //     aggV->aggResultCol().table.name = "AGG";
    // }
    // else if (auto mapV = node->irData.get_view_if<MapView>()) {
    //     mapV->outputCol().table.name = "MAP";

    // }
    // else if (auto sortV = node->irData.get_view_if<SortOrderView>()) {
    //     if (!node->irData.outputCols.empty()) node->irData.outputCols[0].table.name = "SORT";

    // }
    // else if (auto groupV = node->irData.get_view_if<GroupView>()) {
    //     if (!node->irData.outputCols.empty()) node->irData.outputCols[0].table.name = "GROUP";
    // }

    // Ensure correct alias setup
    for (auto& col : node->irData.inputColumns) applyAlias(col);
    for (auto& col : node->irData.outputCols) {
        applyAlias(col);
    }

    // ### Recurse
    for (auto& child : node->children) {
        // If child is a new Select scope (Subquery), do not propagate aliases from this scope
        if (child->irData.get_view_if<SelectView>()) {
            ensureCorrectColumnSetup(child, {}, {});
        } else {
            ensureCorrectColumnSetup(child, aliasToName, nameToAlias);
        }
    }
}

namespace {

    int detFilterColIdx(const BaseType::TableColumn& idxCol, const std::shared_ptr<PlanNode>& node) {

        // Is not join
        if (!node->irData.is<JoinOp>())
            return 0;

        // Is join and has column as output
        if (node->irData.outputCols[0].table == idxCol.table)
            return 0;
        else if (node->irData.outputCols[1].table == idxCol.table)
            return 1;

        // BFS for usage of table that our column is based on
        int idx; // to determine innerCol or outerCol - is inherited downwards
        PlanNode* cur_node;
        std::deque<std::pair<int, PlanNode*>> bfsQueue;
        std::unordered_set<const PlanNode*> visited;


        bfsQueue.push_back({0, node->children[0].get()});
        bfsQueue.push_back({1, node->children[1].get()});

        while (!bfsQueue.empty()) {
            idx = bfsQueue.front().first;
            cur_node = bfsQueue.front().second;
            bfsQueue.pop_front();

            if (visited.count(cur_node)) continue;
            visited.insert(cur_node);

            // Need to find the the table of our column further down the tree
            for (const auto& outCol : cur_node->irData.outputCols) {
                if (outCol.table == idxCol.table) {
                    return idx;
                }
            }

            // bfs
            for (const auto& child : cur_node->children) {
                bfsQueue.push_back({idx, child.get()});
            }
        }
        return 0;
    }

}
// Puts Materializes everywhere and also populates the columns to fetch in TableBaseNode
MaterializationData fillMaterializes(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn, const std::set<BaseType::TableColumn>& inputOfParent) {
    if (!node) return MaterializationData();

    // Previous materializations available (collected from children, possibly updated here)
    std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> pMat;

    // Tables below this node (union of tables found below all children)
    std::set<BaseType::Table> allTablesBelow;

     // Columns this node needs
    std::set<BaseType::TableColumn> columnsThisNode(node->irData.inputColumns.begin(), node->irData.inputColumns.end());

    // Add columns needed by this node to columns needed later
    columnsToMaterializeOn.insert(columnsThisNode.begin(), columnsThisNode.end());

    // Generate materialize nodes (bottom up)
    for (const auto& child : node->children) {

        // ##### RECURSION HERE ######
        MaterializationData matData = fillMaterializes(child.get(), columnsToMaterializeOn, columnsThisNode);
        std::set<BaseType::Table> tablesBelowChild = matData.tablesBelow;
        pMat.merge(matData.previousMaterializations);

        BaseType::TableColumn filterCol;

        // Create or update materialization if child outputs position list
        if (child->irData.outputsPosList) {

            for (const auto& idxCol : columnsToMaterializeOn) {

                // Add a materialization to the materialization node if table below
                if (tablesBelowChild.contains(idxCol.table)) {

                    int idx = detFilterColIdx(idxCol, child);
                    filterCol = child->irData.outputCols[idx]; // except for Select/Result all nodes at the moment only have one output column

                    // Create materialization node
                    std::shared_ptr<PlanNode> matNode = std::make_shared<PlanNode>();
                    matNode->irData = MaterializeView::create(idxCol, filterCol, idxCol);

                    // Left child is Source/Data (Materialization or Fetch)
                    if (auto& mat = pMat[idxCol]){
                        // Use previous materialization if exists
                        matNode->children.push_back(mat);
                    }
                    else {
                        // Create a Fetch Node
                        std::shared_ptr<PlanNode>fetchNode = std::make_shared<PlanNode>();
                        fetchNode->irData = FetchView::create(idxCol);
                        matNode->children.push_back(fetchNode);
                    }

                    // Right child Filter
                    matNode->children.push_back(child);

                    // Set this materialization as the most recent one
                    pMat[idxCol] = matNode;
                }
            }
        }
        if (child->irData.outputsMatVals) {
            BaseType::TableColumn outCol;
            if (auto groupV = child->irData.get_view_if<GroupView>()) {
                if (groupV->aggResultCol()) {
                    outCol = *groupV->aggResultCol();
                }
            }
            else {
                outCol = child->irData.outputCols[0];
            }
            pMat[outCol] = child;
        }

        allTablesBelow.merge(tablesBelowChild);

    }

    // The node is a leafnode (a table node)
    if (auto fetchV = node->irData.get_view_if<FetchView>()) {
        if (fetchV->wasTableBaseNode()) {
            // Set column names (TableBaseNode did not have any)
            for (auto& fetchCol : inputOfParent) {
                if (fetchCol.table.name == fetchV->inputCol().table.name) {
                    fetchV->inputCol().columnName = fetchCol.columnName;
                    fetchV->outputCol().columnName = fetchCol.columnName;
                    pMat[fetchCol] = std::make_shared<PlanNode>(*node);
                }
            }
            // Add the table of this node to tables below
            allTablesBelow.insert(fetchV->inputCol().table);
        }
    }
    // Rewire children if not leaf node
    else {
        node->children = {};
        for (auto& idxCol : node->irData.inputColumns) {
            // Set most recent materialization of children as column
            if (pMat.count(idxCol))
                node->children.push_back(pMat[idxCol]);
            // This should not happen (currently ocuring because no proper map nodes)
            else {
                std::cout << "scream" << std::endl;
            }
        }
    }

    return MaterializationData{allTablesBelow, pMat};

}
