#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <string>
#include "generate_AST.h"   // includes ASTNode, ExprNode, TableNode, etc.

void writeDot(ASTNode* root, std::ofstream& file) {
    if (!root) return;

    std::string label;

    // 🧠 Build the label string depending on the type stored in the variant
    if (auto e = std::get_if<ExprNode>(&root->val)) {
        // Example: WHERE A.id = B.id  OR  WHERE A.id = 12
        label = toExprTypeString(e->exprType) + "\\n";
        label += e->table + "." + e->column + " ";
        label += toOperatorTypeString(e->operatorType) + " ";

        if (!e->table2.empty()) {
            label += e->table2 + "." + e->column2 + " ";
        }
        label += e->value;

    } else if (auto t = std::get_if<TableNode>(&root->val)) {
        if (!t->tableName.empty()) {
            label = t->tableName;
        } 
        if (t->joinType != None) {
            label += toJoinTypeString(t->joinType) + " ";
            label += t->onLeftTable + "." + t->onLeftTableColumn + " = ";
            label += t->onRightTable + "." + t->onRightTableColumn;
        }

    }else {
        label = "Unknown";
    }

    std::ostringstream id;
    id << reinterpret_cast<std::uintptr_t>(root);

    file << "    " << id.str() << " [label=\"" << label << "\"];\n";

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
