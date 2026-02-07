#include "ir/plan_node_to_dot.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <algorithm>
#include <unordered_set>
#include "ir/ir_views.hpp"

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
        if (!col.table.name.empty()) {
            os << col.table.name << ".";
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

        // Output Columns
        if (!node.irData.outputCols.empty()) {
            ss << "Output: ";
            for(size_t i=0; i<node.irData.outputCols.size(); ++i) {
                ss << node.irData.outputCols[i] << (i < node.irData.outputCols.size() - 1 ? ", " : "");
            }
            ss << "\n----------------\n";
        }

        // Visitor for variant values (used in Filter/Map)
        auto streamVal = [&](const auto& val) { ss << val; };

        // Helper to get non-const ref for views
        IrData& mutableIr = const_cast<IrData&>(node.irData);

        if (node.irData.is<FetchOp>()) {
             FetchView view(mutableIr);
             ss << "Fetch: " << view.inputCol();
             if (view.wasTableBaseNode()) ss << "\n[TBN]";
        }
        else if (node.irData.is<SelectOp>()) {
            SelectView view(mutableIr);
            ss << "Select"; 
            if (view.resultIdx().has_value()){
                ss << "\n[Idx: " << view.resultIdx().value() << "]";
            }
            ss << "\n";

            for(size_t i=0; i<view.resultCols().size(); ++i) {
                ss << view.resultCols()[i];
                if (i < view.resultHeaders().size()) {
                    ss << " AS " << view.resultHeaders()[i];
                }
                ss << (i < view.resultCols().size() - 1 ? ", " : "");
            }
        }
        else if (node.irData.is<AggOp>()) {
            AggView view(mutableIr);
            ss << "Agg " << AggFunc_Name(view.aggFunc()) << "\n(" << view.colToAgg() << ")";
        }
        else if (node.irData.is<JoinOp>()) {
            JoinView view(mutableIr);
            ss << "Join " << joinTypeToString(view.joinType()) << "\nON "
               << view.inner() << " " << CompType_Name(view.joinPredicate())
               << " " << view.outer();
        }
        else if (node.irData.is<SemiJoinOp>()) {
            SemiJoinView view(mutableIr);
            ss << "SemiJoin " << joinTypeToString(view.joinType()) << "\nON "
               << view.inner() << " " << CompType_Name(view.joinPredicate())
               << " " << view.outer();
        }
        else if (node.irData.is<FilterOp>()) {
            FilterView view(mutableIr);
            ss << "Filter " << CompType_Name(view.filterType()) << "\n" << view.col1() << " ";

            if (view.col2() != nullptr) {
                ss << *view.col2();
            }
            else if (!view.filterArgs().empty()) {
                if (view.filterType() == CompType::COMP_BETWEEN && view.filterArgs().size() >= 2) {
                    std::visit(streamVal, view.filterArgs()[0]);
                    ss << " AND ";
                    std::visit(streamVal, view.filterArgs()[1]);
                }
                else if (view.filterType() == CompType::COMP_IN) {
                    ss << "(";
                    for (size_t i = 0; i < view.filterArgs().size(); ++i) {
                        std::visit(streamVal, view.filterArgs()[i]);
                        if (i < view.filterArgs().size() - 1) ss << ", ";
                    }
                    ss << ")";
                }
                else {
                    std::visit(streamVal, view.filterArgs()[0]);
                }
            }
        }
        else if (node.irData.is<GroupOp>()) {
            GroupView view(mutableIr);
            ss << "GroupBy (";
            const auto& cols = view.groupingCols();
            for (size_t i = 0; i < cols.size(); ++i) {
                ss << cols[i] << (i < cols.size() - 1? " " : "");
            }
            ss << ")";
            if(view.aggCol().has_value()){
                ss << "\nAgg: " << view.aggCol().value();
            }
        }
        else if (node.irData.is<SortOp>()) {
            SortOrderView view(mutableIr);
            ss << "Sort (";
            const auto& descs = view.orderDescriptions();
            for (size_t i = 0; i < descs.size(); ++i) {
                ss << descs[i].column << (i < descs.size() - 1 ? " " : "");
            }
            ss << ")";
        }
        else if (node.irData.is<MapOp>()) {
            MapView view(mutableIr);
            ss << "Map " << view.column() << " " << ArithOp_Name(view.operatorType()) << " ";
            std::visit(streamVal, view.partnerVal());
        }
        else if (node.irData.is<SetOp>()) {
            SetOpView view(mutableIr);
            ss << "SetOp [" << (int)view.operation() << "]";
        }
        else if (node.irData.is<MatOp>()) {
            MaterializeView view(mutableIr);
            ss << "Mat\nIdx: " << view.idxCol() << "\nFilter: " << view.filterCol();
        }
        else {
            ss << "[Empty/Unknown IR]";
        }

        // Input Columns
        if (!node.irData.inputColumns.empty()) {
            ss << "\n----------------\n";
             ss << "Input: ";
             for(size_t i=0; i<node.irData.inputColumns.size(); ++i) {
                ss << node.irData.inputColumns[i] << (i < node.irData.inputColumns.size() - 1 ? ", " : "");
             }
        }

        return escapeLabel(ss.str());
    }

    // --- API Label Generation ---
    std::string getApiLabel(const PlanNode& node) {
        std::stringstream ss;

         // Output Columns
        if (!node.irData.outputCols.empty()) {
            ss << "Output: ";
            for(size_t i=0; i<node.irData.outputCols.size(); ++i) {
                ss << node.irData.outputCols[i] << (i < node.irData.outputCols.size() - 1 ? ", " : "");
            }
            ss << "\n----------------\n";
        }

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
                ss << "iOut: " << data.iOutputColumn << "\n";
                ss << "oOut: " << data.oOutputColumn;
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::SemiJoinNode>) {
                ss << "API SemiJoin\nInner: " << data.innerColumn << "\n";
                ss << "Outer: " << data.outerColumn << "\n";
                ss << "iOut: " << data.iOutputColumn << "\n";
                ss << "oOut: " << data.oOutputColumn;
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
                ss << ")\nAgg: " << data.aggColumn << "\n";
                ss << "SrtIdx: " << data.outputSortIndex;
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


        // Input Columns
        if (!node.irData.inputColumns.empty()) {
            ss << "\n----------------\n";
             ss << "Input: ";
             for(size_t i=0; i<node.irData.inputColumns.size(); ++i) {
                ss << node.irData.inputColumns[i] << (i < node.irData.inputColumns.size() - 1 ? ", " : "");
             }
        }

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
