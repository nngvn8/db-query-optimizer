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

// void uniqueColNamesSub(PlanNode* node, std::set<const PlanNode*>& visited, NameGenerator& nameGenerator, std::map<BaseType::TableColumn, int>& colNamesMap) {
//     if (visited.contains(node)) return;

//     visited.insert(node);

//     for (const auto& child : node->children) {
//         uniqueColNamesSub(child.get(), visited, nameGenerator, colNamesMap);
//     }

//     if (node->children.empty()) {
//         for (auto& col : node->irData.outputCols) {
//             colNamesMap[col] = nameGenerator.next_int();
//             col.columnName = nameGenerator.cur(col.columnName);
//         }
//     }
//     else {
//         for (auto& col : node->irData.inputColumns) {
//             col.columnName = col.columnName + "_" + std::to_string(colNamesMap[col]);
//         }
//         for (auto& col : node->irData.outputCols) {
//             colNamesMap[col] = nameGenerator.next_int();
//             col.columnName = nameGenerator.cur(col.columnName);
//         }
//     }
// }

inline std::string getRawKey(const BaseType::TableColumn& col) {
    return col.columnName;
}

class Renamer {
    NameGenerator& gen;
    // Cache: Node -> { OriginalName -> UniqueName }
    std::map<const PlanNode*, std::map<std::string, std::string>> nodeOutputRegistry;

public:
    Renamer(NameGenerator& g) : gen(g) {}

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

inline void uniqueColNames(PlanNode* node) {
    NameGenerator nameGenerator;
    Renamer renamer(nameGenerator);
    renamer.process(node);
}