#include "generate_AST.h"
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
    string alias;
    if(expr->alias){
        alias = expr->alias;
    }
    else{
        alias = "";
    }

    switch(expr->type){
        case hsql::kExprColumnRef: {
            string tablename;
            if(expr->table){
                tablename = expr->table;
            }
            else{
                tablename = "";
            }

            ASTNode* node = new ASTNode(SelectClauseNode(false,tablename,expr->name,"",alias,expr->distinct)); 
            return node;
        }
        case hsql::kExprStar: {
            ASTNode* node = new ASTNode(SelectClauseNode(true)); 
            return node;
        }
        case hsql::kExprFunctionRef: {
            switch(expr->exprList->at(0)->type){
                case hsql::kExprOperator : {
                    std::string op;
                    if(expr->exprList->at(0)->opType == hsql::kOpPlus){
                        op = "+";
                    }
                    else if(expr->exprList->at(0)->opType == hsql::kOpMinus){
                        op = "-";
                    }
                    else if(expr->exprList->at(0)->opType == hsql::kOpAsterisk){
                        op = "*";
                    }
                    else if(expr->exprList->at(0)->opType == hsql::kOpSlash){
                        op = "*";
                    }

                    string a = expr->exprList->at(0)->expr->name;
                    string b = expr->exprList->at(0)->expr2->name;
                    
                    string column = a + " " + op + " " +b;

                    ASTNode* node = new ASTNode(SelectClauseNode(false,"",column,toUpper(expr->name),alias,expr->distinct)); 
                    return node;
                    
                }

                case hsql::kExprColumnRef : {
                    if(expr->table){
                        ASTNode* node = new ASTNode(SelectClauseNode(false,expr->exprList->at(0)->table,expr->exprList->at(0)->name,toUpper(expr->name),alias,expr->distinct)); 
                        return node;
                    }
                    else{
                        ASTNode* node = new ASTNode(SelectClauseNode(false,"",expr->exprList->at(0)->name,toUpper(expr->name),alias,expr->distinct)); 
                        return node;
                    }
                    
                }
                case hsql::kExprStar : {
                        ASTNode* node = new ASTNode(SelectClauseNode(true,"","",expr->name,alias,expr->distinct)); 
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
        
        if(expr->expr->table){
            table = expr->expr->table;
        }
        else{
            table = "";
        }
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
        
        ASTNode* node = new ASTNode(WhereClauseNode(table,name,op,"","",value1,value2));
        return node;
    }
    else if(op == "OR"){
        string table1, table2, value1, value2, colname1, colname2;

        if(expr->expr->expr->table){
            table1 = expr->expr->expr->table;
        }
        else{
            table1 = "";
        }

        if(expr->expr2->expr->table){
            table2 = expr->expr2->expr->table;
        }
        else{
            table2 = "";
        }

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

        ASTNode* node = new ASTNode(WhereClauseNode(table1,colname1,op,table2,colname2,value1,value2));
        return node;
    }
    else{
        std::string table,name;
            if(expr->expr->table){
                table = expr->expr->table;
            }
            else{
                table = "";
            }
            if(expr->expr->name){
                name = expr->expr->name;
            }
            else{
                name = "";
            }

        switch(expr->expr2->type){
            case hsql::kExprLiteralInt:{
                ASTNode* node = new ASTNode(WhereClauseNode(table,name,op,"","",to_string(expr->expr2->ival)));
                return node;
            }

            case hsql::kExprLiteralString:{
                ASTNode* node = new ASTNode(WhereClauseNode(table,name,op,"","",expr->expr2->name));
                return node;
            }
                    
            case hsql::kExprLiteralFloat:{
                ASTNode* node = new ASTNode(WhereClauseNode(table,name,op,"","",to_string(expr->expr2->fval)));
                return node;
            }

            case hsql::kExprLiteralNull:{
                ASTNode* node = new ASTNode(WhereClauseNode(table,name,op,"","","NULL"));
                return node;
            }

            case hsql::kExprColumnRef :{
                string table2,name2;
                if(expr->expr2->table){
                    table2 = expr->expr2->table;
                }
                else{
                    table2 = "";
                }   
                
                if(expr->expr2->name){
                    name2 = expr->expr2->name;
                }
                else{
                    name2 = "";
                }
                ASTNode* node = new ASTNode(WhereClauseNode(table,name,op,table2,name2,""));
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
    std::string table = "";
    if(order->expr->table){
        table = order->expr->table;
    }
    switch(order->type){
        case hsql::kOrderAsc:{
            switch (order->null_ordering){
                case hsql::Undefined : {
                    return OrderByDescription(order->expr->name,table,"ASC","UNDEFINED");
                }
                case hsql::First:{
                    return OrderByDescription(order->expr->name,table,"ASC","FIRST");
                }
                case hsql::Last:{
                    return OrderByDescription(order->expr->name,table,"ASC","LAST");
                }
            }
        }
        case hsql::kOrderDesc:{
            switch (order->null_ordering){
                case hsql::Undefined : {
                    return OrderByDescription(order->expr->name,table,"DESC","UNDEFINED");
                }
                case hsql::First:{
                    return OrderByDescription(order->expr->name,table,"DESC","FIRST");
                }
                case hsql::Last:{
                    return OrderByDescription(order->expr->name,table,"DESC","LAST");
                }
            }
        }
    }
    return OrderByDescription("","","","");;
}

GroupByDescription makeGroupByNode(hsql::Expr* column){
    std::string table;
    if(column->table){
        table = column->table;
    }
    else{
        table = "";
    }
    return GroupByDescription(column->name,table);
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
        std::cout<<"Join Node: "<< (*e).joinType <<" "<< (*e).onLeftTable <<" "<< (*e).onLeftTableColumn <<" = "<< (*e).onRightTable <<" "<< (*e).onRightTableColumn <<std::endl;
        }
    }
    else if (auto e = std::get_if<TableBaseNode>(&root->val)) {
        std::cout<<"Table: "<< (*e).tableName<<" "<< (*e).tableAlias<<std::endl;
    }
    else if(auto e = std::get_if<WhereClauseNode>(&root->val)){
        if((*e).operatorType == "OR"){
            std::cout<<"Where: "<< (*e).table <<" "<< (*e).column << " = "<<(*e).value<<" "<< (*e).operatorType << (*e).table2 <<" "<< (*e).column2<<" = "<<(*e).value2<<std::endl;
        }
        else if((*e).operatorType == "BETWEEN"){
            std::cout<<"Where: "<< (*e).table <<" "<< (*e).column << " "<<(*e).operatorType << " "<<(*e).value<<" AND "<<(*e).value2<<std::endl;
        }
        else{
            std::cout<<"Where: "<< (*e).table <<" "<< (*e).column <<" "<< (*e).operatorType <<" "<< (*e).value << " " << (*e).table2 <<" "<< (*e).column2<<std::endl;
        }
    }
    else if(auto e = std::get_if<SelectClauseNode>(&root->val)){
        std::string star = "";
        std::string distict = "";
        std::string aggfunc = "";
        std::string alias = "";
        std::string table = "";
        std::string col = "";
        
        if((*e).star){
            star = "*";
        }
        if((*e).distinct){
            distict = "distinct";
        }
        if (!e->table.empty()) {
            table = " " + e->table + ".";
        }
        if (!e->column.empty()) {
            col = " " + e->column + " ";
        }
        if (!e->aggregateFunction.empty()) {
            aggfunc = " " + e->aggregateFunction + "(" + e->column + ")";
            col = "";
        }
        if (!e->alias.empty()) {
            alias = " as " + e->alias;
        }
        cout<<"Select:"<<distict<< table<< col<< star<< aggfunc <<alias<<std::endl;
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
            cout<<(*e).orderByList[i].table<<""<<(*e).orderByList[i].column <<" "<< (*e).orderByList[i].ordertype <<" | ";
        }
        cout<<std::endl;
    }
    else if(auto e = std::get_if<LimitClauseNode>(&root->val)){
        std::cout<<"Limit: "<< (*e).limit<<" "<< (*e).offset<<std::endl;
    }
    printAST(root->left);
    printAST(root->right);
}

ASTNode* parseQueryExpressionForSet(const hsql::SelectStatement* selectStmt){
    
    ASTNode* root = makeSelectNode(selectStmt->selectList->at(0));
    ASTNode* current = root;  
    
    //logic to parse other select clauses
    if (selectStmt->selectList) {
        for (int i=1;i<selectStmt->selectList->size();i++) {
            current->left = makeSelectNode(selectStmt->selectList->at(i));
            current = current->left;
        }
    }

    //add having
    
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

    // Join
    
    if(selectStmt->fromTable->type == hsql::kTableCrossProduct){
        current->left = exploreCrossProduct(selectStmt->fromTable);
    }
    else{
        current->left = exploreTable(selectStmt->fromTable);
    }
    
    return root;
}

ASTNode* parseQueryExpression(const hsql::SelectStatement* selectStmt){
    
    ASTNode* root = new ASTNode();
    ASTNode* current = root;    

    // logic to parse "Limit clause"
    if(selectStmt->limit){
        if(selectStmt->limit->offset){
            delete root;
            ASTNode* limitnode = new ASTNode(LimitClauseNode(to_string(selectStmt->limit->limit->ival),to_string(selectStmt->limit->offset->ival)));
            root = limitnode;
            current = root;
        }
        else{
            delete root;
            ASTNode* limitnode = new ASTNode(LimitClauseNode(to_string(selectStmt->limit->limit->ival),""));
            root = limitnode;
            current = root;
        }
    }

    // logic to parse order
    if(selectStmt->order){
        std::vector<OrderByDescription> orderByList;
        for(int i=0;i<selectStmt->order->size();i++){
            orderByList.push_back(makeOrderNode(selectStmt->order->at(i)));
        }
        ASTNode* orderbynode = new ASTNode(OrderByClauseNode(orderByList));
        if(auto e = std::get_if<std::monostate>(&current->val)){
            delete root;
            root = orderbynode;
            current = root;
        }
        else{
            current->left = orderbynode;
            current = current->left;
        }
    }

    if(auto e = std::get_if<std::monostate>(&current->val)){
            delete root;
            root = makeSelectNode(selectStmt->selectList->at(0));
            current = root; 
    }
    else{
        current->left = makeSelectNode(selectStmt->selectList->at(0));
        current = current->left; 
    }

    //logic to parse other select clauses
    if (selectStmt->selectList) {
        for (int i=1;i<selectStmt->selectList->size();i++) {
            current->left = makeSelectNode(selectStmt->selectList->at(i));
            current = current->left;
        }
    }

    //add having

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

            if(selectStmt->setOperations){
                delete root;
                root = parseQueryExpressionForSet(selectStmt);

                for(int i=0;i<selectStmt->setOperations->size();i++){
                    switch(selectStmt->setOperations->at(i)->setType){
                        case hsql::kSetUnion : {
                            ASTNode* newNode = new ASTNode(SetOperationNode("UNION"));
                            newNode->left = root;
                            newNode->right = parseQueryExpressionForSet(selectStmt->setOperations->at(i)->nestedSelectStatement);
                            root = newNode;   
                            break;
                        }
                        case hsql::kSetIntersect : {
                            ASTNode* newNode = new ASTNode(SetOperationNode("INTERSECT"));
                            newNode->left = root;
                            newNode->right = parseQueryExpressionForSet(selectStmt->setOperations->at(i)->nestedSelectStatement);
                            root = newNode; 
                            break;
                        }
                        case hsql::kSetExcept : {
                            ASTNode* newNode = new ASTNode(SetOperationNode("EXCEPT"));
                            newNode->left = root;
                            newNode->right = parseQueryExpressionForSet(selectStmt->setOperations->at(i)->nestedSelectStatement);
                            root = newNode; 
                            break;
                        }
                    }
                    if(i == selectStmt->setOperations->size()-1){
                        if(selectStmt->setOperations->at(i)->resultOrder){
                            std::vector<OrderByDescription> orderByList;
                            for(int j=0;j<selectStmt->setOperations->at(i)->resultOrder->size();j++){
                                orderByList.push_back(makeOrderNode(selectStmt->setOperations->at(i)->resultOrder->at(j)));
                            }
                            ASTNode* orderbynode = new ASTNode(OrderByClauseNode(orderByList));
                            orderbynode->left = root;
                            root = orderbynode;
                        }
                        
                        if(selectStmt->setOperations->at(i)->resultLimit){
                            if(selectStmt->setOperations->at(i)->resultLimit->offset){
                                ASTNode* limitnode = new ASTNode(LimitClauseNode(to_string(selectStmt->setOperations->at(i)->resultLimit->limit->ival),to_string(selectStmt->setOperations->at(i)->resultLimit->offset->ival)));
                                limitnode->left = root;
                                root = limitnode;
                            }
                            else{
                                ASTNode* limitnode = new ASTNode(LimitClauseNode(to_string(selectStmt->setOperations->at(i)->resultLimit->limit->ival),""));
                                limitnode->left = root;
                                root = limitnode;  
                            }
                        }
                    }
                }
            }

            else{
                delete root;
                root = parseQueryExpression(selectStmt);
            }

        }   
    }
    cout<<"Parsed Successfully"<<endl<<endl;
    return root;
}