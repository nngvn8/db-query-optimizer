#include "hsql/SQLParser.h"
#include "generate_AST.h"
#include <iostream>
#include <variant>
#include <string>
#include <queue>
#include <memory>

using namespace std;

ASTNode* makeExprNode(hsql::Expr* expr){
    switch(expr->type){
        case hsql::kExprColumnRef: {
            ASTNode* node = new ASTNode(ExprNode(ExprSelect,expr->table,expr->name)); 
            return node;
        }
        case hsql::kExprOperator: {
            switch(expr->opType){
                case hsql::kOpEquals:{
                    switch(expr->expr2->type){
                        case hsql::kExprLiteralInt:{
                            ASTNode* node = new ASTNode(ExprNode(ExprWhere,expr->expr->table,expr->expr->name,OpEquals,"","",to_string(expr->expr2->ival)));
                            return node;
                        }
                        case hsql::kExprColumnRef :{
                            ASTNode* node = new ASTNode(ExprNode(ExprWhere,expr->expr->table,expr->expr->name,OpEquals,expr->expr2->table,expr->expr2->name,""));
                            return node;
                        }
                    }
                }
            } 
        }
        default:{
            return nullptr;
        }
    }
    return nullptr;  
}

ASTNode* makeTableNode(hsql::TableRef* table){
    switch(table->type){
        case hsql::kTableJoin:{
            switch(table->join->type){
                case hsql::kJoinInner:{
                    ASTNode* node = new ASTNode(TableNode(TableJoin,"", JoinInner, table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
                case hsql::kJoinFull:{
                    ASTNode* node = new ASTNode(TableNode(TableJoin,"", JoinFull, table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
                case hsql::kJoinLeft:{
                    ASTNode* node = new ASTNode(TableNode(TableJoin,"", JoinLeft, table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
                case hsql::kJoinRight:{
                    ASTNode* node = new ASTNode(TableNode(TableJoin,"", JoinRight, table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
                case hsql::kJoinCross:{
                    ASTNode* node = new ASTNode(TableNode(TableJoin,"", JoinCross, table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
                case hsql::kJoinNatural:{
                    ASTNode* node = new ASTNode(TableNode(TableJoin,"", JoinNatural, table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
            }
        }
        case hsql::kTableName:{
            ASTNode* node = new ASTNode(TableNode(TableName,table->name));
            return node;
        }
    }
    return nullptr;  
}

std::string toExprTypeString(ExprType type) {
    switch (type) {
        case ExprSelect: return "Select";
        case ExprWhere:  return "Where";
        default:         return "Unknown";
    }
}

std::string toOperatorTypeString(OperatorType type) {
    switch (type) {
        case OpEquals: return "=";
        default:         return "";
    }
}


std::string toJoinTypeString(JoinType type) {
    switch (type) {
        case JoinInner: return "JoinInner";
        case JoinFull: return "JoinFull";
        case JoinLeft: return "JoinLeft";
        case JoinRight: return "JoinRight";
        case JoinCross: return "JoinCross";
        case JoinNatural: return "JoinNatural";
        default:         return "";
    }
}

void printAST(ASTNode* root){
    if(root == nullptr){return;}
    if (auto e = std::get_if<ExprNode>(&root->val)) {
        cout<<toExprTypeString((*e).exprType)<<" "<<(*e).table<<"."<<(*e).column<<" ";
        cout<<toOperatorTypeString((*e).operatorType)<<" ";
        if((*e).table2 != ""){cout<<(*e).table2<<"."<<(*e).column2<<" ";}
        cout<<(*e).value<<endl;
    } else if (auto t = std::get_if<TableNode>(&root->val)) {
        if((*t).tableName != ""){cout<<(*t).tableName<<endl;}
        if((*t).joinType != None){
            cout<<toJoinTypeString((*t).joinType)<<" "<<(*t).onLeftTable<<"."<<(*t).onLeftTableColumn<<" = "<<(*t).onRightTable<<"."<<(*t).onRightTableColumn<<endl;
        }
    }
    printAST(root->left);
    printAST(root->right);
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


ASTNode* exploreTable(hsql::TableRef* table){
    if(!table->join){
        return makeTableNode(table);
    }
    
    ASTNode* newRoot = makeTableNode(table); 
    newRoot->left = exploreTable(table->join->left);
    newRoot->right = exploreTable(table->join->right);                      
    
    return newRoot;
}

ASTNode* generateASTNode(const std::string& query){
    // cout<<query;
    hsql::SQLParserResult result;
    bool success = hsql::SQLParser::parseSQLString(query, &result);

    if(result.isValid()){
        std::cout<<"the result is valid"<<"\n";
    }
    else{
        std::cout<<"the result is invalid"<<"\n";
    }

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* selectStmt = static_cast<const hsql::SelectStatement*>(stmt);
        
    ASTNode* root = new ASTNode();

    if (auto e = std::get_if<ExprNode>(&root->val)){
            cout<<(*e).table;
        }

    for(int i=0;i<result.size();i++){
        const hsql::SQLStatement* stmt = result.getStatement(i);
        const hsql::SelectStatement* selectStmt = static_cast<const hsql::SelectStatement*>(stmt);
        
        delete root;
        root = makeExprNode(selectStmt->selectList->at(0));
        ASTNode* current = root;

        // if (auto e = std::get_if<ExprNode>(&root->val)){
        //     cout<<(*e).table;
        // }
    
        if(stmt->type() == hsql::kStmtSelect){
            //logic to parse "select clause"
            if (selectStmt->selectList) {
                    for (int i=1;i<selectStmt->selectList->size();i++) {
                        current->left = makeExprNode(selectStmt->selectList->at(i));
                        current = current->left;
                    }
                }

            //logic to parse "where clause"
            if(selectStmt->whereClause){
                std::queue<hsql::Expr*> q;
                parseWhere(selectStmt->whereClause, q);
                while (!q.empty()) {
                    hsql::Expr* expr = q.front();
                    current->left = makeExprNode(expr);
                    current = current->left;
                    q.pop();
                }
            }
            
            // Join
            current->left = exploreTable(selectStmt->fromTable);
        }   
    }
    cout<<"Parsed Successfully"<<endl;
    // printAST(root);
    return root;
}

