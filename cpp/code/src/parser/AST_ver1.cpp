#include "hsql/SQLParser.h"
#include <iostream>
#include <fstream>
#include <variant>
#include <string>
#include <queue>
#include <memory>
#include <sstream>
#include <cstdint>

using namespace std;


class ASTNode {
public:
    std::variant<std::monostate, hsql::TableRef*, hsql::Expr*> val;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;

    ASTNode() 
    : val(std::monostate{}), left(nullptr), right(nullptr) {}

    explicit ASTNode(std::variant<std::monostate, hsql::TableRef*, hsql::Expr*> v)
        : val(v), left(nullptr), right(nullptr) {}
    
};

void writeDot(ASTNode* root, std::ofstream& file) {
    if (!root) return;
    std::string label;
    if (auto e = std::get_if<hsql::Expr*>(&root->val)) {
        if ((*e)->type == hsql::kExprColumnRef) {
            label = std::string((*e)->table) + "." + (*e)->name;
        } else if ((*e)->type == hsql::kExprOperator) {
            if ((*e)->expr && (*e)->expr2) {
                label = std::string((*e)->expr->table) + "." + (*e)->expr->name;
                if ((*e)->expr2->name){
                    label += " " + std::string((*e)->expr2->table) + "." + (*e)->expr2->name;
                }
                else{
                    //currently this is not checking for optype and defaulting to "="
                    //other conditions can be added for other operator types
                    label += " = " + std::to_string((*e)->expr2->ival);
                }   
            } else {
                label = "Operator";
            }
        } else {
            label = "Expr";
        }
    } else if (auto t = std::get_if<hsql::TableRef*>(&root->val)) {
        if ((*t)->type == hsql::kTableJoin) {
            label = "Join " + std::string((*t)->join->condition->expr->table) + "." + (*t)->join->condition->expr->name + " = " +
            std::string((*t)->join->condition->expr2->table) + "." + (*t)->join->condition->expr2->name;

            // label += "ON" +  std::to_string((*t)->join->condition->expr->table) + "." + (*t)->join->condition->expr->name + " = ";
            // label += std::to_string((*t)->join->condition->expr2->table) + "." + (*t)->join->condition->expr2->name;
        } else if ((*t)->type == hsql::kTableName) {
            label = (*t)->name;
        } else {
            label = "Table";
        }
    }

    std::ostringstream id;
    id << reinterpret_cast<std::uintptr_t>(root);

    file << "    " << id.str() << " [label=\"" << label << "\"];\n";

    if (root->left) {
        std::ostringstream leftId;
        leftId << reinterpret_cast<std::uintptr_t>(root->left.get());
        file << "    " << id.str() << " -> " << leftId.str() << ";\n";
        writeDot(root->left.get(), file);
    }
    if (root->right) {
        std::ostringstream rightId;
        rightId << reinterpret_cast<std::uintptr_t>(root->right.get());
        file << "    " << id.str() << " -> " << rightId.str() << ";\n";
        writeDot(root->right.get(), file);
    }
}

void generateDotFile(ASTNode* root, const std::string& filename) {
    std::ofstream file(filename);
    file << "digraph AST {\n";
    file << "    node [shape=box];\n";
    writeDot(root, file);
    file << "}\n";
}

void printAST(ASTNode* root){
    if(root == nullptr){return;}
    if (auto e = std::get_if<hsql::Expr*>(&root->val)) {
        switch((*e)->type) {
            case hsql::kExprColumnRef:
                cout<<"select : " << (*e)->table << "." << (*e)->name<< (*e)->opType<<endl;    
                break;
            case hsql::kExprOperator:
                switch((*e)->opType){
                    case hsql::kOpEquals:
                        cout<<"Operator: ";
                        cout<<(*e)->expr->table<<"."<<(*e)->expr->name;
                        if((*e)->expr2->name){
                            cout<<(*e)->expr2->table<<"."<<(*e)->expr2->name<<endl;
                        }
                        else{
                            cout<<" = "<<(*e)->expr2->ival<<endl;    
                        }
                }
                break;
            default:
                cout<<"Unknown";
                break;
        }
    } else if (auto t = std::get_if<hsql::TableRef*>(&root->val)) {
        switch((*t)->type){
            case hsql::kTableJoin:
                std::cout << "Join" << " ON "<<(*t)->join->condition->expr->table<<"."<<(*t)->join->condition->expr->name<<" = ";
                std::cout << (*t)->join->condition->expr2->table<<"."<<(*t)->join->condition->expr2->name<<endl;
            break;
            case hsql::kTableName:
                std::cout <<(*t)->name <<endl;
            break;
        }
        
    }
    printAST(root->left.get());
    printAST(root->right.get());
}

void parseWhere(hsql::Expr* expr, std::queue<hsql::Expr*> &q){
    switch (expr->type) {
        case hsql::kExprOperator:
            parseWhere(expr->expr,q);
            parseWhere(expr->expr2,q);
            if(expr->opType != hsql::kOpAnd && expr->opType != hsql::kOpOr){
                q.push(expr);
            }
            break;
        default:
            break;
    }
}

std::unique_ptr<ASTNode> exploreTable(hsql::TableRef* table){
    if(!table->join){
        return std::make_unique<ASTNode>(table);
    }
    auto newRoot = std::make_unique<ASTNode>(table); 
    newRoot->left = exploreTable(table->join->left);
    newRoot->right = exploreTable(table->join->right);                      
    return newRoot;
}

int main() {
    const std::string query = "SELECT j.id, j.name, t.id FROM testtable t JOIN jointable j ON t.id = j.id WHERE testtable.name = 12";
    hsql::SQLParserResult result;

    bool success = hsql::SQLParser::parseSQLString(query, &result);

    if(result.isValid()){
        std::cout<<"the result is valid"<<"\n";
    }
    else{
        std::cout<<"the result is invalid"<<"\n";
        return 0;
    }
    
    for(int i=0;i<result.size();i++){
        const hsql::SQLStatement* stmt = result.getStatement(i);
        const hsql::SelectStatement* selectStmt = static_cast<const hsql::SelectStatement*>(stmt);
        
        ASTNode* root = new ASTNode(selectStmt->selectList->at(0));
        ASTNode* current = root;
    
        
        if(stmt->type() == hsql::kStmtSelect){
            //logic to parse "select clause"
            if (selectStmt->selectList) {
                    for (int i=1;i<selectStmt->selectList->size();i++) {
                        current->left = std::make_unique<ASTNode>(selectStmt->selectList->at(i)); 
                        current = current->left.get();
                    }
                }
            //logic to parse "where clause"
            if(selectStmt->whereClause){
                std::queue<hsql::Expr*> q;
                parseWhere(selectStmt->whereClause, q);
                while (!q.empty()) {
                    hsql::Expr* expr = q.front();
                    current->left = std::make_unique<ASTNode>(expr); 
                    current = current->left.get();
                    q.pop();
                }
            }
            
            //Join
            current->left = exploreTable(selectStmt->fromTable);
            
        }
        
        //To generate a dot file of the tree
        //run this command to generate a PNG of the DOT file, "dot -Tpng ast.dot -o ast.png"
        generateDotFile(root,"ast3.dot");

        //for Pre-order traversal of the AST
        printAST(root);
    }
    
    cout<<"\n\n";
}
