#include <SQLParser.h>
#include "ir/catalog.hpp"
#include "parser/generate_AST.hpp"
#include "parser/optimizer_one.hpp"
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

ASTNode* makeWhereNode(hsql::Expr* expr){
    std::string op;
    switch(expr->opType){
        case hsql::kOpEquals:{
            op = "EQUAL";
            break;
        }
        case hsql::kOpNotEquals:{
            op = "NOTEQUAL";
            break;
        }
        case hsql::kOpLess:{
            op = "LESSTHEN";
            break;
        }
        case hsql::kOpLessEq:{
            op = "LESSTHENEQ";
            break;
        }
        case hsql::kOpGreater:{
            op = "GREATERTHEN";
            break;
        }
        case hsql::kOpGreaterEq:{
            op = "GREATERTHENEQ";
            break;
        }
        case hsql::kOpOr:{
            op = "OR";
            break;
        }
        case hsql::kOpBetween:{
            op = "BETWEEN";
            break;
        }
        default:
            op = "default";
    }

    if(op == "BETWEEN"){
        std::string value1, value2, table, name;

        if(expr->expr->name){
            name = expr->expr->name;
        }
        else{
            name = "";
        }

        switch(expr->exprList->at(0)->type){
            case hsql::kExprLiteralInt:{
                value1 = to_string(expr->exprList->at(0)->ival);
                break;
            }

            case hsql::kExprLiteralString:{
                value1 = expr->exprList->at(0)->name;
                break;
            }

            case hsql::kExprLiteralFloat:{
                value1 = to_string(expr->exprList->at(0)->fval);
                break;
            }

            case hsql::kExprLiteralNull:{
                value1 = "NULL";
                break;
            }
        }

        switch(expr->exprList->at(1)->type){
            case hsql::kExprLiteralInt:{
                value2 = to_string(expr->exprList->at(1)->ival);
                break;
            }

            case hsql::kExprLiteralString:{
                value2 = expr->exprList->at(1)->name;
                break;
            }

            case hsql::kExprLiteralFloat:{
                value2 = to_string(expr->exprList->at(1)->fval);
                break;
            }

            case hsql::kExprLiteralNull:{
                value2 = "NULL";
                break;
            }
        }

        ASTNode* node = new ASTNode(WhereClauseNode(Catalog::getTableName(name),name,op,"","",value1,value2));
        return node;
    }
    else if(op == "OR"){
        string table1, table2, value1, value2, colname1, colname2;
        switch(expr->expr2->expr2->type){
            case hsql::kExprLiteralInt:{
                value1 = to_string(expr->expr->expr2->ival);
                value2 = to_string(expr->expr2->expr2->ival);
                colname1 = expr->expr->expr->name;
                colname2 = expr->expr2->expr->name;
                break;
            }

            case hsql::kExprLiteralString:{
                value1 = expr->expr->expr2->name;
                value2 = expr->expr2->expr2->name;
                colname1 = expr->expr->expr->name;
                colname2 = expr->expr2->expr->name;
                break;
            }

        }

        ASTNode* node = new ASTNode(WhereClauseNode(Catalog::getTableName(colname1),colname1,op,Catalog::getTableName(colname2),colname2,value1,value2));
        return node;
    }
    else{
        std::string name;
            if(expr->expr->name){
                name = expr->expr->name;
            }
            else{
                name = "";
            }

        switch(expr->expr2->type){
            case hsql::kExprLiteralInt:{
                ASTNode* node = new ASTNode(WhereClauseNode(Catalog::getTableName(name),name,op,"","",to_string(expr->expr2->ival)));
                return node;
            }

            case hsql::kExprLiteralString:{
                ASTNode* node = new ASTNode(WhereClauseNode(Catalog::getTableName(name),name,op,"","",expr->expr2->name));
                return node;
            }

            case hsql::kExprLiteralFloat:{
                ASTNode* node = new ASTNode(WhereClauseNode(Catalog::getTableName(name),name,op,"","",to_string(expr->expr2->fval)));
                return node;
            }

            case hsql::kExprLiteralNull:{
                ASTNode* node = new ASTNode(WhereClauseNode(Catalog::getTableName(name),name,op,"","","NULL"));
                return node;
            }

            case hsql::kExprColumnRef :{
                std::string name2 = expr->expr2->name;
                ASTNode* node = new ASTNode(WhereClauseNode(Catalog::getTableName(name),name,op,Catalog::getTableName(name2),name2,""));
                return node;
            }
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
    hsql::Expr* curr = expr;
    while(curr->opType == hsql::kOpAnd){
        q.push(curr->expr2);
        cout<<curr->expr2->type;
        curr = curr->expr;
    }
    q.push(curr);
}

bool isTableInRef(hsql::TableRef* table, const std::string& tableName) {
    if (!table) return false;

    // 1. Base Table: Check Name and Alias
    if (table->type == hsql::kTableName) {
        std::string tName = table->name ? table->name : "";
        std::string tAlias = std::string(table->alias->name).empty() ? table->alias->name : "";
        return tName == tableName || tAlias == tableName;
    }

    // 2. Join: Check Left and Right recursively
    if (table->type == hsql::kTableJoin) {
        return isTableInRef(table->join->left, tableName) ||
               isTableInRef(table->join->right, tableName);
    }

    // 3. Cross Product: Check list
    if (table->type == hsql::kTableCrossProduct) {
        if (table->list) {
            for (auto* t : *table->list) {
                if (isTableInRef(t, tableName)) return true;
            }
        }
    }

    return false;
}

ASTNode* exploreTable(hsql::TableRef* table){
    if(!table->join){
        return makeTableNode(table);
    }

    ASTNode* newRoot = makeTableNode(table);
    newRoot->left = exploreTable(table->join->left);
    newRoot->right = exploreTable(table->join->right);

    // Ensure that left children also contains left join column
    if (auto joinNode = std::get_if<TableJoinNode>(&newRoot->val)) {
        // Check if children correctly assigned
        if (!isTableInRef(table->join->left, joinNode->onLeftTable)) {
            // Swap Tables
            std::swap(joinNode->onLeftTable, joinNode->onRightTable);
            // Swap Columns
            std::swap(joinNode->onLeftTableColumn, joinNode->onRightTableColumn);
        }
    }

    return newRoot;
}

OrderByDescription makeOrderNode(hsql::OrderDescription* order, const std::vector<hsql::Expr*>* selectList = nullptr){
    std::string table = "";
    if(order->expr->table){
        table = order->expr->table;
    }
    
    std::string columnName = order->expr->name ? order->expr->name : "";

    if (table.empty() && selectList) {
        for (hsql::Expr* expr : *selectList) {
            if (expr->alias && std::string(expr->alias) == columnName) {
                if (expr->type == hsql::kExprFunctionRef) {
                    std::string funcName = toUpper(expr->name);
                    std::string inner = "";
                    if (expr->exprList && !expr->exprList->empty()) {
                        hsql::Expr* innerExpr = expr->exprList->at(0);
                        if (innerExpr->type == hsql::kExprOperator) {
                            std::string op = "";
                            if(innerExpr->opType == hsql::kOpPlus) op = "+";
                            else if(innerExpr->opType == hsql::kOpMinus) op = "-";
                            else if(innerExpr->opType == hsql::kOpAsterisk) op = "*";
                            else if(innerExpr->opType == hsql::kOpSlash) op = "/";

                            std::string left = innerExpr->expr->name ? innerExpr->expr->name : "";
                            std::string right = innerExpr->expr2->name ? innerExpr->expr2->name : "";
                            inner = left + op + right;
                        } else if (innerExpr->type == hsql::kExprColumnRef) {
                            inner = innerExpr->name;
                        }
                    }
                    columnName = funcName + "(" + inner + ")";
                    table = !funcName.empty() ? "AGG" : "MAP";
                } else if (expr->type == hsql::kExprColumnRef) {
                    columnName = expr->name;
                    table = Catalog::getTableName(columnName);
                }
                break;
            }
        }
    }

    if (table.empty()) {
        table = Catalog::getTableName(columnName);
    }

    switch(order->type){
        case hsql::kOrderAsc:{
            switch (order->null_ordering){
                case hsql::Undefined : return OrderByDescription(table, columnName, "ASC", "UNDEFINED");
                case hsql::First : return OrderByDescription(table, columnName, "ASC", "FIRST");
                case hsql::Last : return OrderByDescription(table, columnName, "ASC", "LAST");
            }
        }
        case hsql::kOrderDesc:{
            switch (order->null_ordering){
                case hsql::Undefined : return OrderByDescription(table, columnName, "DESC", "UNDEFINED");
                case hsql::First : return OrderByDescription(table, columnName, "DESC", "FIRST");
                case hsql::Last : return OrderByDescription(table, columnName, "DESC", "LAST");
            }
        }
    }
    return OrderByDescription("","","","");
}

GroupByDescription makeGroupByNode(hsql::Expr* column){
    std::string table;
    if(column->table){
        table = column->table;
    }
    else{
        table = "";
    }
    return GroupByDescription(Catalog::getTableName(column->name),column->name);
}

ASTNode* exploreCrossProduct(hsql::TableRef* table){
    std::vector<std::string> tablenames;

    for(int i=0;i<table->list->size();i++){
        tablenames.push_back(table->list->at(i)->name);
    }

    ASTNode* root = new ASTNode(TableJoinNode("CROSSPRODUCT"));
    ASTNode* curr = root;

    for(int i=0;i<table->list->size()-2;i++){
        curr->right = new ASTNode(TableBaseNode(tablenames.back()));
        curr->left = new ASTNode(TableJoinNode("CROSSPRODUCT"));
        curr = curr->left;
        tablenames.pop_back();
    }

    curr->right = new ASTNode(TableBaseNode(tablenames.back()));
    tablenames.pop_back();
    curr->left = new ASTNode(TableBaseNode(tablenames.back()));
    tablenames.pop_back();

    return root;
}

void printAST(ASTNode* root){
    if(root == nullptr){return;}

    if (auto e = std::get_if<SetOperationNode>(&root->val)) {
        std::cout<<"SetOperation: "<< (*e).setOperation<<std::endl;
    }
    else if (auto e = std::get_if<TableJoinNode>(&root->val)) {
        if((*e).joinType == "CROSSPRODUCT"){
            std::cout<< "CROSSPRODUCT"<<std::endl;
        }
        else{
        std::cout<<"Join Node: "<< (*e).joinType <<" "<< (*e).onLeftTable <<"."<< (*e).onLeftTableColumn <<" = "<< (*e).onRightTable <<"."<< (*e).onRightTableColumn <<std::endl;
        }
    }
    else if (auto e = std::get_if<TableBaseNode>(&root->val)) {
        std::cout<<"Table: "<< (*e).tableName<<" "<< (*e).tableAlias<<std::endl;
    }
    else if(auto e = std::get_if<WhereClauseNode>(&root->val)){
        if((*e).operatorType == "OR"){
            std::cout<<"Where: "<< (*e).table <<"."<< (*e).column << " = "<<(*e).value<<" "<< (*e).operatorType <<" "<< (*e).table2 <<"."<< (*e).column2<<" = "<<(*e).value2<<std::endl;
        }
        else if((*e).operatorType == "BETWEEN"){
            std::cout<<"Where: "<< (*e).table <<"."<< (*e).column << " "<<(*e).operatorType << " "<<(*e).value<<" AND "<<(*e).value2<<std::endl;
        }
        else{
            std::cout<<"Where: "<< (*e).table <<"."<< (*e).column <<" "<< (*e).operatorType <<" "<< (*e).value << " " << (*e).table2 <<"."<< (*e).column2<<std::endl;
        }
    }
    else if(auto e = std::get_if<SelectClauseNode>(&root->val)){
        cout<<"Select: ";
        for(int i=0;i<(*e).description.size();i++){
            cout<< (*e).description[i].table<<"."<<(*e).description[i].column<<" ";
        }
        cout<<std::endl; 
    }
    else if(auto e = std::get_if<AggregateClauseNode>(&root->val)){
        cout<<"Aggregate Node: "<<(*e).aggregateFunction <<"("<<(*e).table<<"."<<(*e).column<<") as "<< (*e).alias<<std::endl; 
    }
    else if(auto e = std::get_if<GroupByClauseNode>(&root->val)){
        cout<<"Group By: ";
        for(int i=0;i<(*e).description.size();i++){
            cout<< (*e).description[i].table<<"."<<(*e).description[i].column<<" ";
        }
        cout<<std::endl;
    }
    else if(auto e = std::get_if<OrderByClauseNode>(&root->val)){
        cout<<"Order By: ";
        for(int i=0;i<(*e).orderByList.size();i++){
            cout<<(*e).orderByList[i].table<<"."<<(*e).orderByList[i].column <<" "<< (*e).orderByList[i].ordertype <<" | ";
        }
        cout<<std::endl;
    }
    else if(auto e = std::get_if<Map>(&root->val)){
        cout<<"Map: ";
        cout<<(*e).table1<<"."<<(*e).column1 <<" "<< (*e).operatorType << " "<< (*e).table2<<"."<<(*e).column2;
        cout<<std::endl;
    }
    else if(auto e = std::get_if<LimitClauseNode>(&root->val)){
        std::cout<<"Limit: "<< (*e).limit<<" "<< (*e).offset<<std::endl;
    }
    printAST(root->left);
    printAST(root->right);
}

ASTNode* parseQueryExpression(const hsql::SelectStatement* selectStmt){

    ASTNode* root = new ASTNode();
    ASTNode* current = root;


    //logic to parse select clauses
    if (selectStmt->selectList) {
        std::vector<SelectClauseDescription> selectClauseDescriptionList;
        for (int i=0;i<selectStmt->selectList->size();i++) {
            switch(selectStmt->selectList->at(i)->type){
                case hsql::kExprColumnRef:{
                    std::string alias = selectStmt->selectList->at(i)->alias ? selectStmt->selectList->at(i)->alias : "";
                    selectClauseDescriptionList.push_back(SelectClauseDescription(Catalog::getTableName(selectStmt->selectList->at(i)->name),selectStmt->selectList->at(i)->name, alias));
                    break;
                }
                case hsql::kExprFunctionRef:{
                    std::string funcName = toUpper(selectStmt->selectList->at(i)->name);
                    std::string inner = "";
                    if(selectStmt->selectList->at(i)->exprList && !selectStmt->selectList->at(i)->exprList->empty()){
                        hsql::Expr* innerExpr = selectStmt->selectList->at(i)->exprList->at(0);
                        if (innerExpr->type == hsql::kExprOperator) {
                            std::string op = "";
                            if(innerExpr->opType == hsql::kOpPlus) op = "+";
                            else if(innerExpr->opType == hsql::kOpMinus) op = "-";
                            else if(innerExpr->opType == hsql::kOpAsterisk) op = "*";
                            else if(innerExpr->opType == hsql::kOpSlash) op = "/";

                             // Assuming column refs in operator
                            std::string left = innerExpr->expr->name ? innerExpr->expr->name : "";
                            std::string right = innerExpr->expr2->name ? innerExpr->expr2->name : "";
                            inner = left + op + right;
                        } else if (innerExpr->type == hsql::kExprColumnRef) {
                            inner = innerExpr->name;
                        }
                }
                     // "Select should contain the arithmetic expression possibly expanded with the aggregation column"
                     // "The alias should not be directly part of the column name."
                    std::string colName = funcName + "(" + inner + ")";
                    std::string tableName = !funcName.empty() ? "AGG" : "MAP";
                    std::string alias = selectStmt->selectList->at(i)->alias ? selectStmt->selectList->at(i)->alias : "";
                    selectClauseDescriptionList.push_back(SelectClauseDescription(tableName, colName, alias));
                    break;
                }
            }
        }
        delete root;
        ASTNode* node = new ASTNode(SelectClauseNode(selectClauseDescriptionList)); 
        root = node;
        current = root; 
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

    // logic to parse order

    if(selectStmt->order){
        std::vector<OrderByDescription> orderByList;
        for(int i=0;i<selectStmt->order->size();i++){
            orderByList.push_back(makeOrderNode(selectStmt->order->at(i), selectStmt->selectList));
        }
        ASTNode* orderbynode = new ASTNode(OrderByClauseNode(orderByList));
        current->left = orderbynode;
        current = current->left;
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

    // logic for aggregate and map

    if (selectStmt->selectList) {
        std::vector<SelectClauseDescription> selectClauseDescriptionList;
        for (int i=0;i<selectStmt->selectList->size();i++) {
            switch(selectStmt->selectList->at(i)->type){
                case hsql::kExprFunctionRef: {
                    string alias;
                    if(selectStmt->selectList->at(i)->alias){
                        alias = selectStmt->selectList->at(i)->alias;
                    }
                    else{
                        alias = "";
                    }
                    
                    std::string inputTable = "";
                    std::string inputColumn = "";
                    hsql::Expr* arg0 = selectStmt->selectList->at(i)->exprList->at(0);

                    if(arg0->type == hsql::kExprOperator){
                            std::string op;
                            if(arg0->opType == hsql::kOpPlus) op = "+";
                            else if(arg0->opType == hsql::kOpMinus) op = "-";
                            else if(arg0->opType == hsql::kOpAsterisk) op = "*";
                            else if(arg0->opType == hsql::kOpSlash) op = "/";
                            
                            std::string left = arg0->expr->name ? arg0->expr->name : "";
                            std::string right = arg0->expr2->name ? arg0->expr2->name : "";
                            
                            inputTable = "MAP";
                            inputColumn = left + op + right;
                    } else if (arg0->type == hsql::kExprColumnRef) {
                        inputTable = Catalog::getTableName(arg0->name);
                        inputColumn = arg0->name;
                    }

                    ASTNode* node = new ASTNode(AggregateClauseNode(toUpper(selectStmt->selectList->at(i)->name), alias, inputTable, inputColumn));

                    if(auto e = std::get_if<std::monostate>(&current->val)){
                        delete root;
                        root = node;
                        current = root; 
                    }
                    else{
                        current->left = node;
                        current = current->left; 
                    }

                    switch(arg0->type){
                        case hsql::kExprOperator : {
                            std::string op;
                            if(arg0->opType == hsql::kOpPlus){
                                op = "+";
                            }
                            else if(arg0->opType == hsql::kOpMinus){
                                op = "-";
                            }
                            else if(arg0->opType == hsql::kOpAsterisk){
                                op = "*";
                            }
                            else if(arg0->opType == hsql::kOpSlash){
                                op = "/";
                            }

                            ASTNode* node = new ASTNode(Map(Catalog::getTableName(arg0->expr->name),
                                    arg0->expr->name,
                                    Catalog::getTableName(arg0->expr2->name),
                                    arg0->expr2->name,
                                    op)
                                );    
                            current->left = node;
                            current = current->left;
                            continue; 
                        }
                        case hsql::kExprColumnRef : {
                            continue; 
                        }
                    }
                }
            }
        }
    }

    //logic to parse "where clause"

    if (selectStmt->whereClause) {

        std::vector<hsql::Expr*> v;
        hsql::Expr* curr = selectStmt->whereClause;

        while (curr->opType == hsql::kOpAnd) {
            v.push_back(curr->expr2);
            curr = curr->expr;
        }
        v.push_back(curr);

        for (int i=v.size()-1;i>=0;i--) {
            current->left = makeWhereNode(v[i]);
            current = current->left;
        }
    }

    // // Join
    if(selectStmt->fromTable->type == hsql::kTableCrossProduct){
        current->left = exploreCrossProduct(selectStmt->fromTable);
    }
    else{
        current->left = exploreTable(selectStmt->fromTable);
    }

    return root;
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
            delete root;
            root = parseQueryExpression(selectStmt);
        }   
    }
    cout<<"Parsed Successfully"<<endl<<endl;

    root = optimizerOne(root);

    return root;
}