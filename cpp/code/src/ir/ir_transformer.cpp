#include "ir/ir_transformer.hpp"

#include <memory>
#include <unordered_set>
#include <regex>
#include <algorithm>

#include "ir/catalog.hpp"
#include "ir/ir_types.hpp"
#include "ir/ir_views.hpp"

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

void removeDeepSort(std::shared_ptr<PlanNode>& node) {
    // Pointer to the shared_ptr we are currently inspecting
    std::shared_ptr<PlanNode>* currentPtr = &node;

    // Find sort
    while (*currentPtr && !isNodeType(currentPtr->get(), SORT)) {
        if ((*currentPtr)->children.empty()) return;
        currentPtr = &((*currentPtr)->children[0]);
    }

    // Replace the sort with its own child
    if (*currentPtr && isNodeType(currentPtr->get(), SORT)) {
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

    // Remove Sort below Sort
    if (SORT.count(currentType)) {
        if (node->children.size() == 1) {
            removeDeepSort(node->children[0]);
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

    // Swap Agg and Sort if Sort is below Agg
    if (std::holds_alternative<AbstractAgg>(node->abstractData)) {
        if (!node->children.empty()) {
            PlanNode* child = node->children[0].get();
            if (std::holds_alternative<AbstractSort>(child->abstractData)) {
                std::swap(node->abstractData, child->abstractData);
                std::swap(node->rawJson, child->rawJson);
            }
        }
    }

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
        std::vector<std::string> aliases;
        std::vector<bool> sort_orders;
        std::map<std::string, std::string> aliasMap;

        for (const Selection& col : queryData.selections) {
            if (!col.alias.empty()) {
                aliasMap[col.alias] = col.content;
            }
        }

        for (int i=0; i < queryData.sorting.size(); ++i){
            std::string sorting_entry = queryData.sorting[i].field;
            bool sorting_entry_is_alias = aliasMap.contains(sorting_entry);
            std::string col_name = !sorting_entry_is_alias ? sorting_entry : aliasMap[sorting_entry];
            std::string alias = sorting_entry_is_alias ? sorting_entry : "";

            col_names.push_back(col_name);
            aliases.push_back(alias);
            sort_orders.push_back(queryData.sorting[i].asc);
        }
        sort->column_names = col_names;
        sort->aliases = aliases;
        sort->asc = sort_orders;
    }

    // Case aggregation node
    if (AbstractAgg* agg = std::get_if<AbstractAgg>(&node->abstractData)) {
        Aggregation agg_from_query = queryData.aggregations[0]; // although parsing supports several, in ssb there's only one
        agg->agg_type = agg_from_query.func;
        agg->agg_mapping = agg_from_query.mapping;
        agg->agg_alias = agg_from_query.alias;
        agg->grouping_cols = queryData.groupBys;
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

                bool normal = leftSet.count(t1) && rightSet.count(t2);
                bool swapped = leftSet.count(t2) && rightSet.count(t1);

                if (normal || swapped) {
                    join->condition = cond;
                    join->left_table = t1;
                    join->right_table = t2;

                    if (!normal) {
                        std::swap(node->children[0], node->children[1]);
                    }

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
        std::string name = col.content;
        if (!col.alias.empty())
            name += " AS " + col.alias;

        select_cols.push_back(name);
    }

    // Fill result node and set it as new root of the tree
    result.output_cols = select_cols;
    resultNode->abstractData = result;
    resultNode->children.push_back(root);

    return resultNode;
}

namespace IrTransformHelpers {

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

    ArithOp mapStringToArithOp(const std::string& op) {
        if (op == "+") return ARITH_ADD;
        if (op == "-") return ARITH_SUB;
        if (op == "*") return ARITH_MUL;
        if (op == "/") return ARITH_DIV;
        if (op == "%") return ARITH_MOD;
        return ARITH_ADD;
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

    // SELECT Node
    if (auto e = std::get_if<SelectClauseNode>(&ast->val)) {
        std::vector<BaseType::TableColumn> selectCols;
        for (const auto& desc : e->description) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            BaseType::TableColumn col(desc.table, desc.column, type, desc.alias);
            selectCols.push_back(col);
        }
        node->irData = SelectView::create(selectCols);
    }
    // Order By
    else if (auto e = std::get_if<OrderByClauseNode>(&ast->val)) {
        std::vector<BaseType::OrderDescription> orders;
        std::vector<BaseType::TableColumn> sortCols;

        for (const auto& desc : e->orderByList) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            bool isAsc = (desc.ordertype != "DESC");
            bool isNullsFirst = (desc.nullordering == "FIRST");

            BaseType::TableColumn sortCol(desc.table, desc.column, type);

            orders.emplace_back(
                sortCol,
                isAsc,
                isNullsFirst
            );

            sortCols.push_back(sortCol);
        }
        BaseType::TableColumn outCol = SortOrderView::generateOutCol(sortCols);
        node->irData = SortOrderView::create(orders, outCol);
    }
    // Group By
    else if (auto e = std::get_if<GroupByClauseNode>(&ast->val)) {
        
        // Transform to list of table columns
        std::vector<BaseType::TableColumn> groups;
        for (const auto& desc : e->description) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            groups.emplace_back(desc.table, desc.column, type);
        }

        // Generate grouping ir data
        BaseType::TableColumn outCol = GroupView::generateOutCol(groups);
        node->irData = GroupView::create(groups, outCol);
    }
    // Aggregation
    else if (auto e = std::get_if<AggregateClauseNode>(&ast->val)) {
        std::optional<AggFunc> aggFunc = IrTransformHelpers::mapStringToAggFunc(e->aggregateFunction);
        if (aggFunc.has_value()) {
            ColumnType inColType = Catalog::getSSBColumnType(e->table, e->column);
            ColumnType outColType;
            switch (aggFunc.value()) {
                case AGG_COUNT:
                    outColType = ColumnType::TYPE_INTEGER;
                    break;
                case AGG_AVG:
                    outColType = ColumnType::TYPE_FLOAT;
                    break;
                case AGG_MIN:
                case AGG_MAX:
                case AGG_SUM:
                    outColType = inColType;
                    break;
            }
            BaseType::TableColumn aggColIn(e->table, e->column, inColType);
            std::string aggColOutName = e->aggregateFunction + "(" + e->column + ")";
            BaseType::TableColumn aggColOut(BaseType::Table("AGG"), aggColOutName, outColType);
            node->irData = AggView::create(aggColIn, aggColOut, aggFunc.value());
        }
        else {
            std::cout << "Aggregation Function of AST tree could not be parsed!" << std::endl;
        }
    }
    // Map
    else if (auto e = std::get_if<Map>(&ast->val)) {
        ColumnType colTypeInput1 = Catalog::getSSBColumnType(e->table1, e->column1);
        ColumnType colTypeInput2 = Catalog::getSSBColumnType(e->table2, e->column2);
        ArithOp op = IrTransformHelpers::mapStringToArithOp(e->operatorType);

        ColumnType outColType;
        if (colTypeInput1 == ColumnType::TYPE_STRING || colTypeInput2 == ColumnType::TYPE_STRING
            || (op == ARITH_MOD && !(colTypeInput1 == ColumnType::TYPE_INTEGER && colTypeInput2 == ColumnType::TYPE_INTEGER))) {
            throw std::runtime_error("Invalid types for arithmetic operation");
        }
        if (colTypeInput1 == ColumnType::TYPE_FLOAT || colTypeInput2 == ColumnType::TYPE_FLOAT) {
            outColType = ColumnType::TYPE_FLOAT;
        }
        else {
            outColType = ColumnType::TYPE_INTEGER;
        }

        BaseType::TableColumn inputCol(e->table1, e->column1, colTypeInput1);
        BaseType::TableColumn partnerVal(e->table2, e->column2, colTypeInput2);
        BaseType::TableColumn outCol(BaseType::Table("MAP"), e->column1 + e->operatorType + e->column2, ColumnType::TYPE_INTEGER);
        node->irData = MapView::create(inputCol, op, partnerVal, outCol);
    }
    // WHERE / Filter Node
    else if (auto e = std::get_if<WhereClauseNode>(&ast->val)) {
        // LOOKUP: Get type for the Input Column
        ColumnType colType = Catalog::getSSBColumnType(e->table, e->column);
        BaseType::TableColumn inputCol(e->table, e->column, colType);
        std::optional<BaseType::TableColumn> col2 = std::nullopt;

        std::vector<std::variant<uint64_t, float, std::string>> filterArgs;
        CompType opType = IrTransformHelpers::mapStringToCompType(e->operatorType);

        // TODO: Or capabilities limited by ast parsing: Always or of two equalities
        if (e->operatorType == "OR") {
            opType = CompType::COMP_IN;

            filterArgs.push_back(IrTransformHelpers::parseValueByType(e->value, colType));
            if (!e->value2.empty()) {
                filterArgs.push_back(IrTransformHelpers::parseValueByType(e->value2, colType));
            }
        }
        else if (e->operatorType == "BETWEEN") {
            opType = CompType::COMP_BETWEEN;

            filterArgs.push_back(IrTransformHelpers::parseValueByType(e->value, colType));
            filterArgs.push_back(IrTransformHelpers::parseValueByType(e->value2, colType));
        }
        // Column based filter (or join)
        else if (!e->column.empty() && !e->column2.empty()) {
            std::string table2 = !e->table2.empty() ? e->table2 : e->table;
            ColumnType col2Type = Catalog::getSSBColumnType(table2, e->column2);
            col2 = BaseType::TableColumn(table2, e->column2, col2Type);
        }
        // Single value filter
        else {
            filterArgs.push_back(IrTransformHelpers::parseValueByType(e->value, colType));
        }

        node->irData = FilterView::create(
            inputCol,
            opType,
            col2,
            filterArgs,
            inputCol
        );

    }
    // JOIN Node
    else if (auto e = std::get_if<TableJoinNode>(&ast->val)) {
        ColumnType leftType = Catalog::getSSBColumnType(e->onLeftTable, e->onLeftTableColumn);

        // TODO: proper type inference as right value might not be column
        ColumnType rightType = Catalog::getSSBColumnType(e->onRightTable, e->onRightTableColumn);

        BaseType::TableColumn leftCol(e->onLeftTable, e->onLeftTableColumn, leftType);
        BaseType::TableColumn rightCol(e->onRightTable, e->onRightTableColumn, rightType);
        BaseType::TableColumn outCol(BaseType::Table("JOIN"), e->onLeftTableColumn + "=" + e->onRightTableColumn, ColumnType::TYPE_INTEGER);

        node->irData = JoinView::create(
            leftCol,
            rightCol,
            outCol,
            IrTransformHelpers::mapStringToJoinType(e->joinType),
            CompType::COMP_EQ
        );
    }
    // Limit (don't support Limit for now)
    // else if (auto e = std::get_if<LimitClauseNode>(&ast->val)) {
    //     node->irData = IR::LimitNode(e->limit, e->offset);
    // }
    // Set Ops
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
            IrTransformHelpers::mapStringToRelOp(e->setOperation)
        );
    }
    // Base Table
    else if (auto e = std::get_if<TableBaseNode>(&ast->val)) {
        BaseType::Table table(e->tableName, e->tableAlias);
        node->irData = FetchView::create(BaseType::TableColumn(table, "", ColumnType::TYPE_INTEGER), true);
    }

    // Recursion
    if (ast->left) {
        node->children.push_back(astToIr(ast->left));
    }
    if (ast->right) {
        node->children.push_back(astToIr(ast->right));
    }

    // Set Ops: set input columns;
    if (node->irData.is<SetOp>()) {
        std::vector<BaseType::TableColumn>& inputColumns = node->irData.inputColumns;
        inputColumns[0] = node->children[0]->irData.outputCols[0];
        inputColumns[1] = node->children[1]->irData.outputCols[0];

        // Propagate the type from the input to the output column (SetOps preserve type)
        node->irData.outputCols[0].columnType = inputColumns[0].columnType;
    }

    return node;
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

void irToApiDataSub(PlanNode* node, std::set<const PlanNode*>& visited) {
    if (!node || visited.contains(node)) return;

    static BaseType::TableColumn missingCol;

    // Filter Node
    if (auto filterV = node->irData.get_view_if<FilterView>()) {
        ItemBuilder::FilterNode filterStruct;
        filterStruct.inputColumn = &filterV->col1();
        filterStruct.outputColumn = &filterV->outputCol();
        filterStruct.filterType = filterV->filterType();
        filterStruct.filterArgVals = filterV->filterArgs();

        node->apiData = filterStruct;
    }

    // Join Node
    else if (auto joinV = node->irData.get_view_if<JoinView>()) {
        ItemBuilder::JoinNode joinStruct;

        joinStruct.innerColumn = &joinV->inner();
        joinStruct.outerColumn = &joinV->outer();
        joinStruct.iOutputColumn = &joinV->innerOut();
        joinStruct.oOutputColumn = &joinV->outerOut();
        joinStruct.joinPredicate = &joinV->joinPredicate();

        node->apiData = joinStruct;
    }

    // Semi Join Node
    else if (auto joinV = node->irData.get_view_if<SemiJoinView>()) {
        ItemBuilder::SemiJoinNode semiJoinStruct;

        semiJoinStruct.innerColumn = &joinV->inner();
        semiJoinStruct.outerColumn = &joinV->outer();
        semiJoinStruct.iOutputColumn = &joinV->innerOut();
        semiJoinStruct.oOutputColumn = &joinV->outerOut();
        semiJoinStruct.joinPredicate = &joinV->joinPredicate();

        node->apiData = semiJoinStruct;
    }

    // Map Node

    else if (auto mapV = node->irData.get_view_if<MapView>()) {
        ItemBuilder::MapNode mapStruct;

        mapStruct.inputColumn = &mapV->inputCol();
        mapStruct.outputColumn = &mapV->outputCol();
        mapStruct.operatorType = &mapV->operatorType();
        mapStruct.partnerVal = mapV->partnerVal();

        node->apiData = mapStruct;
    }

    // Group
    else if (auto groupV = node->irData.get_view_if<GroupView>()) {
        ItemBuilder::MultiGroupNode groupStruct;

        // Convert values to pointers
        for (auto& col : groupV->groupingCols()) {
            groupStruct.groupColumns.push_back(&col);
        }
        groupStruct.outputIdx = &groupV->outputIdx();
        groupStruct.outputSortIndex = &groupV->outputSortIdx();
        groupStruct.outputCluster = &groupV->outputCluster();
        if (auto aggCol = groupV->aggCol()) {
            if (auto aggResultCol = groupV->aggResultCol()) {
                groupStruct.aggColumn = aggCol;
                groupStruct.aggResultColumn = aggResultCol;
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

    // Aggregation
    else if (auto aggV = node->irData.get_view_if<AggView>()) {
        ItemBuilder::AggNode aggStruct;
        aggStruct.inputColumn = &aggV->colToAgg();
        aggStruct.outputColumn = &aggV->aggResultCol();
        aggStruct.aggFunc = aggV->aggFunc();
        // TODO: How to set group columns?
        aggStruct.groupColumns = {};

        node->apiData = aggStruct;
    }

    // Sort
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

    // Set Operation
    else if (auto setOpV = node->irData.get_view_if<SetOpView>()) {
        ItemBuilder::SetOperationNode setStruct;
        setStruct.operation = setOpV->operation();

        // IR stores inputs in a vector, Builder wants explicit pointers
        setStruct.innerColumn = &setOpV->innerCol();
        setStruct.outerColumn = &setOpV->outerCol();
        setStruct.outputColumn = &setOpV->outputCol();

        node->apiData = setStruct;
    }

    // Materialize
    else if (auto matV = node->irData.get_view_if<MaterializeView>()) {

        ItemBuilder::MaterializeNode matStruct;

        matStruct.idxColumn = &matV->idxCol();
        matStruct.filterColumn = &matV->filterCol();
        matStruct.outputColumn = &matV->outputCol();

        node->apiData = matStruct;
    }

    // Select
    else if (auto selectV = node->irData.get_view_if<SelectView>()) {
        ItemBuilder::ResultNode resultStruct;

        // MISSING INFO: Filename is not in IR.
        resultStruct.filename = "result.csv";

        for (auto& col : selectV->resultCols()) {
            resultStruct.resultColumns.push_back(&col);
        }
        resultStruct.resultHeaders = selectV->resultHeaders();
        resultStruct.resultIdx = selectV->resultIdx().has_value() ? &selectV->resultIdx().value() : &missingCol;

        node->apiData = resultStruct;
    }

    // Erase FetchNodes
    node->children.erase(std::remove_if(node->children.begin(), node->children.end(),
                                        [](const std::shared_ptr<PlanNode>& child) {
                                            return child->irData.is<FetchOp>();
                                        }),
                         node->children.end());

    for (const auto& child : node->children){
        irToApiData(child.get());
    }
}

void irToApiData(PlanNode* node) {
    std::set<const PlanNode*> visited;
    irToApiDataSub(node, visited);
}