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

void removeDeepAggregate(std::unique_ptr<PlanNode>& node) {
    // Pointer to the unique_ptr we are currently inspecting
    std::unique_ptr<PlanNode>* currentPtr = &node;

    // Find aggregate
    while (*currentPtr && !isNodeType(currentPtr->get(), AGG)) {
        if ((*currentPtr)->children.empty()) return; 
        currentPtr = &((*currentPtr)->children[0]);
    }

    // Replace the aggregate with its own child
    if (*currentPtr && isNodeType(currentPtr->get(), AGG)) {
        if ((*currentPtr)->children.size() == 1) {
            *currentPtr = std::move((*currentPtr)->children[0]);
        }
    }
}

std::unique_ptr<PlanNode> pruneTree(std::unique_ptr<PlanNode> node) {
    if (!node) return nullptr;

    // 1. RECURSE FIRST: Process children bottom-up
    // We modify the vector in-place by assigning the result of the recursive call back to the slot.
    for (auto& child : node->children) {
        child = pruneTree(std::move(child)); 
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
            return std::move(node->children[0]);
        }
    }

    // Remove gather and corresponding aggregate
    if (GATHER.count(currentType)) {
        if (node->children.size() == 1) {
            // Remove corresponding aggregate
            removeDeepAggregate(node->children[0]);
            
            // Delete the Gather itself by returning the fixed child
            return std::move(node->children[0]);
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
            node->children = std::move(childPtr->children);
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
    for (const std::unique_ptr<PlanNode>& child : node->children) {
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

std::unique_ptr<PlanNode> enrichTree(std::unique_ptr<PlanNode> root, SqlQueryData& queryData) {

    // Enrich all nodes of the tree with data from query 
    enrichTreeSub(root.get(), queryData);

    // Build result node as new root
    std::unique_ptr<PlanNode> resultNode = std::make_unique<PlanNode>();
    
    AbstractResult result;
    
    // Add data from query into the result
    std::vector<std::string> select_cols;
    for (const Selection& col : queryData.selections) {
        select_cols.push_back(col.content);
    }
    
    // Fill result node and set it as new root of the tree
    result.output_cols = select_cols;
    resultNode->abstractData = result;
    resultNode->children.push_back(std::move(root));

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

std::unique_ptr<PlanNode> astToIr(ASTNode* ast) {
    if (!ast) return nullptr;

    auto node = std::make_unique<PlanNode>();
    PlanNode* childrenTarget = node.get();

    // 1. SELECT Node
    if (auto e = std::get_if<SelectClauseNode>(&ast->val)) {
        // LOOKUP: Get real type from Catalog
        ColumnType type = Catalog::getSSBColumnType(e->table, e->column);
        BaseType::TableColumn col(e->table, e->column, type, e->alias);
        
        auto aggType = mapStringToAggFunc(e->aggregateFunction);

        if (aggType.has_value()) {
            // Dismantle: Select -> Agg
            auto aggNode = std::make_unique<PlanNode>();
            
            BaseType::TableColumn aggInputCol(e->table, e->column, type);
            
            aggNode->irData = IR::AggNode(aggInputCol, aggType.value());

            // Outer Select selects the Agg result
            node->irData = IR::SelectNode(e->star, col, e->distinct);

            childrenTarget = aggNode.get(); 
            node->children.push_back(std::move(aggNode));
        } else {
            node->irData = IR::SelectNode(e->star, col, e->distinct);
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
        
        node->irData = IR::JoinNode(
            mapStringToJoinType(e->joinType),
            CompType::COMP_EQ, 
            leftCol, 
            rightCol
        );
    }
    // 4. Base Table
    else if (auto e = std::get_if<TableBaseNode>(&ast->val)) {
        node->irData = IR::TableBaseNode(BaseType::Table(e->tableName, e->tableAlias));
    }
    // 5. Group By
    else if (auto e = std::get_if<GroupByClauseNode>(&ast->val)) {
        std::vector<BaseType::TableColumn> groups;
        for (const auto& desc : e->description) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            groups.emplace_back(desc.table, desc.column, type);
        }
        node->irData = IR::GroupByNode(groups);
    }
    // 6. Order By
    else if (auto e = std::get_if<OrderByClauseNode>(&ast->val)) {
        std::vector<BaseType::OrderDescription> orders;
        for (const auto& desc : e->orderByList) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            bool isAsc = (desc.ordertype != "DESC"); 
            bool isNullsFirst = (desc.nullordering == "FIRST");
            
            orders.emplace_back(
                BaseType::TableColumn(desc.table, desc.column, type), 
                isAsc, 
                isNullsFirst
            );
        }
        node->irData = IR::SortOrderNode(orders);
    }
    // 7. Limit
    else if (auto e = std::get_if<LimitClauseNode>(&ast->val)) {
        node->irData = IR::LimitNode(e->limit, e->offset);
    }
    // 8. Set Ops
    else if (auto e = std::get_if<SetOperationNode>(&ast->val)) {
        BaseType::TableColumn dummy; // Still dummy as AST has no columns here
        node->irData = IR::SetOperationNode(mapStringToRelOp(e->setOperation), dummy, dummy);
    }

    // Recursion
    if (ast->left) {
        childrenTarget->children.push_back(astToIr(ast->left));
    }
    if (ast->right) {
        childrenTarget->children.push_back(astToIr(ast->right));
    }

    return node;
}

void fillMaterializes(PlanNode* node, std::set<BaseType::TableColumn> columns) {
    if (!node) return;

    // Top down add columns needed for this node to ``columns``
    // Here add columns that this node specifically needs
    std::visit([&](auto& n) {
        // Visit to peel of variant (peel irData) (compiler generates code for each possible content (n) of the variant)
        // Get the type of n (determine with decltype, unwrap with decay_t) and check it's not monostate
        if constexpr (!std::is_same_v<std::decay_t<decltype(n)>, std::monostate>)
            columns.insert(n.inputColumns.begin(), n.inputColumns.end());
    }, node->irData);

    // Traverse Tree
    for (auto& child : node->children) {
        fillMaterializes(child.get(), columns);
    }

    // Bottom up generate materialize nodes
    if (!node->children.empty()){
        for (size_t i =0; i < node->children.size(); ++i) {
            
            // Create Materialize Node
            auto col1 = BaseType::TableColumn(); // dummy for now
            auto col2 = BaseType::TableColumn(); // dummy for now
            std::unique_ptr<PlanNode> matNode = std::make_unique<PlanNode>();
            matNode->irData = IR::MaterializeNode(col1, col2);

            // Insert below current node and rewire
            matNode->children.push_back(std::move(node->children[i]));
            node->children[i] = std::move(matNode);
        }
    }
}