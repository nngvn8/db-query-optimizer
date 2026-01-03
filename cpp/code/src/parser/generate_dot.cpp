#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <string>
#include "generate_AST.h"  

void writeDot(ASTNode* root, std::ofstream& file) {
    if (!root) return;

    std::string label;

    if (auto e = std::get_if<SetOperationNode>(&root->val)) {
        label = "SetOperation:\\n" + e->setOperation;
    }
    else if (auto e = std::get_if<TableJoinNode>(&root->val)) {
        if (e->joinType == "CROSSPRODUCT") {
            label = "CROSSPRODUCT";
        } else {
            label = "Join Node:\\n";
            label += e->joinType + "\\n";
            label += e->onLeftTable + "." + e->onLeftTableColumn;
            label += " = ";
            label += e->onRightTable + "." + e->onRightTableColumn;
        }
    }
    else if (auto e = std::get_if<TableBaseNode>(&root->val)) {
        label = "Table:\\n" + e->tableName;
        if (!e->tableAlias.empty()) {
            label += "\\n" + e->tableAlias;
        }
    }
    else if (auto e = std::get_if<WhereClauseNode>(&root->val)) {
        label = "Where:\\n";
        if (e->operatorType == "OR") {
            label += e->table + "." + e->column + " = " + e->value;
            label += "\\nOR\\n";
            label += e->table2 + "." + e->column2 + " = " + e->value2;
        }
        else if (e->operatorType == "BETWEEN") {
            label += e->table + "." + e->column;
            label += " BETWEEN " + e->value + " AND " + e->value2;
        }
        else {
            label += e->table + "." + e->column + " ";
            label += e->operatorType + " ";
            label += e->value;
            if (!e->table2.empty()) {
                label += " " + e->table2 + "." + e->column2;
            }
        }
    }
    else if (auto e = std::get_if<SelectClauseNode>(&root->val)) {
        label = "Select:\\n";

        if (e->distinct) {
            label += "DISTINCT ";
        }

        if (e->star) {
            label += "*";
        } else if (!e->aggregateFunction.empty()) {
            label += e->aggregateFunction + "(" + e->column + ")";
        } else {
            if (!e->table.empty()) {
                label += e->table + ".";
            }
            label += e->column;
        }

        if (!e->alias.empty()) {
            label += "\\nAS " + e->alias;
        }
    }
    else if (auto e = std::get_if<GroupByClauseNode>(&root->val)) {
        label = "Group By:\\n";
        for (const auto& d : e->description) {
            label += d.table + "." + d.column + "\\n";
        }
    }
    else if (auto e = std::get_if<OrderByClauseNode>(&root->val)) {
        label = "Order By:\\n";
        for (const auto& o : e->orderByList) {
            label += o.table + "." + o.column + " " + o.ordertype + "\\n";
        }
    }
    else if (auto e = std::get_if<LimitClauseNode>(&root->val)) {
        label = "Limit:\\n";
        label += e->limit;
        label += " OFFSET ";
        label += e->offset;
    }
    else {
        label = "Unknown";
    }

    std::ostringstream id;
    id << reinterpret_cast<std::uintptr_t>(root);

    file << "    " << id.str()
         << " [shape=box, label=\"" << label << "\"];\n";

    if (root->left) {
        std::ostringstream leftId;  
        leftId << reinterpret_cast<std::uintptr_t>(root->left);
        file << "    " << id.str() << " -> " << leftId.str() << ";\n";
        writeDot(root->left, file);
    }

    if (root->right) {
        std::ostringstream rightId;
        rightId << reinterpret_cast<std::uintptr_t>(root->right);
        file << "    " << id.str() << " -> " << rightId.str() << ";\n";
        writeDot(root->right, file);
    }
}


void generateDotFile(ASTNode* root, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file " << filename << std::endl;
        return;
    }

    file << "digraph AST {\n";
    file << "    node [shape=box, fontname=\"Helvetica\"];\n";
    writeDot(root, file);
    file << "}\n";

    file.close();
}
