// #include "ir_transformer.hpp"

// #include <memory>
// #include <unordered_set>
// #include <regex>

// #include <ir/catalog.hpp>
// #include <ir/ir_types.hpp>
// #include <ir/ir_views.hpp>

// MaterializationData fillMaterializes(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn) {
//     if (!node) return MaterializationData();

//     std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> pMat; // previous materializations
    
//     // Add inputs of this node to the set of columns to materialize
//     columnsToMaterializeOn.insert(node->irData.inputColumns.begin(), node->irData.inputColumns.end());

//     std::set<BaseType::Table> allTablesBelow;

//     // Fetch columns this node explicitly needs
//     std::set<BaseType::TableColumn> columnsThisNode(node->irData.inputColumns.begin(), node->irData.inputColumns.end());

//     // ---------------------------------------------------------
//     // PHASE 1: Recurse children and build Materialization Nodes
//     // ---------------------------------------------------------
//     for (size_t i = 0; i < node->children.size(); ++i) {
        
//         // 1. Capture Original Child before recursion or modification
//         std::shared_ptr<PlanNode> originalChild = node->children[i];

//         // 2. Recurse
//         MaterializationData matData = fillMaterializes(originalChild.get(), columnsToMaterializeOn);
//         std::set<BaseType::Table> tablesBelowChild = matData.tablesBelow;
//         pMat.merge(matData.previousMaterializations); // Merge results from below

//         // 3. Determine Child Type
//         BaseType::TableColumn filterCol;
//         bool childIsPositionListNode = false;
//         bool childIsFetchNode = false;

//         // Check for Filter/PosList provider
//         if (originalChild->irData.is<JoinOp>()
//             || originalChild->irData.is<FilterOp>()
//             || originalChild->irData.is<GroupOp>()
//             || originalChild->irData.is<SortOp>()
//             || originalChild->irData.is<SetOp>()) {
//                 filterCol = originalChild->irData.outputCols[0];
//                 childIsPositionListNode = true;
//         } else {
//             // Check for Fetch/TableBase
//             if (originalChild->irData.is<FetchOp>()) {
//                 childIsFetchNode = true;
//             }
//         }

//         // 4. Generate Materializations for this specific child
//         if (childIsPositionListNode || childIsFetchNode) {
            
//             for (const auto& idxCol : columnsToMaterializeOn) {
                
//                 // A: Handle TableBaseNode (Fetch) upgrade
//                 // If the child is a FetchNode (Table), we don't wrap it in a MatNode immediately.
//                 // We just ensure the FetchNode knows it needs to fetch this column.
//                 // Note: Actual column population happens in the specific block at the end of function.
//                 if (childIsFetchNode && tablesBelowChild.contains(BaseType::Table(idxCol.table.name))) {
//                     // Logic handled in "The node is a leafnode" block below/or pre-inserted
//                     // We just need to ensure pMat points to a Fetch for this column if it doesn't exist
                    
//                     // Note: You originally created a NEW FetchNode here.
//                     // Better approach: If pMat doesn't have it, create the root Fetch here.
//                     if (pMat.find(idxCol) == pMat.end()) {
//                          std::shared_ptr<PlanNode> fetchNode = std::make_shared<PlanNode>();
//                          fetchNode->irData = FetchView::create(idxCol, true); // Create real fetch
//                          pMat[idxCol] = fetchNode;
//                     }
//                     continue; 
//                 }

//                 // B: Handle Materialization Insertion
//                 if (tablesBelowChild.contains(BaseType::Table(idxCol.table.name))) {
                    
//                     // Create New Materialization Node
//                     std::shared_ptr<PlanNode> matNode = std::make_shared<PlanNode>();
//                     matNode->irData = MaterializeView::create(idxCol, filterCol, idxCol);

//                     // Child 1 (Left): Data Source (Previous Mat or New Fetch)
//                     if (auto& mat = pMat[idxCol]) {
//                         matNode->children.push_back(mat);
//                     } else {
//                         // Start of chain: New Fetch
//                         std::shared_ptr<PlanNode> fetchNode = std::make_shared<PlanNode>();
//                         fetchNode->irData = FetchView::create(idxCol);
//                         matNode->children.push_back(fetchNode);
//                     }

//                     // Child 2 (Right): Filter Source
//                     // CRITICAL FIX: Always use originalChild, not node->children[i]
//                     matNode->children.push_back(originalChild);

//                     // Update Map: This Mat node is now the latest provider for idxCol
//                     pMat[idxCol] = matNode;
//                 }
//             }
//         }
//         allTablesBelow.merge(tablesBelowChild);
//     }

//     // ---------------------------------------------------------
//     // SPECIAL: Leaf Node (TableBaseNode) Column Population
//     // ---------------------------------------------------------
//     // Fixes the Segfault by clearing the dummy column
//     if (auto fetchV = node->irData.get_view_if<FetchView>()) {
//         if (fetchV->wasTableBaseNode()) {
            
//             // Fix: Clear the dummy empty column created in astToIr
//             if (!node->irData.inputColumns.empty() && node->irData.inputColumns[0].columnName.empty()) {
//                 node->irData.inputColumns.clear();
//             }

//             for (BaseType::TableColumn col : columnsToMaterializeOn) {
//                 std::string tableName = fetchV->inputCol().table.name;
//                 std::string tableAlias = fetchV->inputCol().table.alias.value_or("");
                
//                 // Only add if it matches this table
//                 if (col.table.name == tableName || (!tableAlias.empty() && tableAlias == col.table.name)) {
//                     // Avoid duplicates
//                     bool exists = false;
//                     for(const auto& existing : node->irData.inputColumns) {
//                         if(existing.columnName == col.columnName) exists = true;
//                     }
//                     if(!exists) {
//                         node->irData.inputColumns.push_back(col);
//                     }
//                     // Important: Ensure pMat knows this node provides this column
//                     // This handles the "Fetch Insertion" issue logic
//                     pMat[col] = std::shared_ptr<PlanNode>(node, [](PlanNode*){}); // shared_ptr aliasing or just current node? 
//                     // Be careful with shared_from_this logic if not enabled. 
//                     // Actually, for TableBaseNodes, we usually don't need pMat here because the parent loop handles it.
//                 }      
//                 allTablesBelow.insert(fetchV->inputCol().table);
//             }
//         }
//     }

//     // ---------------------------------------------------------
//     // PHASE 2: Rebuild Children (Wiring)
//     // ---------------------------------------------------------
//     // Set the children of this node to be exactly the Materialization/Fetch nodes
//     // that provide the columns this node needs.
    
//     // Only do this if this node actually consumes columns (i.e. not a leaf/TableBase)
//     if (!node->children.empty()) {
//         std::vector<std::shared_ptr<PlanNode>> newChildren;
//         std::set<std::shared_ptr<PlanNode>> addedNodes; // Dedup

//         for (const auto& col : columnsThisNode) {
//             if (pMat.count(col)) {
//                 std::shared_ptr<PlanNode> provider = pMat[col];
                
//                 // Only add unique providers to the children list
//                 if (addedNodes.find(provider) == addedNodes.end()) {
//                     newChildren.push_back(provider);
//                     addedNodes.insert(provider);
//                 }
//             }
//         }

//         // If we found providers, replace children.
//         // If we didn't (e.g., ResultNode referencing virtual cols, or Setup nodes), keep original?
//         // Usually, for Join/Filter/Group, this replacement is correct.
//         if (!newChildren.empty()) {
//             node->children = newChildren;
//         }
//     }

//     return MaterializationData(allTablesBelow, pMat);
// }