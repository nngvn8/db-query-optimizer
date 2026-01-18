#include "plan_node_to_dot.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <algorithm>
#include <unordered_set>

// --- Helper Functions (Local) ---

namespace {

    // Helper to escape characters for DOT labels
    std::string escapeLabel(const std::string& label) {
        std::string escaped;
        for (char c : label) {
            if (c == '"') {
                escaped += "\\\"";
            } else if (c == '\\') {
                escaped += "\\\\";
            } else if (c == '\n') {
                escaped += "\\n";
            } else {
                escaped += c;
            }
        }
        return escaped;
    }

    // Reuse printing logic for TableColumn
    std::ostream& operator<<(std::ostream& os, const BaseType::TableColumn& col) {
        if (!col.tableName.empty()) {
            os << col.tableName << ".";
        }
        os << col.columnName;
        if (col.alias.has_value()) {
            os << " AS " << col.alias.value();
        }
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const BaseType::TableColumn* col) {
        if (col) {
            os << *col;
        } else {
            os << "NULL";
        }
        return os;
    }

    std::string joinTypeToString(const BaseType::Join& join) {
        switch(join) {
            case BaseType::INNER_JOIN: return "INNER";
            case BaseType::LEFT_OUTER_JOIN: return "LEFT";
            case BaseType::RIGHT_OUTER_JOIN: return "RIGHT";
            case BaseType::FULL_OUTER_JOIN: return "FULL";
            default: return "UNKNOWN_JOIN";
        }
    }

    // --- IR Label Generation ---
    std::string getIrLabel(const PlanNode& node) {
        std::stringstream ss;
        
        // Visitor for variant values (used in Filter/Map)
        auto streamVal = [&](const auto& val) { ss << val; };

        if (const auto* n = std::get_if<IR::TableBaseNode>(&node.irData)) {
            ss << "Table: " << n->table.name;
            ss << "\n(";
            for (size_t i = 0; i < n->inputColumns.size(); ++i) {
                ss << n->inputColumns[i].columnName;
                if (i < n->inputColumns.size() - 1) ss << ", ";
            }
            ss << ")";
        }
        else if (const auto* n = std::get_if<IR::FetchNode>(&node.irData)) {
             ss << "Fetch: " << n->column();
        }
        else if (const auto* n = std::get_if<IR::SelectNode>(&node.irData)) {
            ss << "Select " << (n->distinct ? "DISTINCT " : "") 
               << (n->star ? "*" : "") << "\n" << n->column();
        }
        else if (const auto* n = std::get_if<IR::AggNode>(&node.irData)) {
            ss << "Agg " << AggFunc_Name(n->aggFunc) << "\n(" << n->column() << ")";
        }
        else if (const auto* n = std::get_if<IR::JoinNode>(&node.irData)) {
            ss << "Join " << joinTypeToString(n->joinType) << "\nON " 
               << n->leftTableColumn() << " " << CompType_Name(n->joinPredicate) 
               << " " << n->rightTableColumn();
        }
        else if (const auto* n = std::get_if<IR::FilterNode>(&node.irData)) {
            ss << "Filter " << CompType_Name(n->filterType) << "\n" << n->column1() << " ";

            if (n->column2().has_value()) {
                ss << n->column2().value();
            } 
            else if (!n->filterArgs.empty()) {
                if (n->filterType == CompType::COMP_BETWEEN && n->filterArgs.size() >= 2) {
                    std::visit(streamVal, n->filterArgs[0]);
                    ss << " AND ";
                    std::visit(streamVal, n->filterArgs[1]);
                }
                else if (n->filterType == CompType::COMP_IN) {
                    ss << "(";
                    for (size_t i = 0; i < n->filterArgs.size(); ++i) {
                        std::visit(streamVal, n->filterArgs[i]);
                        if (i < n->filterArgs.size() - 1) ss << ", ";
                    }
                    ss << ")";
                }
                else {
                    std::visit(streamVal, n->filterArgs[0]);
                }
            }
        }
        else if (const auto* n = std::get_if<IR::GroupByNode>(&node.irData)) {
            ss << "GroupBy (";
            const auto& desc = n->description();
            for (size_t i = 0; i < desc.size(); ++i) {
                ss << desc[i] << (i < desc.size() - 1? " " : "");
            }
            ss << ")";
        }
        else if (const auto* n = std::get_if<IR::SortOrderNode>(&node.irData)) {
            ss << "Sort (";
            for (size_t i = 0; i < n->columnList.size(); ++i) {
                ss << n->columnList[i].column << (i < n->columnList.size() - 1 ? " " : "");
            }
            ss << ")";
        }
        else if (const auto* n = std::get_if<IR::MapNode>(&node.irData)) {
            ss << "Map " << n->column() << " " << ArithOp_Name(n->operatorType) << " ";
            std::visit(streamVal, n->partnerVal);
        }
        else if (const auto* n = std::get_if<IR::LimitNode>(&node.irData)) {
            ss << "Limit " << n->limit << " Offset " << n->offset;
        }
        else if (const auto* n = std::get_if<IR::ResultNode>(&node.irData)) {
            ss << "Result -> " << n->fileName;
        }
        else if (std::get_if<IR::UpdateNode>(&node.irData)) ss << "Update";
        else if (std::get_if<IR::InsertNode>(&node.irData)) ss << "Insert";
        else if (std::get_if<IR::DeleteNode>(&node.irData)) ss << "Delete";
        else if (const auto* n = std::get_if<IR::MaterializeNode>(&node.irData)) {
            ss << "Mat\nIdx: " << n->column() << "\nFilter: " << n->column2();
        }
        else if (std::get_if<IR::PositionList>(&node.irData)) ss << "PosList";
        else if (std::get_if<IR::Bitmap>(&node.irData)) ss << "Bitmap";
        else {
            ss << "[Empty/Unknown IR]";
        }

        return escapeLabel(ss.str());
    }

    // --- API Label Generation ---
    std::string getApiLabel(const PlanNode& node) {
        std::stringstream ss;
        auto streamVal = [&](const auto& val) { ss << val; };

        std::visit([&](auto&& data) {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, std::monostate>) {
                ss << "[Empty API]";
            }
            else if constexpr (std::is_same_v<T, std::vector<ItemBuilder::FetchNode>>) {
                ss << "API Fetch\n(";
                for (size_t i = 0; i < data.size(); ++i) {
                    ss << data[i].inputColumn;
                    if (data[i].printToFile) ss << "[f]";
                    if (i < data.size() - 1) ss << ", ";
                }
                ss << ")";
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::FetchNode>) {
                ss << "API Fetch " << data.inputColumn << (data.printToFile ? "[f]" : "");
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::FilterNode>) {
                ss << "API Filter\n";
                ss << data.inputColumn << " -> " << data.outputColumn << "\n";
                ss << "[" << CompType_Name(data.filterType) << "] (";
                for (size_t i = 0; i < data.filterArgVals.size(); ++i) {
                    std::visit(streamVal, data.filterArgVals[i]);
                    if (i < data.filterArgVals.size() - 1) ss << ", ";
                }
                ss << ")";
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::JoinNode>) {
                ss << "API Join\nInner: " << data.innerColumn << "\n";
                ss << "Outer: " << data.outerColumn << "\n";
                ss << "Out: " << data.outputColumn;
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::MapNode>) {
                ss << "API Map\n";
                ss << data.inputColumn << " -> " << data.outputColumn;
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::MaterializeNode>) {
                ss << "API Mat\nIdx: " << data.idxColumn << "\nFilter: " << data.filterColumn;
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::MultiGroupNode>) {
                ss << "API MultiGroup\n(";
                for (size_t i = 0; i < data.groupColumns.size(); ++i) {
                    ss << data.groupColumns[i];
                    if (i < data.groupColumns.size() - 1) ss << ", ";
                }
                ss << ")\nAgg: " << data.aggColumn;
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::SetOperationNode>) {
                ss << "API SetOp [" << (int)data.operation << "]";
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::SortNode>) {
                ss << "API Sort\n(";
                for (size_t i = 0; i < data.inputColumns.size(); ++i) {
                    ss << data.inputColumns[i];
                    if (i < data.inputColumns.size() - 1) ss << ", ";
                }
                ss << ")";
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::AggNode>) {
                ss << "API Agg " << AggFunc_Name(data.aggFunc) << "\n(";
                for (size_t i = 0; i < data.groupColumns.size(); ++i) {
                    ss << data.groupColumns[i] << (i < data.groupColumns.size() - 1 ? ", " : "");
                }
                ss << ")";
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::ResultNode>) {
                ss << "API Result\n-> " << data.filename;
            }
        }, node.apiData);

        return escapeLabel(ss.str());
    }

    void writePlanNodeDot(const PlanNode* node, std::ofstream& file, DotContentType contentType, std::unordered_set<const PlanNode*>& visited) {
        if (!node) return;
        
        // Handle DAG/Cycles: If already visited, stop.
        if (visited.count(node)) return;
        visited.insert(node);

        std::ostringstream id;
        id << reinterpret_cast<std::uintptr_t>(node);

        std::string label;
        if (contentType == DotContentType::IR_DATA) {
            label = getIrLabel(*node);
        } else {
            label = getApiLabel(*node);
        }

        file << "    " << id.str() << " [label=\"" << label << "\"];\n";

        for (const auto& child : node->children) {
            if (child) {
                std::ostringstream childId;
                childId << reinterpret_cast<std::uintptr_t>(child.get());
                
                // Write Edge
                file << "    " << id.str() << " -> " << childId.str() << ";\n";
                
                // Recurse
                writePlanNodeDot(child.get(), file, contentType, visited);
            }
        }
    }
} // namespace

void generatePlanDotFile(const PlanNode& root, const std::string& filename, DotContentType contentType) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file " << filename << std::endl;
        return;
    }

    file << "digraph PlanNode {\n";
    file << "    node [shape=box, fontname=\"Helvetica\"];\n";
    
    std::unordered_set<const PlanNode*> visited;
    writePlanNodeDot(&root, file, contentType, visited);

    file << "}\n";
    file.close();
}
