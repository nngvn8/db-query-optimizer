/**
 * @file unique_col_names.hpp
 * @brief Provides functionality to rename columns in a query plan to ensure uniqueness.
 * This is crucial for query execution engines where ambiguous column names can lead to incorrect results.
 */

#include <vector>
#include <set>
#include <map>
#include <string>
#include <deque>
#include <memory>

#include <memory>
#include "ir/plan_node.hpp"
#include "ir/ir_views.hpp"
#include "util/name_generator.hpp"

/**
 * @brief Extracts the raw column name from a TableColumn object.
 * @param col The TableColumn object.
 * @return The raw column name as a string.
 */
inline std::string getRawKey(const BaseType::TableColumn& col) {
    return col.columnName;
}

/**
 * @class Renamer
 * @brief A class that traverses a query plan and renames columns to ensure uniqueness.
 *
 * The Renamer uses a NameGenerator to create new, unique names for columns.
 * It caches the renamed columns for each node to avoid redundant processing.
 */
class Renamer {
    NameGenerator& gen;
    // Cache: Node -> { OriginalName -> UniqueName }
    std::map<const PlanNode*, std::map<std::string, std::string>> nodeOutputRegistry;

public:
    /**
     * @brief Constructs a Renamer object.
     * @param g A reference to a NameGenerator object.
     */
    Renamer(NameGenerator& g) : gen(g) {}

    /**
     * @brief Processes a query plan node to rename its columns.
     *
     * This method recursively processes the children of the given node,
     * then renames the input and output columns of the current node.
     *
     * @param node A pointer to the PlanNode to process.
     * @return A map from original column names to their new unique names.
     */
    std::map<std::string, std::string> process(PlanNode* node) {
        // Check if already renamed output cols
        if (nodeOutputRegistry.count(node)) {
            return nodeOutputRegistry[node];
        }

        // Don't assign integers to FetchNodes as going to be deleted in IrData
        if (node->irData.is<FetchOp>()) {
            std::map<std::string, std::string> fetchRename;
            std::string fetchColName(getRawKey(node->irData.inputColumns[0]));
            fetchRename[fetchColName] = fetchColName;
            return fetchRename;
        }

        // Collect inputs from children preserving order
        // Map: "d_year" -> ["d_year_0", "d_year_1"]
        std::map<std::string, std::deque<std::string>> inputCandidates;

        for (const auto& child : node->children) {
            auto childOutputs = process(child.get());

            for (const auto& [key, uniqueName] : childOutputs) {
                inputCandidates[key].push_back(uniqueName);
            }
        }

        // Assign outputs to inputs (FiFo)
        if (!node->children.empty()) {
            for (auto& col : node->irData.inputColumns) {
                std::string rawKey = getRawKey(col);

                if (inputCandidates.count(rawKey) && !inputCandidates[rawKey].empty()) {
                    auto& q = inputCandidates[rawKey];

                    // Assign the version at the front of the queue
                    col.columnName = q.front();

                    // If we have multiple distinct versions (collision), consume this one.
                    // If we only have one version (sharing), keep it for the next input to use.
                    if (q.size() > 1) {
                        q.pop_front();
                    }
                }
            }
        }

        // Generate outputs
        std::map<std::string, std::string> myOutputs;
        for (auto& col : node->irData.outputCols) {
            std::string rawKey = getRawKey(col);
            std::string newName = gen.next(col.columnName);

            col.columnName = newName;
            myOutputs[rawKey] = newName;
        }

        // Cache results
        nodeOutputRegistry[node] = myOutputs;
        return myOutputs;
    }
};

/**
 * @brief A convenience function that renames all columns in a query plan to be unique.
 * @param node The root of the query plan to process.
 */
inline void uniqueColNames(PlanNode* node) {
    NameGenerator nameGenerator;
    Renamer renamer(nameGenerator);
    renamer.process(node);
}
