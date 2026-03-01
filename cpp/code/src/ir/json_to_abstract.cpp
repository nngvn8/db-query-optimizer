#include "ir/ir_transformer.hpp"

#include <memory>
#include <unordered_set>
#include <regex>
#include <algorithm>

#include "util/catalog.hpp"
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

        // minimal trim
        left.erase(0, left.find_first_not_of(" 	"));
        left.erase(left.find_last_not_of(" 	") + 1);
        right.erase(0, right.find_first_not_of(" 	"));
        right.erase(right.find_last_not_of(" 	") + 1);

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
