#include "hsql/SQLParser.h"
#include "generate_ast_alternate.h"
#include <iostream>
#include <variant>
#include <string>
#include <queue>
#include <algorithm>
#include <cctype>
#include <memory>

using namespace std;

std::string toUpper(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return result;
}

ASTNode* makeSelectNode(hsql::Expr* expr){
    switch(expr->type){
        case hsql::kExprColumnRef: {
            if(expr->table){
                ASTNode* node = new ASTNode(SelectClauseNode(false,expr->table,expr->name,"",expr->distinct)); 
                return node;
            }
            else{
                ASTNode* node = new ASTNode(SelectClauseNode(false,"",expr->name,"",expr->distinct)); 
                return node;
            }
        }
        case hsql::kExprStar: {
            ASTNode* node = new ASTNode(SelectClauseNode(true)); 
            return node;
        }
        case hsql::kExprFunctionRef: {
            switch(expr->exprList->at(0)->type){
                case hsql::kExprColumnRef : {
                    if(expr->table){
                        ASTNode* node = new ASTNode(SelectClauseNode(false,expr->exprList->at(0)->table,expr->exprList->at(0)->name,toUpper(expr->name),expr->distinct)); 
                        return node;
                    }
                    else{
                        ASTNode* node = new ASTNode(SelectClauseNode(false,"",expr->exprList->at(0)->name,toUpper(expr->name),expr->distinct)); 
                        return node;
                    }
                    
                }
                case hsql::kExprStar : {
                        ASTNode* node = new ASTNode(SelectClauseNode(true,"","",expr->name)); 
                        return node;
                }
            }
        }
    
        default:{
            return nullptr;
        }
    }
    return nullptr;  
}


ASTNode* makeWhereNode(hsql::Expr* expr){
    switch(expr->type){
        case hsql::kExprOperator: {
            switch(expr->opType){
                case hsql::kOpEquals:{
                    switch(expr->expr2->type){
                        case hsql::kExprLiteralInt:{
                            ASTNode* node = new ASTNode(WhereClauseNode(expr->expr->table,expr->expr->name,"EQUALS","","",to_string(expr->expr2->ival)));
                            return node;
                        }

                        case hsql::kExprLiteralString:{
                            ASTNode* node = new ASTNode(WhereClauseNode(expr->expr->table,expr->expr->name,"EQUALS","","",expr->expr2->name));
                            return node;
                        }
                        
                        case hsql::kExprLiteralFloat:{
                            ASTNode* node = new ASTNode(WhereClauseNode(expr->expr->table,expr->expr->name,"EQUALS","","",to_string(expr->expr2->fval)));
                            return node;
                        }

                        case hsql::kExprLiteralNull:{
                            ASTNode* node = new ASTNode(WhereClauseNode(expr->expr->table,expr->expr->name,"EQUALS","","","NULL"));
                            return node;
                        }

                        case hsql::kExprColumnRef :{
                            ASTNode* node = new ASTNode(WhereClauseNode(expr->expr->table,expr->expr->name,"EQUALS",expr->expr2->table,expr->expr2->name,""));
                            return node;
                        }
                    }
                }

                case hsql::kOpNotEquals:{
                    switch(expr->expr2->type){
                        case hsql::kExprLiteralInt:{
                            ASTNode* node = new ASTNode(WhereClauseNode(expr->expr->table,expr->expr->name,"NOTEQUALS","","",to_string(expr->expr2->ival)));
                            return node;
                        }

                        case hsql::kExprLiteralString:{
                            ASTNode* node = new ASTNode(WhereClauseNode(expr->expr->table,expr->expr->name,"NOTEQUALS","","",expr->expr2->name));
                            return node;
                        }
                        
                        case hsql::kExprLiteralFloat:{
                            ASTNode* node = new ASTNode(WhereClauseNode(expr->expr->table,expr->expr->name,"NOTEQUALS","","",to_string(expr->expr2->fval)));
                            return node;
                        }

                        case hsql::kExprLiteralNull:{
                            ASTNode* node = new ASTNode(WhereClauseNode(expr->expr->table,expr->expr->name,"NOTEQUALS","","","NULL"));
                            return node;
                        }

                        case hsql::kExprColumnRef :{
                            ASTNode* node = new ASTNode(WhereClauseNode(expr->expr->table,expr->expr->name,"NOTEQUALS",expr->expr2->table,expr->expr2->name,""));
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
                    ASTNode* node = new ASTNode(TableJoinNode("JOININNER", table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
                case hsql::kJoinFull:{
                    ASTNode* node = new ASTNode(TableJoinNode("JOINFULL", table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
                case hsql::kJoinLeft:{
                    ASTNode* node = new ASTNode(TableJoinNode("JOINLEFT", table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
                case hsql::kJoinRight:{
                    ASTNode* node = new ASTNode(TableJoinNode("JOINRIGHT", table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
                case hsql::kJoinCross:{
                    ASTNode* node = new ASTNode(TableJoinNode("JOINCROSS", table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
                case hsql::kJoinNatural:{
                    ASTNode* node = new ASTNode(TableJoinNode("JOINNATURAL", table->join->condition->expr->table, table->join->condition->expr->name, table->join->condition->expr2->table, table->join->condition->expr2->name));
                    return node;
                }
            }
        }
        case hsql::kTableName:{
            ASTNode* node = new ASTNode(TableBaseNode(table->name,table->getName()));
            return node;
        }
    }
    return nullptr;  
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

OrderByDescription makeOrderNode(hsql::OrderDescription* order){
    switch(order->type){
        case hsql::kOrderAsc:{
            switch (order->null_ordering){
                case hsql::Undefined : {
                    return OrderByDescription(order->expr->name,order->expr->table,"ASC","UNDEFINED");
                }
                case hsql::First:{
                    return OrderByDescription(order->expr->name,order->expr->table,"ASC","FIRST");
                }
                case hsql::Last:{
                    return OrderByDescription(order->expr->name,order->expr->table,"ASC","LAST");
                }
            }
        }
        case hsql::kOrderDesc:{
            switch (order->null_ordering){
                case hsql::Undefined : {
                    return OrderByDescription(order->expr->name,order->expr->table,"DESC","UNDEFINED");
                }
                case hsql::First:{
                    return OrderByDescription(order->expr->name,order->expr->table,"DESC","FIRST");
                }
                case hsql::Last:{
                    return OrderByDescription(order->expr->name,order->expr->table,"DESC","LAST");
                }
            }
        }
    }
    return OrderByDescription("","","","");;
}

GroupByDescription makeGroupByNode(hsql::Expr* column){
    return GroupByDescription(column->name,column->table);
}


void printAST(ASTNode* root){
    if(root == nullptr){return;}
    if (auto e = std::get_if<TableJoinNode>(&root->val)) {
        std::cout<<"Join Node: "<< (*e).joinType <<" "<< (*e).onLeftTable <<"."<< (*e).onLeftTableColumn <<" = "<< (*e).onRightTable <<"."<< (*e).onRightTableColumn <<std::endl;
    }
    else if (auto e = std::get_if<TableBaseNode>(&root->val)) {
        std::cout<<"Table: "<< (*e).tableName<<" "<< (*e).tableAlias<<std::endl;
    }
    else if(auto e = std::get_if<WhereClauseNode>(&root->val)){
        std::cout<<"Where: "<< (*e).table <<"."<< (*e).column <<" "<< (*e).operatorType <<" "<< (*e).value << " " << (*e).table2 <<"."<< (*e).column2<<std::endl;
    }
    else if(auto e = std::get_if<SelectClauseNode>(&root->val)){
        std::string star = "";
        std::string distict = "";

        if((*e).star){
            star = "*";
        }
        if((*e).distinct){
            distict = "distinct";
        }
        cout<<"Select: "<<distict <<" "<< (*e).aggregateFunction <<" "<< star <<" "<< (*e).table<<"."<< (*e).column <<std::endl;
    }
    else if(auto e = std::get_if<GroupByClauseNode>(&root->val)){
        cout<<"Group By: ";
        for(int i=0;i<(*e).description.size();i++){
            cout<< (*e).description[i].table<<" "<<(*e).description[i].column;
        }
        cout<<std::endl;
    }
    else if(auto e = std::get_if<OrderByClauseNode>(&root->val)){
        cout<<"Order By: ";
        for(int i=0;i<(*e).orderByList.size();i++){
            cout<<(*e).orderByList[i].table<<"."<<(*e).orderByList[i].column <<" "<< (*e).orderByList[i].ordertype <<" | NULL_ORDERING : "<< (*e).orderByList[i].nullordering<<" ";
        }
        cout<<std::endl;
    }
    else if(auto e = std::get_if<LimitClauseNode>(&root->val)){
        std::cout<<"Limit: "<< (*e).limit<<" "<< (*e).offset<<std::endl;
    }
    printAST(root->left);
    printAST(root->right);
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
        
    ASTNode* root = new ASTNode();

    for(int i=0;i<result.size();i++){
        const hsql::SQLStatement* stmt = result.getStatement(i);
        
        if(stmt->type() == hsql::kStmtSelect){
            const hsql::SelectStatement* selectStmt = static_cast<const hsql::SelectStatement*>(stmt);

            //logic to parse "select clause"
            delete root;
            root = makeSelectNode(selectStmt->selectList->at(0));
            ASTNode* current = root;    

            if (selectStmt->selectList) {
                    for (int i=1;i<selectStmt->selectList->size();i++) {
                        current->left = makeSelectNode(selectStmt->selectList->at(i));
                        current = current->left;
                    }
                }

            //logic to parse "where clause"
            if(selectStmt->whereClause){
                std::queue<hsql::Expr*> q;
                parseWhere(selectStmt->whereClause, q);
                while (!q.empty()) {
                    hsql::Expr* expr = q.front();
                    current->left = makeWhereNode(expr);
                    current = current->left;
                    q.pop();
                }
            }
            
            //logic to parse "Group By clause"
            if(selectStmt->groupBy){
                std::vector<GroupByDescription> groupByList;
                
                for(int i=0;i<selectStmt->groupBy->columns->size();i++){
                    groupByList.push_back(makeGroupByNode(selectStmt->groupBy->columns->at(i)));
                }
                ASTNode* groupBynode = new ASTNode(GroupByClauseNode(groupByList));
                current->left = groupBynode;
                current = current->left;
            }

            //logic to parse "Order By clause"

            if(selectStmt->order){
                std::vector<OrderByDescription> orderByList;
                for(int i=0;i<selectStmt->order->size();i++){
                    orderByList.push_back(makeOrderNode(selectStmt->order->at(i)));
                }
                ASTNode* orderbynode = new ASTNode(OrderByClauseNode(orderByList));
                current->left = orderbynode;
                current = current->left;
            }

            // logic to parse "Limit clause"
            if(selectStmt->limit){
                if(selectStmt->limit->offset){
                    ASTNode* limitnode = new ASTNode(LimitClauseNode(to_string(selectStmt->limit->limit->ival),to_string(selectStmt->limit->offset->ival)));
                    current->left = limitnode;
                    current = current->left;
                }
                else{
                    ASTNode* limitnode = new ASTNode(LimitClauseNode(to_string(selectStmt->limit->limit->ival),""));
                    current->left = limitnode;
                    current = current->left;   
                }
            }
    
            // // Join
            current->left = exploreTable(selectStmt->fromTable);
        }   
    }
    cout<<"Parsed Successfully"<<endl<<endl;
    // printAST(root);
    return root;
}

