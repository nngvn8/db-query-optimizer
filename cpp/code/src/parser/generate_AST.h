#ifndef GENERATE_AST_H
#define GENERATE_AST_H

#include "hsql/SQLParser.h"
#include <iostream>
#include <variant>
#include <string>
#include <queue>
#include <memory>

enum TableRefType { TableName, TableSelect, TableJoin, TableCrossProduct};
enum JoinType { JoinInner, JoinFull, JoinLeft, JoinRight, JoinCross, JoinNatural, None };

class TableNode {
public:
    TableRefType tableRefType;
    std::string tableName;
    
    JoinType joinType;
    std::string onLeftTable;
    std::string onLeftTableColumn;
    std::string onRightTable;
    std::string onRightTableColumn;

    TableNode(TableRefType tableRefType,
            const std::string& tableName="",

            JoinType joinType= None,
            const std::string& onLeftTable="",
            const std::string& onLeftTableColumn="",
            const std::string& onRightTable="",
            const std::string& onRightTableColumn="")
                :tableRefType(tableRefType),
                tableName(tableName),
                joinType(joinType),
                onLeftTable(onLeftTable),
                onLeftTableColumn(onLeftTableColumn),
                onRightTable(onRightTable),
                onRightTableColumn(onRightTableColumn){}
};

enum ExprType { ExprSelect, ExprWhere};
enum OperatorType {OpEquals, NoneOp};

class ExprNode {
    public:
        ExprType exprType;
        std::string table;
        std::string column;

        OperatorType operatorType;
        std::string table2;
        std::string column2;
        std::string value;
        
        ExprNode(ExprType exprType,
                const std::string table,
                const std::string column,
                OperatorType operatorType = NoneOp,
                const std::string table2 = "",
                const std::string column2 = "",
                const std::string value = "")
                    :exprType(exprType),
                    table(table),
                    column(column),
                    operatorType(operatorType),
                    table2(table2),
                    column2(column2),
                    value(value)
                    {};
  
};


class ASTNode {
public:
    std::variant<
        std::monostate,
        TableNode,
        ExprNode
    > val;

    ASTNode* left;
    ASTNode* right;

    ASTNode()
        : val(std::monostate{}), left(nullptr), right(nullptr) {}

    explicit ASTNode(TableNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}

    explicit ASTNode(ExprNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}
};

ASTNode* makeExprNode(hsql::Expr* expr = nullptr);

ASTNode* makeTableNode(hsql::TableRef* table = nullptr);

std::string toExprTypeString(ExprType type);

std::string toOperatorTypeString(OperatorType type);

std::string toJoinTypeString(JoinType type);

void printAST(ASTNode* root);

void parseWhere(hsql::Expr* expr, std::queue<hsql::Expr*> &q);

void parseSelect(hsql::Expr* expr);

ASTNode* exploreTable(hsql::TableRef* table);

ASTNode* generateASTNode(const std::string& query);

#endif