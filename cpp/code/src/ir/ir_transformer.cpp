#include "ir_transformer.hpp"

#include <memory>
#include <unordered_set>
#include <regex>

#include <ir/catalog.hpp>
#include <ir/ir_types.hpp>
#include <ir/ir_views.hpp>

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
            
            // aggNode->irData = IR::AggNode(aggInputCol, aggType.value(), aggInputCol);
            aggNode->irData = AggView::create(aggInputCol, aggInputCol, aggType.value());

            // Outer Select selects the Agg result
            // node->irData = IR::SelectNode(e->star, col, e->distinct, col);
            node->irData = SelectView::create({col},e->star,e->distinct);

            childrenTarget = aggNode.get(); 
            node->children.push_back(aggNode);
        } else {
            // node->irData = IR::SelectNode(e->star, col, e->distinct, col);
            node->irData = SelectView::create({col},e->star,e->distinct);
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

        // node->irData = IR::FilterNode(
        //     inputCol,
        //     opType,
        //     col2,
        //     filterArgs,
        //     inputCol
        // );
        node->irData = FilterView::create(
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
        BaseType::TableColumn outCol(BaseType::Table(""), e->onLeftTableColumn + "=" + e->onRightTableColumn, ColumnType::TYPE_INTEGER);
        
        node->irData = JoinView::create(
            leftCol, 
            rightCol,
            outCol,
            mapStringToJoinType(e->joinType),
            CompType::COMP_EQ 
        );
    }
    // 4. Base Table
    else if (auto e = std::get_if<TableBaseNode>(&ast->val)) {
        BaseType::Table table(e->tableName, e->tableAlias);
        node->irData = FetchView::create(BaseType::TableColumn(table, "", ColumnType::TYPE_INTEGER), true);
    }
    // 5. Group By
    else if (auto e = std::get_if<GroupByClauseNode>(&ast->val)) {
        std::vector<BaseType::TableColumn> groups;
        // std::string groupingColString = "GROUP_";
        std::stringstream ss;
        ss << "GROUP_";
        int i = 0;
        for (const auto& desc : e->description) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            groups.emplace_back(desc.table, desc.column, type);
            
            // groupingColString += (desc.table.empty() ? "" : desc.table[0] + ".") + desc.column + (i < e->description.size() - 1 ? "_" : "");
            char tablePrefix = desc.table.empty() ? '?' : desc.table[0];
    
            ss << tablePrefix << "." << desc.column;
    
            // Add underscore only if it's not the last element
            if (i < e->description.size() - 1) {
                ss << "_";
            }
            i++;
        }
        std::string groupingColString = ss.str();
        BaseType::TableColumn outCol(BaseType::Table(""), groupingColString, ColumnType::TYPE_INTEGER);
        node->irData = GroupView::create(groups, outCol);
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

            orderColString += std::string(1, desc.table[0]) + "." + desc.column + (i < e->orderByList.size() - 1 ? "_" : "");
            i++;
        }
        BaseType::TableColumn outCol(BaseType::Table(""), orderColString, ColumnType::TYPE_INTEGER);
        node->irData = SortOrderView::create(orders, outCol);
    }
    // 7. Limit (don't support Limit for now)
    // else if (auto e = std::get_if<LimitClauseNode>(&ast->val)) {
    //     node->irData = IR::LimitNode(e->limit, e->offset);
    // }
    // 8. Set Ops
    else if (auto e = std::get_if<SetOperationNode>(&ast->val)) {
        BaseType::TableColumn dummy; // Still dummy as AST has no columns here
        BaseType::TableColumn outCol(
            BaseType::Table(""), 
            e->setOperation, 
            ColumnType::TYPE_INTEGER
        );
        node->irData = SetOpView::create(
            dummy, 
            dummy, 
            outCol,
            mapStringToRelOp(e->setOperation) 
        );
    }

    // Recursion
    if (ast->left) {
        childrenTarget->children.push_back(astToIr(ast->left));
    }
    if (ast->right) {
        childrenTarget->children.push_back(astToIr(ast->right));
    }

    // Set Ops: set input columns;
    if (node->irData.is<SetOp>()) {
        std::vector<BaseType::TableColumn>& inputColumns = node->irData.inputColumns;
        inputColumns[0] = node->children[0]->irData.outputCols[0];
        inputColumns[1] = node->children[1]->irData.outputCols[0];
    }

    return node;
}

// Puts Materializes everywhere and also populates the columns to fetch in TableBaseNode
MaterializationData fillMaterializes(PlanNode* node, std::set<BaseType::TableColumn> columnsToMaterializeOn, const std::set<BaseType::TableColumn>& inputOfParent) {
    if (!node) return MaterializationData();

    std::map<BaseType::TableColumn, std::shared_ptr<PlanNode>> pMat; // previous materializations
    
    std::set<BaseType::Table> tablesBelow;
    std::set<BaseType::Table> allTablesBelow;

    columnsToMaterializeOn.insert(node->irData.inputColumns.begin(), node->irData.inputColumns.end());

    // Fetch columns this node needs
    std::set<BaseType::TableColumn> columnsThisNode(node->irData.inputColumns.begin(), node->irData.inputColumns.end());

    // Generate materialize nodes (bottom up)    
    for (size_t i = 0; i < node->children.size(); ++i) {

        std::shared_ptr<PlanNode> originalChild = node->children[i];
        
        // const auto& node->children[i] = node->children[i];
        // ##### RECURSION HERE ######
        MaterializationData matData = fillMaterializes(node->children[i].get(), columnsToMaterializeOn, columnsThisNode);
        std::set<BaseType::Table> tablesBelowChild = matData.tablesBelow;
        pMat.merge(matData.previousMaterializations);
        
        
        BaseType::TableColumn filterCol;
        bool childIsPositionListNode = false;
        
        if (node->children[i]->irData.is<JoinOp>()
            || node->children[i]->irData.is<FilterOp>()
            || node->children[i]->irData.is<GroupOp>()
            || node->children[i]->irData.is<SortOp>()
            || node->children[i]->irData.is<SetOp>()) {
                filterCol = node->children[i]->irData.outputCols[0]; // except for Select/Result all nodes at the moment only have one output column
                childIsPositionListNode = true;
            }
        
        // Create or update materialization if child outputs position list
        if (childIsPositionListNode) {
                             
            for (const auto& idxCol : columnsToMaterializeOn) {

                // Add a materialization to the materialization node if table below
                if (tablesBelowChild.contains(BaseType::Table(idxCol.table.name))) {
                                                            
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
                    matNode->children.push_back(originalChild);
                    
                    // Set this materialization as the most recent one
                    pMat[idxCol] = matNode;
                }
            }
        }
        else {
            BaseType::TableColumn outCol = node->irData.outputCols[0];
            std::shared_ptr<PlanNode> physOutNode = std::make_shared<PlanNode>(*node);
            // if (!outCol.columnName.empty()) pMat[outCol] = physOutNode;
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
        for (auto& idxCol : columnsThisNode) {
            // Set most recent materialization of children as column
            if (pMat.count(idxCol))
                node->children.push_back(pMat[idxCol]);
            else {
                std::cout << "scream" << std::endl;
            }
        }
    }

    return MaterializationData(allTablesBelow, pMat);

}


void irToApiData(PlanNode* node) {
    if (!node) return;

    static BaseType::TableColumn missingCol;

    // 1. Filter Node (checked)
    if (auto filterV = node->irData.get_view_if<FilterView>()) {
        ItemBuilder::FilterNode filterStruct;
        filterStruct.inputColumn = &filterV->col1();
        filterStruct.outputColumn = &filterV->outputCol();
        filterStruct.filterType = filterV->filterType();
        filterStruct.filterArgVals = filterV->filterArgs();

        node->apiData = filterStruct;
    }

    // 2. Join Node
    else if (auto joinV = node->irData.get_view_if<JoinView>()) {
        ItemBuilder::JoinNode joinStruct;
        
        joinStruct.innerColumn = &joinV->inner();
        joinStruct.outerColumn = &joinV->outer();
        joinStruct.outputColumn = &joinV->output();
        joinStruct.joinPredicate = &joinV->joinPredicate(); 

        node->apiData = joinStruct;
    }

    // 3. Base Table (checked)
    else if (auto fetchV = node->irData.get_view_if<FetchView>()) {
            ItemBuilder::FetchNode fetchStruct;
            
            fetchStruct.inputColumn = &fetchV->inputCol();
            fetchStruct.printToFile = false; // Defaulting to false

        node->apiData = fetchStruct;
    }

    // 4. Group By (MultiGroup)
    else if (auto groupV = node->irData.get_view_if<GroupView>()) {
        ItemBuilder::MultiGroupNode groupStruct;

        // Convert values to pointers
        for (auto& col : groupV->groupingCols()) {
            groupStruct.groupColumns.push_back(&col);
        }
        groupStruct.outputIdx = &groupV->outputIdx();
        groupStruct.outputCluster = &groupV->outputCluster(); 
        // TODO: how to set aggCol if not provided
        if (auto& aggCol = groupV->aggCol()) {
            if (auto& aggResultCol = groupV->aggResultCol()) {
                groupStruct.aggColumn = &aggCol.value(); 
                groupStruct.aggResultColumn = &aggResultCol.value();
            }
        }
        else {
            groupStruct.aggColumn = &missingCol;
            groupStruct.aggResultColumn = &missingCol;
        }
        groupStruct.storeExtends = false; 
        groupStruct.sortOrders = groupV->sortOrders();
        
        node->apiData = groupStruct;
    }

    // 5. Aggregation
    else if (auto aggV = node->irData.get_view_if<AggView>()) {
        ItemBuilder::AggNode aggStruct;
        aggStruct.inputColumn = &aggV->colToAgg();
        aggStruct.outputColumn = &aggV->aggResultCol();
        aggStruct.aggFunc = aggV->aggFunc();
        // groupColumns in IR::AggNode (vector<string>) matches ItemBuilder logic
        // If IR vector is empty, it assumes purely scalar agg or handled by MultiGroup
        aggStruct.groupColumns = {}; 

        node->apiData = aggStruct;
    }

    // 6. Sort (checked())
    else if (auto sortV = node->irData.get_view_if<SortOrderView>()) {
        ItemBuilder::SortNode sortStruct;

        for (auto& desc : sortV->orderDescriptions()) {
            sortStruct.inputColumns.push_back(&desc.column);
            sortStruct.sortOrders.push_back(desc.orderType); // assuming bool maps directly
        }
        sortStruct.idxOutput = &sortV->idxOutput();
        sortStruct.existingIdx = sortV->existingIdx() ? &*sortV->existingIdx() : &missingCol;

        node->apiData = sortStruct;
    }

    // 7. Set Operation (checked())
    else if (auto setOpV = node->irData.get_view_if<SetOpView>()) {
        ItemBuilder::SetOperationNode setStruct;
        setStruct.operation = setOpV->operation();
        
        // IR stores inputs in a vector, Builder wants explicit pointers
        setStruct.innerColumn = &setOpV->innerCol();
        setStruct.outerColumn = &setOpV->outerCol();
        setStruct.outputColumn = &setOpV->outputCol();

        node->apiData = setStruct;
    }

    // 8. Materialize (checked)
    else if (auto matV = node->irData.get_view_if<MaterializeView>()) {
        
        ItemBuilder::MaterializeNode matStruct;

        matStruct.idxColumn = &matV->idxCol();
        matStruct.filterColumn = &matV->filterCol();
        matStruct.outputColumn = &matV->outputCol();
        
        node->apiData = matStruct;
    }

    // 9. Select (Result)
    else if (auto selectV = node->irData.get_view_if<SelectView>()) {
        ItemBuilder::ResultNode resultStruct;

        // MISSING INFO: Filename is not in IR.
        resultStruct.filename = "result.csv"; 

        // IR::SelectNode has 'outputColumn' (singular). 
        // Builder expects a vector of result columns.
        for (auto& col : selectV->resultCols()) {
            resultStruct.resultColumns.push_back(&col);
        }
        resultStruct.resultHeaders = selectV->resultHeaders();
        resultStruct.resultIdx = selectV->resultIdx().has_value() ? &selectV->resultIdx().value() : &missingCol;

        node->apiData = resultStruct;
    }

    for (const auto& child : node->children){
        irToApiData(child.get());
    }
}