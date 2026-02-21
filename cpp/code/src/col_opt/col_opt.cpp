#include "col_opt/col_opt.hpp"

#include "ir/ir_views.hpp"
#include "ir/catalog.hpp"

void placeSemiJoins(PlanNode* node, std::set<BaseType::Table> tablesNeededLater) {
    if (!node) return;

    // Tables this node needs (input and output columns are the same, doesn't matter which one we traverse)
    std::set<BaseType::Table> tablesThisNode;
    for (const BaseType::TableColumn& col : node->irData.inputColumns) {
        tablesThisNode.insert(col.table);
    }

    // If encountering join
    if (auto joinV = node->irData.get_view_if<JoinView>()) {
        bool innerTableNeededLater = tablesNeededLater.contains(joinV->inner().table);
        bool outerTableNeededLater = tablesNeededLater.contains(joinV->outer().table);

        // If only one table needed later
        if (!(innerTableNeededLater && outerTableNeededLater)) {

            const BaseType::TableColumn& colToDiscard = innerTableNeededLater ? joinV->outer() : joinV->inner();

            // If column is unique - Continue only if it is unique and therefore only severs as filter and does not multiply any rows
            if (Catalog::isUnique(colToDiscard.table.name, colToDiscard.columnName)) {
                IrData semiJoinData = SemiJoinView::create(node->irData);
                SemiJoinView semiJoinV(semiJoinData);

                // Swap inner and outer, if inner is the one needed later
                if (innerTableNeededLater && !outerTableNeededLater) {
                    std::swap(semiJoinV.inner(), semiJoinV.outer());
                    std::swap(semiJoinV.innerOut(), semiJoinV.outerOut());
                    std::swap(node->irData.outputCols[0], node->irData.outputCols[1]);
                }
                node->irData = semiJoinData;
            }
        }
    }

    // Add tables of this node to tables needed later
    tablesNeededLater.insert(tablesThisNode.begin(), tablesThisNode.end());


    // RECURSE
    for (const auto& child : node->children) {
        placeSemiJoins(child.get(), tablesNeededLater);
    }

}

void removeSortIfSubsetGroup(std::shared_ptr<PlanNode>* node_ptr) {
    if (!node_ptr || !*node_ptr) return;
    std::shared_ptr<PlanNode> node = *node_ptr;

    // If sort with group as child
    if (auto sortV = node->irData.get_view_if<SortOrderView>()) {
        for (const auto& child : node->children) {
            if (auto groupV = child->irData.get_view_if<GroupView>()) {
                // Check if input sort subset input group
                bool isSubset = true;
                std::set<BaseType::TableColumn> groupInputsSet(child->irData.inputColumns.begin(), child->irData.inputColumns.end());
                std::set<BaseType::TableColumn> sortInputsSet(node->irData.inputColumns.begin(), node->irData.inputColumns.end());
                for (const auto& sortInput : sortV->sortCols()) {
                    if (!groupInputsSet.contains(sortInput)) {
                        isSubset = false;
                        break;
                    }
                }
                // Change order of input cols grouping and set sort orders
                if (isSubset) {
                    // Take input cols and sort orders of sort
                    auto newGroupInputs = sortV->sortCols();
                    std::vector<bool> sortOrders = sortV->orderDescriptions() 
                                                    | std::views::transform([](const BaseType::OrderDescription& desc) { return desc.orderType; }) 
                                                    | std::ranges::to<std::vector>();
                    // Add remaining cols from grouping
                    for (const auto& groupInput : groupV->groupingCols()) {
                        if (!sortInputsSet.contains(groupInput)) {
                            newGroupInputs.push_back(groupInput);
                            sortOrders.push_back(true);
                        }
                    }
                    // Update grouping
                    groupV->groupingCols() = newGroupInputs;
                    groupV->sortOrders() = sortOrders;

                    // Delete sort
                    *node_ptr = child;
                }
            }
        }
    }
    // ### RECURSE ###
    for (auto& child : node->children) {
        removeSortIfSubsetGroup(&child);
    }
}

int moveAggIntoGroup(PlanNode* node) {
    if (!node) return 0;

    int aggCount = 0;

    // ### RECURSE ###
    for (const auto& child : node->children) {
        aggCount += moveAggIntoGroup(child.get());
    }

    // Count aggregation
    if (node->irData.is<AggOp>()) {
        ++aggCount;
    }
    // Remove aggregation if it is a single one and it is a sum
    // Assumes aggregation as child of grouping
    else if (node->irData.is<GroupOp>() && aggCount == 1) {
        GroupView groupV(node->irData);

        // Find aggregation children of grouping
        for (auto& child : node->children) {
            if (auto aggV = child->irData.get_view_if<AggView>()) {
                if (aggV->aggFunc() == AggFunc::AGG_SUM) {
                    // Put aggregation info into group
                    groupV.setAgg(aggV->colToAgg(), aggV->aggResultCol());
                    // Delete Aggregation
                    child = child->children[0];
                }
            }
        }
    }
    // Handling subqueries (aggregations scoped to selection above)
    else if (node->irData.is<SelectOp>()) {
        aggCount = 0;
    }

    return aggCount;
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
        std::shared_ptr<PlanNode> cur_node;
        std::deque<std::pair<int, const std::shared_ptr<PlanNode>&>> bfsQueue;


        bfsQueue.push_back(std::pair<int, const std::shared_ptr<PlanNode>&>(0, node->children[0]));
        bfsQueue.push_back(std::pair<int, const std::shared_ptr<PlanNode>&>(1, node->children[1]));

        while (!bfsQueue.empty()) {
            idx = bfsQueue.front().first;
            cur_node = bfsQueue.front().second;
            bfsQueue.pop_front();

            // Need to find the the table of our column further down the tree
            for (const auto& outCol : cur_node->irData.outputCols) {
                if (outCol.table == idxCol.table) {
                    return idx;
                }
            }

            // bfs
            for (const auto& child : cur_node->children) {
                bfsQueue.push_back(std::pair<int, const std::shared_ptr<PlanNode>&>(idx, child));
            }
        }
    }

}

// Puts Materializes everywhere and also populates the columns to fetch in TableBaseNode
LateMaterializationData putLateMaterialization(PlanNode* node, std::set<BaseType::TableColumn> columnsNeededLater, const std::set<BaseType::TableColumn>& inputOfParent) {
    if (!node) return LateMaterializationData();

    // Latest position lists generated/updated at this node. NOTE: At the moment the parent will update a table position list only ONCE for all children
    std::map<BaseType::Table, std::shared_ptr<PlanNode>> curPos;

    // Previous position lists available (collected from children, possibly updated here)
    std::map<BaseType::Table, std::shared_ptr<PlanNode>> pPos; 
    
    // Tables below this node (union of tables found below all children)
    std::set<BaseType::Table> allTablesBelow; 
    
    // Columns this node needs
    std::set<BaseType::TableColumn> columnsThisNode(node->irData.inputColumns.begin(), node->irData.inputColumns.end());
    
    // Add columns needed by this node to columns needed later
    columnsNeededLater.insert(columnsThisNode.begin(), columnsThisNode.end());

    // Container for materialized values provided by all direct children
    std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> matChildren;

    std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> allPrevMat;

    // Generate materialize nodes (bottom up)
    for (const auto& child : node->children) {

        // ##### RECURSION HERE ######
        LateMaterializationData matData = putLateMaterialization(child.get(), columnsNeededLater, columnsThisNode);
        std::set<BaseType::Table> tablesBelowChild = matData.tablesBelow;
        pPos.merge(matData.previousPositionlists);
        allPrevMat.merge(matData.previousMaterialValues);

        BaseType::TableColumn filterCol;
        
        // Create or update materialization if child outputs position list
        if (child->irData.outputsPosList) {

            for (const auto& laterCol : columnsNeededLater) {

                // Update position lists of this table if node if table below
                if (tablesBelowChild.contains(laterCol.table)) {
                    
                    // If there was no position list update by any of the children (TODO several children want to update because same table at several leaves)
                    if (!curPos[laterCol.table]) {
                    
                        // If there is a position previous position list, that this child updates 
                        if (auto& prevPosListNode = pPos[laterCol.table]) {
                            
                            // Find filter col in current child (in case it has multiple outputs)
                            int idx = detFilterColIdx(laterCol, child);
                            filterCol = child->irData.outputCols[idx]; // except for Select/Result all nodes at the moment only have one output column
                            
                            // Find previous position list col
                            auto hasTable = [&laterCol] (BaseType::TableColumn col) { return col.table == laterCol.table; };
                            BaseType::TableColumn prevPosListCol = *(prevPosListNode->irData.outputCols | std::views::filter(hasTable)).begin();
                            
                            std::shared_ptr<PlanNode> matNode = std::make_shared<PlanNode>();
                            matNode->irData = MaterializeView::create(prevPosListCol, filterCol, laterCol);

                            // Left child: Previous position list to be updated as left child (source/columnNeededLater)
                            matNode->children.push_back(prevPosListNode);
                            // PositionList output by child, updating the previous position list
                            matNode->children.push_back(child);

                            // Set this materialization as the most recent one
                            curPos[laterCol.table] = matNode;
                        }
                        else {
                            // Save position list
                            curPos[laterCol.table] = child;
                        }
                    }
                }
            }
        }
        // Remember children of the node that provided value/materialized data
        if (child->irData.outputsMatVals) {
            if (auto groupV = child->irData.get_view_if<GroupView>()) {
                if (groupV->aggResultCol())
                    matChildren[*groupV->aggResultCol()] = child;
            }
            else {
                matChildren[child->irData.outputCols[0]] = child;
            }
        }
        allTablesBelow.merge(tablesBelowChild);
    }

    // Put all new position lists into the global registry
    for (auto const& [table, posListNode] : curPos) {
        pPos[table] = posListNode;
    }

    for (auto const& [col, matNode] : matChildren) {
        allPrevMat[col] = matNode;
    }

    // The node is a leafnode (a table node)
    if (auto fetchV = node->irData.get_view_if<FetchView>()) {
        if (fetchV->wasTableBaseNode()) {
            // Set column names (TableBaseNode did not have any)
            for (auto& fetchCol : inputOfParent) {
                if (fetchCol.table.name == fetchV->inputCol().table.name) {
                    fetchV->inputCol().columnName = fetchCol.columnName;
                    fetchV->outputCol().columnName = fetchCol.columnName;                    
                }
            }
            // Add the table of this node to tables below
            allTablesBelow.insert(fetchV->inputCol().table);
        }
    }
    // Rewire children if not leaf node
    else {
        node->children = {};
        
        // Provide all materializations needed
        for (auto& idxCol : node->irData.inputColumns) {
            // Set most recent materialization of children as column
            if (const auto& matChild = matChildren[idxCol])
                node->children.push_back(matChild);
            
            // Check if there is a position list we materialized on 
            else if (const auto& latestPosList = pPos[idxCol.table]) {
                // Fetch Column needed
                std::shared_ptr<PlanNode> fetchNode = std::make_shared<PlanNode>();
                fetchNode->irData = FetchView::create(idxCol, false);
                
                // Materialize latest position list on it
                std::shared_ptr<PlanNode> matNode = std::make_shared<PlanNode>();
                matNode->irData = MaterializeView::create(idxCol, latestPosList->irData.outputCols[0], idxCol);
                matNode->children.push_back(fetchNode);
                matNode->children.push_back(latestPosList);
                
                // Make this input for child
                node->children.push_back(matNode);
            } 
            // TODO not clean! Possibly breaks. This has been introduced to input aggregations from earlier
            // Case where we need a previous Materialization
            else if (const auto& prevMat = allPrevMat[idxCol]) {
                node->children.push_back(prevMat);
            }
            // This should not happen
            else {
                std::cout << "scream" << std::endl;
            }
        }
    }

    return LateMaterializationData{allTablesBelow, pPos, allPrevMat};

}


LateMaterializationData putLateMaterializationHybrid(PlanNode* node, std::set<BaseType::TableColumn> columnsNeededLater, const std::vector<BaseType::TableColumn>& inputOfParent) {
    if (!node) return LateMaterializationData();
    
    // Tables below this node (union of tables found below all children)
    std::set<BaseType::Table> allTablesBelow; 

    // Add columns needed by this node to columns needed later
    columnsNeededLater.insert(node->irData.inputColumns.begin(), node->irData.inputColumns.end());

    // Container for materialized values provided by all direct children
    std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> curMat;

    // Latest position lists generated/updated at this node. NOTE: At the moment the parent will update a table position list only ONCE for all children
    std::map<BaseType::Table, std::shared_ptr<PlanNode>> curPos;

    // Generate materialize nodes (bottom up)
    for (const auto& child : node->children) {

        // ##### RECURSION HERE ######
        LateMaterializationData matData = putLateMaterializationHybrid(child.get(), columnsNeededLater, node->irData.inputColumns);
        std::set<BaseType::Table> tablesBelowChild = matData.tablesBelow;
        
        // REMOVING MERGES REMOVES CRASHES???
        // Previous position lists available (collected from children, possibly updated here)
        std::map<BaseType::Table, std::shared_ptr<PlanNode>> pPos = matData.previousPositionlists;
        std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> prevMat = matData.previousMaterialValues;

        BaseType::TableColumn filterCol;

        // Still maintain pos list and mats if not affected by this node 
        // (There are nodes that output both matVals and posList)
        if (!child->irData.outputsPosList) {
            curPos.merge(pPos);
            prevMat.merge(prevMat);
        }
        
        // Create or update materialization if child outputs position list
        if (child->irData.outputsPosList) {

            // Only update columns needed later
            for (const auto& laterCol : columnsNeededLater) {

                // Possibly update previous materialization, if
                // - column is result of map or aggregation or,
                // - only column of table
                bool isAggOrMapMatData = laterCol.table.name == "AGG" || laterCol.table.name == "MAP";
                bool isOnlyColumnOfTableNeededLater = std::ranges::count(columnsNeededLater, laterCol.table, &BaseType::TableColumn::table) == 1;
                if (prevMat[laterCol] && (isAggOrMapMatData || isOnlyColumnOfTableNeededLater)) {
                                            
                    // Find filter col in current child (in case it has multiple outputs)
                    int idx = detFilterColIdx(laterCol, child);
                    filterCol = child->irData.outputCols[idx];
                    
                    // Create Materialize Node updating materialization
                    std::shared_ptr<PlanNode> matNode = std::make_shared<PlanNode>();
                    bool matNodeOutputsPosList = false;
                    matNode->irData = MaterializeView::create(laterCol, filterCol, laterCol, matNodeOutputsPosList);
                    
                    // Left child: Previous Materialization to be updated as left child (source/columnNeededLater)
                    matNode->children.push_back(prevMat[laterCol]);
                    // PositionList output by child, updating the previous position list
                    matNode->children.push_back(child);
                    
                    curMat[laterCol] = matNode;
                }

                // Update position lists of this table if table below
                else if (tablesBelowChild.contains(laterCol.table)) {

                    // Update postion list if there was no position list update by any of the children yet (TODO several children want to update because same table at several leaves)
                    if (!curPos[laterCol.table]) {

                        // If there is an existing position list, that this child updates 
                        if (auto& prevPosListNode = pPos[laterCol.table]) {

                            // Find filter col in current child (in case it has multiple outputs)
                            int idx = detFilterColIdx(laterCol, child);
                            filterCol = child->irData.outputCols[idx]; 
                            
                            // Find previous position list col
                            auto hasTable = [&laterCol] (BaseType::TableColumn col) { return col.table == laterCol.table; };
                            BaseType::TableColumn prevPosListCol = *(prevPosListNode->irData.outputCols | std::views::filter(hasTable)).begin();
                            
                            // Create Materialize Node
                            std::shared_ptr<PlanNode> matNode = std::make_shared<PlanNode>();
                            bool matNodeOutputsPosList = true;
                            matNode->irData = MaterializeView::create(prevPosListCol, filterCol, laterCol, matNodeOutputsPosList);

                            // Left child: Previous position list to be updated as left child (source/columnNeededLater)
                            matNode->children.push_back(prevPosListNode);
                            // PositionList output by child, updating the previous position list
                            matNode->children.push_back(child);

                            // Set this materialization as the most recent one
                            curPos[laterCol.table] = matNode;
                        }

                        // If this is the first position list on that table
                        else {
                            // Save position list
                            curPos[laterCol.table] = child;
                        }
                    }
                }
            }
        }
        // Remember children of the node that provided value/materialized data
        if (child->irData.outputsMatVals) {            
            if (auto groupV = child->irData.get_view_if<GroupView>()) {
                if (groupV->aggResultCol())
                    curMat[*groupV->aggResultCol()] = child;
            }
            else {
                curMat[child->irData.outputCols[0]] = child;
            }
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
                }
            }
            // Add the table of this node to tables below
            allTablesBelow.insert(fetchV->inputCol().table);
        }
    }
    // Rewire children if not leaf node
    else {
        node->children = {};
        
        // Provide all materializations needed
        for (auto& idxCol : node->irData.inputColumns) {
            // Set most recent materialization of children as column
            if (const auto& matChild = curMat[idxCol])
                node->children.push_back(matChild);
            
            // Check if there is a position list we materialized on 
            else if (const auto& latestPosList = curPos[idxCol.table]) {
                // Fetch Column needed
                std::shared_ptr<PlanNode> fetchNode = std::make_shared<PlanNode>();
                fetchNode->irData = FetchView::create(idxCol, false);
                
                // Materialize latest position list on it
                std::shared_ptr<PlanNode> matNode = std::make_shared<PlanNode>();
                bool matNodeOutputsPosList = false;
                matNode->irData = MaterializeView::create(idxCol, latestPosList->irData.outputCols[0], idxCol, matNodeOutputsPosList);
                matNode->children.push_back(fetchNode);
                matNode->children.push_back(latestPosList);
                
                // Make this input for child
                node->children.push_back(matNode);

                // Register as Materialization
                curMat[idxCol] = matNode;
            } 
            // This should not happen
            else {
                std::cout << "scream " << std::endl;
            }
        }
    }
    return LateMaterializationData{allTablesBelow, curPos, curMat};
}
