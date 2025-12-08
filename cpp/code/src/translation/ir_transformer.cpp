#include "ir_transformer.hpp"

#include <memory>
#include <unordered_set>


namespace {
    const std::unordered_set<std::string> HASH = {"Hash"};
    const std::unordered_set<std::string> GATHER = {"Gather", "Gather Merge"};
    const std::unordered_set<std::string> JOIN = {"Nested Loop", "Hash Join"};
    const std::unordered_set<std::string> AGG = {"Aggregate"};
    const std::unordered_set<std::string> BITMAP = {"Bitmap Heap Scan", "Bitmap Index Scan"};
    const std::unordered_set<std::string> SCAN = {"Index Scan", "Seq Scan"};
    const std::unordered_set<std::string> SORT = {"Incremental Sort", "Sort"};

    const std::unordered_set<std::string> PRUNE_TARGETS = {"Hash", "Gather", "Gather Merge"};
}


bool isNodeType(const PlanNode* node, const std::string& type) {
    return node && node->rawJson.has_value() && node->rawJson->nodeType == type;
}

bool isNodeType(const PlanNode* node, const std::unordered_set<std::string>& types) {
    return node && node->rawJson.has_value() && types.count(node->rawJson->nodeType);
}

PlanNode::AbstractData convertToAbstract(const JsonRawData& rawJson) {
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

        // source.basetable = rawJson.planParams.baseTable.has_value() 
        //          ? rawJson.planParams.baseTable->fullName 
        //          : "";
        abstract_data = source;
    }
    else if (rawJson.nodeType == "Limit") {
        AbstractResult result;
        abstract_data = result;
    }
    return abstract_data;
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


    // 2. CHECK PRUNING LOGIC
    // Access the raw data to check the type

    // Remove if Prune Target
    if (HASH.count(currentType) || GATHER.count(currentType)) {
        // THE REWIRE TRICK:
        // 1. We detach the first child from the current 'node'.
        // 2. We return that child to the *caller* (the parent of 'node').
        // 3. 'node' itself (the Hash/Gather) goes out of scope here and is deleted.
        if (node->children.size() == 1) {
            return std::move(node->children[0]);
        }
    }


    // Handling the aggregate Sandwich
    if (currentType == "Aggregate" && node->children.size() == 1) {
        PlanNode* childPtr = node->children[0].get();
        
        if (isNodeType(childPtr, "Aggregate")) {
                // move handles empty lists
                // std::unique_ptr<PlanNode> childNode = std::move(node->children[0]);
                node->children = std::move(childPtr->children);
        }
    }

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
    // 3. TRANSFORM RAW TO ABSTRACT (In-place) // TODO: this line is not working yet
    // If we didn't prune it, we convert Raw Data -> Abstract Data here
    node->abstractData = convertToAbstract(node->rawJson.value());

    return node; // Return the modified (but same pointer) node
}

