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

    std::string escapeHtml(const std::string& text) {
        std::string escaped;
        for (char c : text) {
            if (c == '<') escaped += "&lt;";
            else if (c == '>') escaped += "&gt;";
            else if (c == '&') escaped += "&amp;";
            else if (c == '"') escaped += "&quot;";
            else escaped += c;
        }
        return escaped;
    }

    struct ColumnPrinter {
        const BaseType::TableColumn* col;
        bool shortMode;
        ColumnPrinter(const BaseType::TableColumn& c, bool s) : col(&c), shortMode(s) {}
        ColumnPrinter(const BaseType::TableColumn* c, bool s) : col(c), shortMode(s) {}
    };

    std::ostream& operator<<(std::ostream& os, const ColumnPrinter& cp) {
        if (!cp.col) {
            os << "NULL";
            return os;
        }
        if (!cp.shortMode && !cp.col->table.name.empty()) {
            os << cp.col->table.name << ".";
        }
        os << cp.col->columnName;
        if (cp.col->alias.has_value()) {
            os << " AS " << cp.col->alias.value();
        }
        return os;
    }

    std::string getCompSymbol(CompType type) {
        switch (type) {
            case COMP_EQ: return "=";
            case COMP_NE: return "!=";
            case COMP_LT: return "<";
            case COMP_LE: return "<=";
            case COMP_GT: return ">";
            case COMP_GE: return ">=";
            case COMP_IN: return "IN";
            case COMP_BETWEEN: return "BETWEEN";
            default: return CompType_Name(type);
        }
    }

    std::string getArithSymbol(ArithOp op) {
        switch (op) {
            case ARITH_ADD: return "+";
            case ARITH_SUB: return "-";
            case ARITH_MUL: return "*";
            case ARITH_DIV: return "/";
            case ARITH_MOD: return "%";
            default: return ArithOp_Name(op);
        }
    }

    class HtmlLabelBuilder {
        std::string title;
        std::vector<std::string> details;
        std::string inputStr;
        std::string outputStr;

    public:
        void setTitle(const std::string& t) { title = escapeHtml(t); }
        
        void addDetail(const std::string& d) { details.push_back(escapeHtml(d)); }
        void addRawDetail(const std::string& d) { details.push_back(d); }
        
        void setInput(const std::string& i) { inputStr = escapeHtml(i); }
        void setOutput(const std::string& o) { outputStr = escapeHtml(o); }

        std::string build() const {
            std::stringstream ss;
            ss << "<\n";
            ss << "        <TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
            
            if (!outputStr.empty()) {
                ss << "            <TR><TD ALIGN=\"LEFT\">Output: " << outputStr << "</TD></TR>\n";
                ss << "            <HR/>\n";
            }

            ss << "            <TR><TD ALIGN=\"CENTER\"><FONT POINT-SIZE=\"16\"><B>" << title << "</B></FONT>";
            for (const auto& d : details) {
                ss << "<BR/>" << d;
            }
            ss << "</TD></TR>\n";

            if (!inputStr.empty()) {
                ss << "            <HR/>\n";
                ss << "            <TR><TD ALIGN=\"LEFT\">Input: " << inputStr << "</TD></TR>\n";
            }

            ss << "        </TABLE>\n";
            ss << "    >";
            return ss.str();
        }
    };

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

            for(size_t i=0; i<view.resultHeaders().size(); ++i) {
                ss << view.resultHeaders()[i] << (i < view.resultHeaders().size() - 1 ? ", " : "");
            }
        }
        else if (node.irData.is<AggOp>()) {
            AggView view(mutableIr);
            ss << "Agg " << AggFunc_Name(view.aggFunc()) << "\n(" << view.colToAgg() << ")";
        }
        else if (node.irData.is<JoinOp>()) {
            JoinView view(mutableIr);
            ss << "Join " << joinTypeToString(view.joinType()) << "\nON "
               << view.inner() << " " << getCompSymbol(view.joinPredicate())
               << " " << view.outer();
        }
        else if (node.irData.is<SemiJoinOp>()) {
            SemiJoinView view(mutableIr);
            ss << "SemiJoin " << joinTypeToString(view.joinType()) << "\nON "
               << view.inner() << " " << getCompSymbol(view.joinPredicate())
               << " " << view.outer();
        }
        else if (node.irData.is<FilterOp>()) {
            FilterView view(mutableIr);
            ss << "Filter " << view.col1() << " ";
            ss << getCompSymbol(view.filterType()) << " ";

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
            if(auto col = view.aggCol()){
                ss << "\nAgg: " << *col;
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
            ss << "Map " << view.inputCol() << " " << getArithSymbol(view.operatorType()) << " ";
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

        if (node.irData.outputsPosList) ss << "\nPOSLIST-OUT";
        if (node.irData.outputsMatVals) ss << "\nMATDATA-OUT";

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

    std::string getIrLabelV2(const PlanNode& node, bool shortColumns) {
        HtmlLabelBuilder builder;
        IrData& mutableIr = const_cast<IrData&>(node.irData);

        // Outputs
        if (!node.irData.outputCols.empty()) {
            std::stringstream ss;
            for(size_t i=0; i<node.irData.outputCols.size(); ++i) {
                ss << ColumnPrinter(node.irData.outputCols[i], shortColumns) << (i < node.irData.outputCols.size() - 1 ? ", " : "");
            }
            builder.setOutput(ss.str());
        }

        // Inputs
        if (!node.irData.inputColumns.empty()) {
            std::stringstream ss;
            for(size_t i=0; i<node.irData.inputColumns.size(); ++i) {
                ss << node.irData.inputColumns[i] << (i < node.irData.inputColumns.size() - 1 ? ", " : "");
            }
            builder.setInput(ss.str());
        }

        if (node.irData.is<FetchOp>()) {
             FetchView view(mutableIr);
             builder.setTitle("IR Fetch");
             std::stringstream ss;
             ss << ColumnPrinter(view.inputCol(), shortColumns);
             if (view.wasTableBaseNode()) ss << " [TBN]";
             builder.addDetail(ss.str());
        }
        else if (node.irData.is<SelectOp>()) {
            SelectView view(mutableIr);
            builder.setTitle("IR Select");
            if (view.resultIdx().has_value()){
                std::stringstream ss; ss << "Idx: " << ColumnPrinter(view.resultIdx().value(), shortColumns); builder.addDetail(ss.str());
            }
            
            std::stringstream ss;
            for(size_t i=0; i<view.resultHeaders().size(); ++i) {
                ss << view.resultHeaders()[i] << (i < view.resultHeaders().size() - 1 ? ", " : "");
            }
            builder.addDetail(ss.str());
        }
        else if (node.irData.is<AggOp>()) {
            AggView view(mutableIr);
            builder.setTitle("IR Agg " + AggFunc_Name(view.aggFunc()));
            std::stringstream ss;
            ss << "(" << view.colToAgg() << ")";
            builder.addDetail(ss.str());
        }
        else if (node.irData.is<JoinOp>()) {
            JoinView view(mutableIr);
            builder.setTitle("IR Join " + joinTypeToString(view.joinType()));
            std::stringstream ss;
            ss << "ON " << ColumnPrinter(view.inner(), shortColumns) << " " << getCompSymbol(view.joinPredicate()) << " " << ColumnPrinter(view.outer(), shortColumns);
            builder.addDetail(ss.str());
        }
        else if (node.irData.is<SemiJoinOp>()) {
            SemiJoinView view(mutableIr);
            builder.setTitle("IR SemiJoin " + joinTypeToString(view.joinType()));
            std::stringstream ss;
            ss << "ON " << ColumnPrinter(view.inner(), shortColumns) << " " << getCompSymbol(view.joinPredicate()) << " " << ColumnPrinter(view.outer(), shortColumns);
            builder.addDetail(ss.str());
        }
        else if (node.irData.is<FilterOp>()) {
            FilterView view(mutableIr);
            builder.setTitle("IR Filter");
            std::stringstream ss;
            ss << ColumnPrinter(view.col1(), shortColumns) << " ";
            if (view.col2() != nullptr) {
                ss << getCompSymbol(view.filterType()) << " " << ColumnPrinter(*view.col2(), shortColumns);
            } else if (!view.filterArgs().empty()) {
                if (view.filterType() == CompType::COMP_BETWEEN && view.filterArgs().size() >= 2) {
                    ss << "BETWEEN ";
                    std::visit([&](const auto& v){ ss << v; }, view.filterArgs()[0]);
                    ss << " AND ";
                    std::visit([&](const auto& v){ ss << v; }, view.filterArgs()[1]);
                }
                else if (view.filterType() == CompType::COMP_IN) {
                    ss << "IN (";
                    for (size_t i = 0; i < view.filterArgs().size(); ++i) {
                        std::visit([&](const auto& v){ ss << v; }, view.filterArgs()[i]);
                        if (i < view.filterArgs().size() - 1) ss << ", ";
                    }
                    ss << ")";
                }
                else {
                    ss << getCompSymbol(view.filterType()) << " ";
                    std::visit([&](const auto& v){ ss << v; }, view.filterArgs()[0]);
                }
            }
            builder.addDetail(ss.str());
        }
        else if (node.irData.is<GroupOp>()) {
            GroupView view(mutableIr);
            builder.setTitle("IR GroupBy");
            std::stringstream ss;
            ss << "(";
            const auto& cols = view.groupingCols();
            for (size_t i = 0; i < cols.size(); ++i) {
                ss << ColumnPrinter(cols[i], shortColumns) << (i < cols.size() - 1? " " : "");
            }
            ss << ")";
            builder.addDetail(ss.str());
            if(auto col = view.aggCol()){
                std::stringstream ss2;
                ss2 << "Agg: " << ColumnPrinter(*col, shortColumns);
                builder.addDetail(ss2.str());
            }
        }
        else if (node.irData.is<SortOp>()) {
            SortOrderView view(mutableIr);
            builder.setTitle("IR Sort");
            std::stringstream ss;
            ss << "(";
            const auto& descs = view.orderDescriptions();
            for (size_t i = 0; i < descs.size(); ++i) {
                ss << ColumnPrinter(descs[i].column, shortColumns) << (i < descs.size() - 1 ? " " : "");
            }
            ss << ")";
            builder.addDetail(ss.str());
        }
        else if (node.irData.is<MapOp>()) {
            MapView view(mutableIr);
            builder.setTitle("IR Map");
            std::stringstream ss;
            ss << ColumnPrinter(view.inputCol(), shortColumns) << " " << getArithSymbol(view.operatorType()) << " ";
            std::visit([&](const auto& v){ ss << v; }, view.partnerVal());
            builder.addDetail(ss.str());
        }
        else if (node.irData.is<SetOp>()) {
            SetOpView view(mutableIr);
            builder.setTitle("IR SetOp");
            builder.addDetail("[" + std::to_string((int)view.operation()) + "]");
        }
        else if (node.irData.is<MatOp>()) {
            MaterializeView view(mutableIr);
            builder.setTitle("IR Mat");
            std::stringstream ss; ss << "Idx: " << ColumnPrinter(view.idxCol(), shortColumns); builder.addDetail(ss.str());
            std::stringstream ss2; ss2 << "Filter: " << ColumnPrinter(view.filterCol(), shortColumns); builder.addDetail(ss2.str());
        }
        else {
            builder.setTitle("Empty/Unknown IR");
        }

        if (node.irData.outputsPosList) builder.addRawDetail("<FONT COLOR=\"blue\">POSLIST-OUT</FONT>");
        if (node.irData.outputsMatVals) builder.addRawDetail("<FONT COLOR=\"blue\">MATDATA-OUT</FONT>");

        return builder.build();
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
                ss << "[" << getCompSymbol(data.filterType) << "] (";
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
                ss << data.outputColumn;
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
                ss << data.inputColumn;
                ss << ")";
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::ResultNode>) {
                ss << "API Result";
                if (node.irData.is<SelectOp>()) {
                     SelectView view(const_cast<IrData&>(node.irData));
                     ss << "\n";
                     for(size_t i=0; i<view.resultHeaders().size(); ++i) {
                        ss << view.resultHeaders()[i] << (i < view.resultHeaders().size() - 1 ? ", " : "");
                     }
                } else {
                    ss << "\n-> " << data.filename;
                }
            }
        }, node.apiData);

        if (node.irData.outputsPosList) ss << "\nPOSLIST-OUT";
        if (node.irData.outputsMatVals) ss << "\nMATDATA-OUT";

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

    std::string getApiLabelV2(const PlanNode& node, bool shortColumns) {
        HtmlLabelBuilder builder;

        // Outputs
        if (!node.irData.outputCols.empty()) {
            std::stringstream ss;
            for(size_t i=0; i<node.irData.outputCols.size(); ++i) {
                ss << ColumnPrinter(node.irData.outputCols[i], shortColumns) << (i < node.irData.outputCols.size() - 1 ? ", " : "");
            }
            builder.setOutput(ss.str());
        }

        // Inputs
        if (!node.irData.inputColumns.empty()) {
            std::stringstream ss;
            for(size_t i=0; i<node.irData.inputColumns.size(); ++i) {
                ss << ColumnPrinter(node.irData.inputColumns[i], shortColumns) << (i < node.irData.inputColumns.size() - 1 ? ", " : "");
            }
            builder.setInput(ss.str());
        }

        std::visit([&](auto&& data) {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, std::monostate>) {
                builder.setTitle("Empty API");
            }
            else if constexpr (std::is_same_v<T, std::vector<ItemBuilder::FetchNode>>) {
                builder.setTitle("API Fetch");
                std::stringstream ss;
                ss << "(";
                for (size_t i = 0; i < data.size(); ++i) {
                    ss << ColumnPrinter(data[i].inputColumn, shortColumns);
                    if (data[i].printToFile) ss << "[f]";
                    if (i < data.size() - 1) ss << ", ";
                }
                ss << ")";
                builder.addDetail(ss.str());
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::FetchNode>) {
                builder.setTitle("API Fetch");
                std::stringstream ss;
                ss << ColumnPrinter(data.inputColumn, shortColumns) << (data.printToFile ? "[f]" : "");
                builder.addDetail(ss.str());
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::FilterNode>) {
                builder.setTitle("API Filter");
                std::stringstream ss;
                ss << ColumnPrinter(data.inputColumn, shortColumns) << " ";
                
                if (data.filterType == CompType::COMP_BETWEEN && data.filterArgVals.size() >= 2) {
                     ss << "BETWEEN ";
                     std::visit([&](const auto& v){ ss << v; }, data.filterArgVals[0]);
                     ss << " AND ";
                     std::visit([&](const auto& v){ ss << v; }, data.filterArgVals[1]);
                }
                else if (data.filterType == CompType::COMP_IN) {
                     ss << "IN (";
                     for (size_t i = 0; i < data.filterArgVals.size(); ++i) {
                         std::visit([&](const auto& v){ ss << v; }, data.filterArgVals[i]);
                         if (i < data.filterArgVals.size() - 1) ss << ", ";
                     }
                     ss << ")";
                }
                else {
                    ss << getCompSymbol(data.filterType) << " ";
                    if (!data.filterArgVals.empty()) {
                         std::visit([&](const auto& v){ ss << v; }, data.filterArgVals[0]);
                    }
                }
                builder.addDetail(ss.str());
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::JoinNode>) {
                builder.setTitle("API Join");
                std::stringstream ss; ss << "Inner: " << ColumnPrinter(data.innerColumn, shortColumns); builder.addDetail(ss.str());
                std::stringstream ss2; ss2 << "Outer: " << ColumnPrinter(data.outerColumn, shortColumns); builder.addDetail(ss2.str());
                std::stringstream ss3; ss3 << "iOut: " << ColumnPrinter(data.iOutputColumn, shortColumns); builder.addDetail(ss3.str());
                std::stringstream ss4; ss4 << "oOut: " << ColumnPrinter(data.oOutputColumn, shortColumns); builder.addDetail(ss4.str());
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::SemiJoinNode>) {
                builder.setTitle("API SemiJoin");
                std::stringstream ss; ss << "Inner: " << ColumnPrinter(data.innerColumn, shortColumns); builder.addDetail(ss.str());
                std::stringstream ss2; ss2 << "Outer: " << ColumnPrinter(data.outerColumn, shortColumns); builder.addDetail(ss2.str());
                std::stringstream ss3; ss3 << "iOut: " << ColumnPrinter(data.iOutputColumn, shortColumns); builder.addDetail(ss3.str());
                std::stringstream ss4; ss4 << "oOut: " << ColumnPrinter(data.oOutputColumn, shortColumns); builder.addDetail(ss4.str());
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::MapNode>) {
                builder.setTitle("API Map");
                std::stringstream ss;
                ss << ColumnPrinter(data.outputColumn, shortColumns);
                builder.addDetail(ss.str());
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::MaterializeNode>) {
                builder.setTitle("API Mat");
                std::stringstream ss; ss << "Idx: " << ColumnPrinter(data.idxColumn, shortColumns); builder.addDetail(ss.str());
                std::stringstream ss2; ss2 << "Filter: " << ColumnPrinter(data.filterColumn, shortColumns); builder.addDetail(ss2.str());
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::MultiGroupNode>) {
                builder.setTitle("API MultiGroup");
                std::stringstream ss;
                ss << "(";
                for (size_t i = 0; i < data.groupColumns.size(); ++i) {
                    ss << ColumnPrinter(data.groupColumns[i], shortColumns);
                    if (i < data.groupColumns.size() - 1) ss << ", ";
                }
                ss << ")";
                builder.addDetail(ss.str());
                
                std::stringstream ss2; ss2 << "Agg: " << ColumnPrinter(data.aggColumn, shortColumns); builder.addDetail(ss2.str());
                std::stringstream ss3; ss3 << "SrtIdx: " << ColumnPrinter(data.outputSortIndex, shortColumns); builder.addDetail(ss3.str());
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::SetOperationNode>) {
                builder.setTitle("API SetOp");
                builder.addDetail("[" + std::to_string((int)data.operation) + "]");
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::SortNode>) {
                builder.setTitle("API Sort");
                std::stringstream ss;
                ss << "(";
                for (size_t i = 0; i < data.inputColumns.size(); ++i) {
                    ss << ColumnPrinter(data.inputColumns[i], shortColumns);
                    if (i < data.inputColumns.size() - 1) ss << ", ";
                }
                ss << ")";
                builder.addDetail(ss.str());
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::AggNode>) {
                builder.setTitle("API Agg " + AggFunc_Name(data.aggFunc));
                std::stringstream ss;
                ss << "(" << ColumnPrinter(data.inputColumn, shortColumns) << ")";
                builder.addDetail(ss.str());
            }
            else if constexpr (std::is_same_v<T, ItemBuilder::ResultNode>) {
                builder.setTitle("API Result");
                if (node.irData.is<SelectOp>()) {
                     SelectView view(const_cast<IrData&>(node.irData));
                     std::stringstream ss;
                     for(size_t i=0; i<view.resultHeaders().size(); ++i) {
                        ss << view.resultHeaders()[i] << (i < view.resultHeaders().size() - 1 ? ", " : "");
                     }
                     builder.addDetail(ss.str());
                } else {
                    std::stringstream ss;
                    ss << "-> " << data.filename;
                    builder.addDetail(ss.str());
                }
            }
        }, node.apiData);

        if (node.irData.outputsPosList) builder.addRawDetail("<FONT COLOR=\"blue\">POSLIST-OUT</FONT>");
        if (node.irData.outputsMatVals) builder.addRawDetail("<FONT COLOR=\"blue\">MATDATA-OUT</FONT>");

        return builder.build();
    }

    void writePlanNodeDot(const PlanNode* node, std::ofstream& file, DotContentType contentType, std::unordered_set<const PlanNode*>& visited, bool useV2, bool shortColumns, bool ignoreFetch) {
        if (!node) return;

        // Handle DAG/Cycles: If already visited, stop.
        if (visited.count(node)) return;
        visited.insert(node);

        std::ostringstream id;
        id << reinterpret_cast<std::uintptr_t>(node);

        std::string label;
        if (useV2) {
            if (contentType == DotContentType::IR_DATA) {
                label = getIrLabelV2(*node, shortColumns);
            } else {
                label = getApiLabelV2(*node, shortColumns);
            }
            file << "    " << id.str() << " [label=" << label << "];\n";
        } else {
            if (contentType == DotContentType::IR_DATA) {
                label = getIrLabel(*node);
            } else {
                label = getApiLabel(*node);
            }
            file << "    " << id.str() << " [label=\"" << label << "\"];\n";
        }

        for (const auto& child : node->children) {
            if (child) {
                std::ostringstream childId;
                childId << reinterpret_cast<std::uintptr_t>(child.get());

                if (ignoreFetch && contentType == DotContentType::IR_DATA && child->irData.is<FetchOp>()) {
                    continue;
                }

                // Write Edge
                file << "    " << id.str() << " -> " << childId.str() << " [dir=back];\n";

                // Recurse
                writePlanNodeDot(child.get(), file, contentType, visited, useV2, shortColumns, ignoreFetch);
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
    writePlanNodeDot(&root, file, contentType, visited, false, false, false);

    file << "}\n";
    file.close();
}

void generatePlanDotFileV2(const PlanNode& root, const std::string& filename, DotContentType contentType, bool shortColumns, bool ignoreFetch) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file " << filename << std::endl;
        return;
    }

    file << "digraph PlanNode {\n";
    file << "    node [shape=none, fontname=\"Helvetica\", margin=0];\n";

    std::unordered_set<const PlanNode*> visited;
    writePlanNodeDot(&root, file, contentType, visited, true, shortColumns, ignoreFetch);

    file << "}\n";
    file.close();
}