#include "ir_transformer.hpp"

#include <memory>
#include <unordered_set>
#include <regex>

#include <ir/catalog.hpp>
#include <ir/ir_base.hpp>

// #include <WorkItem.pb.h>

namespace {
    const std::unordered_set<std::string> HASH = {"Hash"};
    const std::unordered_set<std::string> GATHER = {"Gather", "Gather Merge"};
    const std::unordered_set<std::string> JOIN = {"Nested Loop", "Hash Join"};
    const std::unordered_set<std::string> AGG = {"Aggregate"};
    const std::unordered_set<std::string> BITMAP = {"Bitmap Heap Scan", "Bitmap Index Scan"};
    const std::unordered_set<std::string> SCAN = {"Index Scan", "Seq Scan"};
    const std::unordered_set<std::string> SORT = {"Incremental Sort", "Sort"};

    const std::unordered_set<std::string> PRUNE_TARGETS = {"Hash", "Gather", "Gather Merge"};

    bool isNodeType(const PlanNode* node, const std::string& type) {
        return node && node->rawJson.has_value() && node->rawJson->nodeType == type;
    }

    bool isNodeType(const PlanNode* node, const std::unordered_set<std::string>& types) {
        return node && node->rawJson.has_value() && types.count(node->rawJson->nodeType);
    }

}

PlanNode::AbstractData convertToAbstract(const BaseType::JsonRawData& rawJson) {
    PlanNode::AbstractData abstract_data;

    if (JOIN.count(rawJson.nodeType)) {
        AbstractJoin join;
        abstract_data = join;
    }
    else if (AGG.count(rawJson.nodeType)) {
        AbstractAgg agg;
        abstract_data = agg;
    }
    else if (SORT.count(rawJson.nodeType)) {
        AbstractSort sort;
        abstract_data = sort;
    }
    else if (SCAN.count(rawJson.nodeType) || BITMAP.count(rawJson.nodeType)) {
        AbstractSource source;

        source.basetable = rawJson.planParams.baseTable.has_value() 
                 ? rawJson.planParams.baseTable->name 
                 : "";
        abstract_data = source;
    }
    else if (rawJson.nodeType == "Limit") {
        AbstractResult result;
        abstract_data = result;
    }
    return abstract_data;
}

void removeDeepAggregate(std::shared_ptr<PlanNode>& node) {
    // Pointer to the shared_ptr we are currently inspecting
    std::shared_ptr<PlanNode>* currentPtr = &node;

    // Find aggregate
    while (*currentPtr && !isNodeType(currentPtr->get(), AGG)) {
        if ((*currentPtr)->children.empty()) return; 
        currentPtr = &((*currentPtr)->children[0]);
    }

    // Replace the aggregate with its own child
    if (*currentPtr && isNodeType(currentPtr->get(), AGG)) {
        if ((*currentPtr)->children.size() == 1) {
            *currentPtr = (*currentPtr)->children[0];
        }
    }
}

std::shared_ptr<PlanNode> pruneTree(std::shared_ptr<PlanNode> node) {
    if (!node) return nullptr;

    // 1. RECURSE FIRST: Process children bottom-up
    // We modify the vector in-place by assigning the result of the recursive call back to the slot.
    for (auto& child : node->children) {
        child = pruneTree(child); 
    }

    // Safety check: If node has no raw data (synthetic), return as is
    if (!node->rawJson.has_value()) return node;

    const std::string& currentType = node->rawJson->nodeType;

    // PRUNE TREE

    // Remove hash table nodes
    if (HASH.count(currentType)) {
        // THE REWIRE TRICK:
        // 1. We detach the first child from the current 'node'.
        // 2. We return that child to the *caller* (the parent of 'node').
        // 3. 'node' itself (the Hash/Gather) goes out of scope here and is deleted.
        if (node->children.size() == 1) {
            return node->children[0];
        }
    }

    // Remove gather and corresponding aggregate
    if (GATHER.count(currentType)) {
        if (node->children.size() == 1) {
            // Remove corresponding aggregate
            removeDeepAggregate(node->children[0]);
            
            // Delete the Gather itself by returning the fixed child
            return node->children[0];
        }
    }
    
    // Collapse bitmap
    if (BITMAP.count(currentType) && node->children.size() == 1) {
        PlanNode* childPtr = node->children[0].get();

        if (isNodeType(childPtr, BITMAP)) {
            if (childPtr->rawJson.has_value()) {
            // If parent doesn't have a filter, take the child's
                if (!node->rawJson->planParams.filterPredicate.has_value()) {
                        node->rawJson->planParams.filterPredicate = childPtr->rawJson->planParams.filterPredicate;
                }
                
                // Also copy the index name if needed
                if (node->rawJson->planParams.index.empty()) {
                    node->rawJson->planParams.index = childPtr->rawJson->planParams.index;
                }
            }
            node->children = childPtr->children;
        }
    }
    
    // Generate abstract representation and fill with needed raw data 
    node->abstractData = convertToAbstract(node->rawJson.value());

    return node; // Return the modified (but same pointer) node
}

namespace {

    // Helper to map column prefixes to table names (SSB Schema)
    std::string getTableFromColumn(const std::string& col) {
        if (col.find("lo_") == 0) return "lineorder";
        if (col.find("c_") == 0)  return "customer";
        if (col.find("s_") == 0)  return "supplier";
        if (col.find("p_") == 0)  return "part";
        if (col.find("d_") == 0)  return "dates"; // dim_date is currently used in plan, but remapped to dates in parsing
        return "";
    }

    // Helper to extract the two tables involved in a condition string
    // e.g., "lo_custkey = c_custkey" -> {"lineorder", "customer"}
    std::pair<std::string, std::string> parseConditionTables(const std::string& condition) {
        auto eqPos = condition.find('=');
        if (eqPos == std::string::npos) return {"", ""};

        std::string left = condition.substr(0, eqPos);
        std::string right = condition.substr(eqPos + 1);
        
        // minimal trim (you might already have a trim function)
        left.erase(0, left.find_first_not_of(" \t"));
        left.erase(left.find_last_not_of(" \t") + 1);
        right.erase(0, right.find_first_not_of(" \t"));
        right.erase(right.find_last_not_of(" \t") + 1);

        return {getTableFromColumn(left), getTableFromColumn(right)};
    }
}

std::set<std::string> enrichTreeSub(PlanNode* node, SqlQueryData& queryData){
    
    // Sets of tables of each the children (should be no more than 2)
    std::vector<std::set<std::string>> childTableSets;
    
    // Union of the tables of all children, therefore the tables that can be found in the whole subtree
    std::set<std::string> currentTables;

    // Collect base tables from children
    for (const std::shared_ptr<PlanNode>& child : node->children) {
        std::set<std::string> childTables = enrichTreeSub(child.get(), queryData);
        childTableSets.push_back(childTables);
        currentTables.insert(childTables.begin(), childTables.end());
    }

    // CASE source node
    if (AbstractSource* source = std::get_if<AbstractSource>(&node->abstractData)) {
        std::string basetable = source->basetable;
        
        for (const std::string& cond : queryData.conditions) {
            
            // Add all filters to the source node
            std::regex attrRegex(R"(\b[a-z]+_[a-z0-9]+\b)");
            auto begin = std::sregex_iterator(cond.begin(), cond.end(), attrRegex);
            auto end = std::sregex_iterator();
            std::set<std::string> tablesInCond;

            for (std::sregex_iterator i = begin; i!= end; ++i) {
                std::string attr = i->str();
                std::string table = getTableFromColumn(attr);
                tablesInCond.insert(table);                
            }
            if (tablesInCond.size() == 1 && tablesInCond.count(basetable)) source->filters.push_back(cond);
        }

        // Return the a set containing the basetable
        return {basetable};
    }

    // Case sort node
    if (AbstractSort* sort = std::get_if<AbstractSort>(&node->abstractData)) {
        std::vector<std::string> col_names;
        std::vector<bool> sort_orders;
        for (int i=0; i < queryData.sorting.size(); ++i){
            col_names.push_back(queryData.sorting[i].field);
            sort_orders.push_back(queryData.sorting[i].asc);
        }
        sort->column_names = col_names;
        sort->asc = sort_orders;
    }

    // Case aggregation node
    if (AbstractAgg* agg = std::get_if<AbstractAgg>(&node->abstractData)) {
        Aggregation agg_from_query = queryData.aggregations[0]; // although parsing supports several, in ssb there's only one
        agg->agg_type = agg_from_query.func;
        agg->agg_mapping = agg_from_query.mapping;
        agg->agg_alias = agg_from_query.alias;
    }

    // CASE join node
    if (AbstractJoin* join = std::get_if<AbstractJoin>(&node->abstractData)) {
        
        // We only support joins of two tables. There should only be two tables in childTableSets
        if (childTableSets.size() == 2) {
            const std::set<std::string>& leftSet = childTableSets[0];
            const std::set<std::string>& rightSet =  childTableSets[1];
        
            // Find the right condition
            for (const std::string& cond : queryData.conditions) {
                auto [t1, t2] = parseConditionTables(cond);                

                bool match = (leftSet.count(t1) && rightSet.count(t2)) ||
                             (leftSet.count(t2) && rightSet.count(t1));
                             
                if (match) {
                    join->condition = cond;
                    join->left_table = t1;
                    join->right_table = t2;
                    break; // don't process any other conditions, as we found the one for the join
                } // additionally pop the condition?            
            }
        }
    }
    return currentTables;
}

std::shared_ptr<PlanNode> enrichTree(std::shared_ptr<PlanNode> root, SqlQueryData& queryData) {

    // Enrich all nodes of the tree with data from query 
    enrichTreeSub(root.get(), queryData);

    // Build result node as new root
    std::shared_ptr<PlanNode> resultNode = std::make_shared<PlanNode>();
    
    AbstractResult result;
    
    // Add data from query into the result
    std::vector<std::string> select_cols;
    for (const Selection& col : queryData.selections) {
        select_cols.push_back(col.content);
    }
    
    // Fill result node and set it as new root of the tree
    result.output_cols = select_cols;
    resultNode->abstractData = result;
    resultNode->children.push_back(root);

    return resultNode;
}

namespace {

    // --- Helper Functions for Enum Mapping ---

    CompType mapStringToCompType(const std::string& op) {
        if (op == "=") return COMP_EQ;
        if (op == "<") return COMP_LT;
        if (op == "<=") return COMP_LE;
        if (op == ">") return COMP_GT;
        if (op == ">=") return COMP_GE;
        if (op == "!=" || op == "<>") return COMP_NE;
        if (op == "BETWEEN") return COMP_BETWEEN; // Simplification
        if (op == "IN") return COMP_IN;
        return COMP_EQ; // Default fallback
    }

    BaseType::Join mapStringToJoinType(std::string type) {
        std::transform(type.begin(), type.end(), type.begin(), ::toupper);
        if (type.find("LEFT") != std::string::npos) return BaseType::LEFT_OUTER_JOIN;
        if (type.find("RIGHT") != std::string::npos) return BaseType::RIGHT_OUTER_JOIN;
        if (type.find("FULL") != std::string::npos) return BaseType::FULL_OUTER_JOIN;
        return BaseType::INNER_JOIN;
    }

    RelOp mapStringToRelOp(std::string op) {
        std::transform(op.begin(), op.end(), op.begin(), ::toupper);
        if (op == "INTERSECT") return REL_INTERSECTION;
        if (op == "EXCEPT") return REL_NEGATION;
        return REL_UNION;
    
    }
    std::optional<AggFunc> mapStringToAggFunc(std::string func) {
        if (func.empty()) return std::nullopt;
        std::transform(func.begin(), func.end(), func.begin(), ::toupper);
        if (func == "SUM") return AGG_SUM;
        if (func == "COUNT") return AGG_COUNT;
        if (func == "MIN") return AGG_MIN;
        if (func == "MAX") return AGG_MAX;
        if (func == "AVG") return AGG_AVG;
        return std::nullopt;
    }

    // --- Value Parsing based on Known Type ---
    std::variant<uint64_t, float, std::string> parseValueByType(const std::string& val, ColumnType type) {
        if (type == ColumnType::TYPE_INTEGER) {
            try { return static_cast<uint64_t>(std::stoull(val)); } 
            catch (...) { return static_cast<uint64_t>(0); }
        }
        if (type == ColumnType::TYPE_FLOAT) {
            try { return std::stof(val); } 
            catch (...) { return 0.0f; }
        }
        // Fallback / String
        return val;
    }
}

std::shared_ptr<PlanNode> astToIr(ASTNode* ast) {
    if (!ast) return nullptr;

    auto node = std::make_shared<PlanNode>();
    PlanNode* childrenTarget = node.get();

    // 1. SELECT Node
    if (auto e = std::get_if<SelectClauseNode>(&ast->val)) {
        // LOOKUP: Get real type from Catalog
        ColumnType type = Catalog::getSSBColumnType(e->table, e->column);
        BaseType::TableColumn col(e->table, e->column, type, e->alias);
        
        auto aggType = mapStringToAggFunc(e->aggregateFunction);

        if (aggType.has_value()) {
            // Dismantle: Select -> Agg
            auto aggNode = std::make_shared<PlanNode>();
            
            BaseType::TableColumn aggInputCol(e->table, e->column, type);
            
            aggNode->irData = IR::AggNode(aggInputCol, aggType.value(), aggInputCol);

            // Outer Select selects the Agg result
            node->irData = IR::SelectNode(e->star, col, e->distinct, col);

            childrenTarget = aggNode.get(); 
            node->children.push_back(aggNode);
        } else {
            node->irData = IR::SelectNode(e->star, col, e->distinct, col);
        }
    }
    // 2. WHERE / Filter Node
    else if (auto e = std::get_if<WhereClauseNode>(&ast->val)) {
        // LOOKUP: Get type for the Input Column
        ColumnType colType = Catalog::getSSBColumnType(e->table, e->column);
        BaseType::TableColumn inputCol(e->table, e->column, colType);
        std::optional<BaseType::TableColumn> col2 = std::nullopt;

        std::vector<std::variant<uint64_t, float, std::string>> filterArgs;
        CompType opType = mapStringToCompType(e->operatorType);

        if (e->operatorType == "OR") {
            opType = CompType::COMP_IN;

            filterArgs.push_back(parseValueByType(e->value, colType));
            if (!e->value2.empty()) {
                filterArgs.push_back(parseValueByType(e->value2, colType));
            }
        }
        else if (e->operatorType == "BETWEEN") {
            opType = CompType::COMP_BETWEEN;

            filterArgs.push_back(parseValueByType(e->value, colType));
            filterArgs.push_back(parseValueByType(e->value2, colType));
        }
        // Column based filter (or join)
        else if (!e->column.empty() && !e->column2.empty()) {
            std::string table2 = !e->table2.empty() ? e->table2 : e->table;
            ColumnType col2Type = Catalog::getSSBColumnType(table2, e->column2);
            col2 = BaseType::TableColumn(table2, e->column2, col2Type);
        }
        // Single value filter
        else {
            filterArgs.push_back(parseValueByType(e->value, colType));
        }

        node->irData = IR::FilterNode(
            inputCol,
            opType,
            col2,
            filterArgs,
            inputCol
        );
    }
    // 3. JOIN Node
    else if (auto e = std::get_if<TableJoinNode>(&ast->val)) {
        ColumnType leftType = Catalog::getSSBColumnType(e->onLeftTable, e->onLeftTableColumn);
        ColumnType rightType = Catalog::getSSBColumnType(e->onRightTable, e->onRightTableColumn);

        BaseType::TableColumn leftCol(e->onLeftTable, e->onLeftTableColumn, leftType);
        BaseType::TableColumn rightCol(e->onRightTable, e->onRightTableColumn, rightType);
        BaseType::TableColumn outCol("", e->onLeftTableColumn + "=" + e->onRightTableColumn, ColumnType::TYPE_PAIR_POSLIST);
        
        node->irData = IR::JoinNode(
            mapStringToJoinType(e->joinType),
            CompType::COMP_EQ, 
            leftCol, 
            rightCol,
            outCol
        );
    }
    // 4. Base Table
    else if (auto e = std::get_if<TableBaseNode>(&ast->val)) {
        node->irData = IR::TableBaseNode(BaseType::Table(e->tableName, e->tableAlias));
    }
    // 5. Group By
    else if (auto e = std::get_if<GroupByClauseNode>(&ast->val)) {
        std::vector<BaseType::TableColumn> groups;
        std::string groupingColString = "GROUP_";
        int i = 0;
        for (const auto& desc : e->description) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            groups.emplace_back(desc.table, desc.column, type);
            
            groupingColString += desc.table[0] + "." + desc.column + (i < e->description.size() - 1 ? "_" : "");
            i++;
        }
        BaseType::TableColumn outCol("", groupingColString, ColumnType::TYPE_INTEGER);
        node->irData = IR::GroupByNode(groups, outCol);
    }
    // 6. Order By
    else if (auto e = std::get_if<OrderByClauseNode>(&ast->val)) {
        std::vector<BaseType::OrderDescription> orders;
        int i = 0;
        std::string orderColString = "ORDER_";
        for (const auto& desc : e->orderByList) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            bool isAsc = (desc.ordertype != "DESC"); 
            bool isNullsFirst = (desc.nullordering == "FIRST");
            
            orders.emplace_back(
                BaseType::TableColumn(desc.table, desc.column, type), 
                isAsc, 
                isNullsFirst
            );

            orderColString += desc.table[0] + "." + desc.column + (i < e->orderByList.size() - 1 ? "_" : "");
            i++;
        }
        BaseType::TableColumn outCol("", orderColString, ColumnType::TYPE_INTEGER);
        node->irData = IR::SortOrderNode(orders, outCol);
    }
    // 7. Limit (don't support Limit for now)
    // else if (auto e = std::get_if<LimitClauseNode>(&ast->val)) {
    //     node->irData = IR::LimitNode(e->limit, e->offset);
    // }
    // 8. Set Ops
    else if (auto e = std::get_if<SetOperationNode>(&ast->val)) {
        BaseType::TableColumn dummy; // Still dummy as AST has no columns here
        BaseType::TableColumn outCol(
            "", 
            e->setOperation, 
            ColumnType::TYPE_INTEGER
        );
        node->irData = IR::SetOperationNode(mapStringToRelOp(e->setOperation), dummy, dummy, outCol);
    }

    // Recursion
    if (ast->left) {
        childrenTarget->children.push_back(astToIr(ast->left));
    }
    if (ast->right) {
        childrenTarget->children.push_back(astToIr(ast->right));
    }

    // Set Ops: set input columns;
    if (auto e = std::get_if<IR::SetOperationNode>(&node->irData)) {
        std::vector<BaseType::TableColumn>& inputColumns = e->inputColumns;
        inputColumns[0] = *node->children[0]->getIrDataOutputColumn();
        inputColumns[1] = *node->children[1]->getIrDataOutputColumn();
    }

    return node;
}

// Puts Materializes everywhere and also populates the columns to fetch in TableBaseNode
std::set<BaseType::Table> fillMaterializes(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn) {
    if (!node) return {};
    
    std::set<BaseType::Table> tablesBelow;
    
    // Top down add columns needed for this node to ``columnsToMaterializeOn``
    // Here add columns that this node specifically needs
    std::visit([&](auto& n) {
        // Visit to peel of variant (peel irData) (compiler generates code for each possible content (n) of the variant)
        // Get the type of n (determine with decltype, unwrap with decay_t) and check it's not monostate
        if constexpr (!std::is_same_v<std::decay_t<decltype(n)>, std::monostate>)
            columnsToMaterializeOn.insert(n.inputColumns.begin(), n.inputColumns.end());
    }, node->irData);

    std::set<BaseType::Table> allTablesBelow;

    // Generate materialize nodes (bottom up)    
    for (size_t i = 0; i < node->children.size(); ++i) {

        // const auto& node->children[i] = node->children[i];
        std::set<BaseType::Table> tablesBelowChild = fillMaterializes(node->children[i].get(), columnsToMaterializeOn);

        
        BaseType::TableColumn filterCol;
        bool isPositionListNode;
        
        // Check if child outputs a position list and if so, get it
        std::visit([&](auto n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, IR::FilterNode> ||
                std::is_same_v<T, IR::JoinNode> ||
                std::is_same_v<T, IR::GroupByNode> ||
                std::is_same_v<T, IR::SortOrderNode> ||
                std::is_same_v<T, IR::SetOperationNode>){
                    filterCol = n.outputColumn;
                    isPositionListNode = true;
                }
                else {
                    isPositionListNode = false;
                }
                
            }, node->children[i]->irData);
            

        if (isPositionListNode) {
            // Build plan node
            std::shared_ptr<PlanNode> matNode = std::make_shared<PlanNode>();
            auto matNodeContent = IR::MaterializeNode();
            
            for (const auto& idxCol : columnsToMaterializeOn) {
                // Add a materialization to the materialization node if table below
                // TODO: enable correct parsing of aliases -> ideally let TableColumn contain an entry of type Table
                if (tablesBelowChild.contains(BaseType::Table(idxCol.tableName))) {
                    matNodeContent.materializations.push_back(IR::MaterializeNode::Materialization(idxCol, filterCol, idxCol));
                }
            }

            // Insert materializationNode below current node and rewire
            matNode->irData = matNodeContent;
            matNode->children.push_back(node->children[i]);
            node->children[i] = matNode;
        }

        allTablesBelow.merge(tablesBelowChild);

    }

    // The node is a leafnode (a table node)
    if (auto* n = std::get_if<IR::TableBaseNode>(&node->irData)) {
        for (BaseType::TableColumn col : columnsToMaterializeOn) {
            if (col.tableName == n->table.name || (n->table.alias ? n->table.alias == col.tableName : false)) {
                n->inputColumns.push_back(col);
            }      
        allTablesBelow.insert(n->table);
        }
    }

    return allTablesBelow;
}


void irToApiData(PlanNode* node) {
    if (!node) return;

    static BaseType::TableColumn missingCol;

    // 1. Filter Node (checked)
    if (auto* n = std::get_if<IR::FilterNode>(&node->irData)) {
        ItemBuilder::FilterNode filterStruct;
        filterStruct.inputColumn = &n->inputColumns[0];
        filterStruct.outputColumn = &n->outputColumn;
        filterStruct.filterType = n->filterType;
        filterStruct.filterArgVals = n->filterArgs;

        node->apiData = filterStruct;
    }

    // 2. Join Node
    else if (auto* n = std::get_if<IR::JoinNode>(&node->irData)) {
        ItemBuilder::JoinNode joinStruct;
        
        joinStruct.innerColumn = &n->inputColumns[0];
        joinStruct.outerColumn = &n->inputColumns[1];
        joinStruct.outputColumn = &n->outputColumn;
        
        // POINTER ISSUE: ItemBuilder expects CompType*, IR has CompType nue.
        // We take the address of the nue stored in the variant.
        joinStruct.joinPredicate = &n->joinPredicate; 

        node->apiData = joinStruct;
    }

    // 3. Base Table (checked)
    else if (auto* n = std::get_if<IR::TableBaseNode>(&node->irData)) {
        std::vector<ItemBuilder::FetchNode> fetchColList;
        for (auto& colFetch : n->inputColumns) {
            ItemBuilder::FetchNode fetchStruct;
            
            fetchStruct.inputColumn = &colFetch;
            fetchStruct.printToFile = false; // Defaulting to false

            fetchColList.push_back(fetchStruct);
        }
        node->apiData = fetchColList;
    }

    // 4. Group By (MultiGroup)
    else if (auto* n = std::get_if<IR::GroupByNode>(&node->irData)) {
        ItemBuilder::MultiGroupNode groupStruct;

        // Convert values to pointers
        for (auto& col : n->inputColumns) {
            groupStruct.groupColumns.push_back(&col);
        }

        groupStruct.outputIdx = &n->outputColumn;

        // MISSING INFO: The following are required by ItemBuilder but missing in IR::GroupByNode
        groupStruct.outputCluster = &missingCol; 
        groupStruct.aggColumn = &missingCol; 
        groupStruct.aggResultColumn = &missingCol;
        groupStruct.storeExtends = false; 
        // groupStruct.sortOrders = {}; // empty default

        node->apiData = groupStruct;
    }

    // 5. Aggregation
    else if (auto* n = std::get_if<IR::AggNode>(&node->irData)) {
        ItemBuilder::AggNode aggStruct;
        aggStruct.inputColumn = &n->inputColumns[0];
        aggStruct.outputColumn = &n->outputColumn;
        aggStruct.aggFunc = n->aggFunc;
        // groupColumns in IR::AggNode (vector<string>) matches ItemBuilder logic
        // If IR vector is empty, it assumes purely scalar agg or handled by MultiGroup
        aggStruct.groupColumns = {}; 

        node->apiData = aggStruct;
    }

    // 6. Sort (checked())
    else if (auto* n = std::get_if<IR::SortOrderNode>(&node->irData)) {
        ItemBuilder::SortNode sortStruct;

        for (auto& desc : n->columnList) {
            sortStruct.inputColumns.push_back(&desc.column);
            sortStruct.sortOrders.push_back(desc.orderType); // assuming bool maps directly
        }

        sortStruct.idxOutput = &n->outputColumn;
        
        // MISSING INFO: ItemBuilder asks for 'existingIdx' (pointer).
        sortStruct.existingIdx = &n->columnList[0].column; //&missingCol;

        node->apiData = sortStruct;
    }

    // 7. Set Operation (checked())
    else if (auto* n = std::get_if<IR::SetOperationNode>(&node->irData)) {
        ItemBuilder::SetOperationNode setStruct;
        setStruct.operation = n->operation;
        
        // IR stores inputs in a vector, Builder wants explicit pointers
        setStruct.innerColumn = (n->inputColumns.size() > 0) ? &n->inputColumns[0] : nullptr;
        setStruct.outerColumn = (n->inputColumns.size() > 1) ? &n->inputColumns[1] : nullptr;
        setStruct.outputColumn = &n->outputColumn;

        node->apiData = setStruct;
    }

    // 8. Materialize (checked)
    else if (auto* n = std::get_if<IR::MaterializeNode>(&node->irData)) {
        std::vector<ItemBuilder::MaterializeNode> matList;
        for (auto& matData : n->materializations) {
            ItemBuilder::MaterializeNode matStruct;

            matStruct.idxColumn = &matData.idxColumn;
            matStruct.filterColumn = &matData.filterColumn;
            matStruct.outputColumn = &matData.idxColumn; // Reuse input as output if not specified otherwise
            matList.push_back(matStruct);
        }
        node->apiData = matList;
    }

    // 9. Select (Result)
    else if (auto* n = std::get_if<IR::SelectNode>(&node->irData)) {
        ItemBuilder::ResultNode resultStruct;

        // MISSING INFO: Filename is not in IR.
        resultStruct.filename = "result.csv"; 

        // IR::SelectNode has 'outputColumn' (singular). 
        // Builder expects a vector of result columns.
        resultStruct.resultColumns.push_back(&n->outputColumn);
        resultStruct.resultHeaders.push_back(n->outputColumn.columnName);
        
        // MISSING INFO: resultIdx
        resultStruct.resultIdx = &missingCol;

        node->apiData = resultStruct;
    }

    for (const auto& child : node->children){
        irToApiData(child.get());
    }
}