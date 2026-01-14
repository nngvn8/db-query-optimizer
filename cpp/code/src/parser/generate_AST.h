#ifndef GENERATE_AST_H
#define GENERATE_AST_H

#include "hsql/SQLParser.h"
#include <iostream>
#include <variant>
#include <optional>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include <cctype>
#include <memory>

class TableJoinNode {
public:
    std::string joinType;
    std::string onLeftTable;
    std::string onLeftTableColumn;
    std::string onRightTable;
    std::string onRightTableColumn;

    TableJoinNode(
            const std::string& joinType= "",
            const std::string& onLeftTable="",
            const std::string& onLeftTableColumn="",
            const std::string& onRightTable="",
            const std::string& onRightTableColumn="")
                :joinType(joinType),
                onLeftTable(onLeftTable),
                onLeftTableColumn(onLeftTableColumn),
                onRightTable(onRightTable),
                onRightTableColumn(onRightTableColumn){}
};

class TableBaseNode {
public:
    std::string tableName;
    std::string tableAlias;
    
    TableBaseNode(const std::string& tableName="",
            const std::string& tableAlias="")
                :tableName(tableName),
                tableAlias(tableAlias){}
};

class WhereClauseNode {
    public:
        std::string table;
        std::string column;
        std::string operatorType;
        std::string table2;
        std::string column2;
        std::string value;
        std::string value2;
        
        
        WhereClauseNode(const std::string& table="",
                const std::string& column="",
                const std::string& operatorType = "",
                const std::string& table2 = "",
                const std::string& column2 = "",
                const std::string& value = "",
                const std::string& value2 = ""
            )
                    :table(table),
                    column(column),
                    operatorType(operatorType),
                    table2(table2),
                    column2(column2),
                    value(value),
                    value2(value2)
                    {};
};

struct Map {
    std::string table1;
    std::string column1;
    std::string table2;    
    std::string column2;
    std::string operatorType;
    std::string value;

    Map(
        const std::string& table1 = "",
        const std::string& column1 = "",
        const std::string& table2 = "",
        const std::string& column2 = "",
        const std::string& operatorType = "",
        const std::string& value = ""
    )
        : column1(column1),
          table1(table1),
          column2(column2),
          table2(table2),
          operatorType(operatorType),
          value(value)
    {}
};


class SelectClauseNode {
    public:
        bool star;
        std::string table;
        std::string column;
        std::string aggregateFunction;
        std::string alias;
        std::optional<Map> map;
        bool distinct;
        
        SelectClauseNode(
                const bool star = false,
                const std::string& table = "",
                const std::string& column = "",
                const std::string& aggrFunc = "",
                const std::string& alias = "",
                std::optional<Map> map = std::nullopt,
                const bool distinct = false)
                    :
                    star(star),
                    table(table),
                    column(column),
                    aggregateFunction(aggrFunc),
                    alias(alias),
                    map(std::move(map)),
                    distinct(distinct)
                    {};
  
};


struct GroupByDescription {
    GroupByDescription() = default;
    GroupByDescription(const std::string tbl,const std::string col)
        : column(col), table(tbl) {}

    virtual ~GroupByDescription() = default;

    std::string column;
    std::string table;
};

class GroupByClauseNode {
public:
    std::vector<GroupByDescription> description; 

    GroupByClauseNode(std::vector<GroupByDescription>& description)
        : description(description) {}
};

struct OrderByDescription {
    std::string column;
    std::string table;
    std::string ordertype;
    std::string nullordering;

    OrderByDescription(
        const std::string& table = "",
        const std::string& column = "",
        const std::string& ordertype = "",
        const std::string& nullordering = ""
    ) : table(table),
        column(column),
        ordertype(ordertype),
        nullordering(nullordering)
    {}
};

class OrderByClauseNode {
public:
    std::vector<OrderByDescription> orderByList;
    
    OrderByClauseNode(const std::vector<OrderByDescription>& list)
        : orderByList(list)
    {}
};

class LimitClauseNode {
    public:
        std::string limit;
        std::string offset;

        LimitClauseNode(
        const std::string& limit = "",
        const std::string& offset = "") 
        : limit(limit),
        offset(offset)
        {}
};

class SetOperationNode {
    public:
        std::string setOperation;

        SetOperationNode(const std::string& setOperation) 
        : setOperation(setOperation)
        {}
};

class ASTNode {
public:
    std::variant<
        std::monostate,
        SetOperationNode,
        TableJoinNode,
        TableBaseNode,
        WhereClauseNode,
        SelectClauseNode,
        GroupByClauseNode,
        OrderByClauseNode,
        LimitClauseNode
    > val;

    ASTNode* left;
    ASTNode* right;

    ASTNode()
        : val(std::monostate{}), left(nullptr), right(nullptr) {}

    explicit ASTNode(SetOperationNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}

    explicit ASTNode(TableJoinNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}

    explicit ASTNode(TableBaseNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}

    explicit ASTNode(WhereClauseNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}

    explicit ASTNode(SelectClauseNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}

    explicit ASTNode(GroupByClauseNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}

    explicit ASTNode(OrderByClauseNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}
    
    explicit ASTNode(LimitClauseNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}

    ~ASTNode() {
        delete left;
        delete right; 
    }
};

ASTNode* makeExprNode(hsql::Expr* expr = nullptr);

ASTNode* makeTableNode(hsql::TableRef* table = nullptr);

void printAST(ASTNode* root);

void parseWhere(hsql::Expr* expr, std::queue<hsql::Expr*> &q);

void parseSelect(hsql::Expr* expr);

ASTNode* exploreTable(hsql::TableRef* table);

ASTNode* generateASTNode(const std::string& query);

ASTNode* makeWhereNode(hsql::Expr* expr);

ASTNode* makeSelectNode(hsql::Expr* expr);

ASTNode* makeTableNode(hsql::TableRef* table);

OrderByDescription makeOrderNode(hsql::OrderDescription* order);

GroupByDescription makeGroupByNode(hsql::Expr* column);

ASTNode* parseQueryExpression(const hsql::SelectStatement* selectStmt);

ASTNode* parseQueryExpressionForSet(const hsql::SelectStatement* selectStmt);

#endif