/**
 * @file generate_AST.hpp
 * @brief Defines the structure of the Abstract Syntax Tree (AST) and functions for its generation.
 *
 * This file contains the definitions for the various node types that make up the AST,
 * which is a tree representation of a parsed SQL query. It also declares the functions
 * responsible for parsing a SQL query string and constructing the corresponding AST.
 */

#ifndef GENERATE_AST_H
#define GENERATE_AST_H

#include "SQLParser.h"
#include <iostream>
#include <variant>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include <cctype>
#include <memory>

/**
 * @class TableJoinNode
 * @brief Represents a join between two tables in the AST.
 */
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

/**
 * @class TableBaseNode
 * @brief Represents a base table in the AST.
 */
class TableBaseNode {
public:
    std::string tableName;
    std::string tableAlias;

    TableBaseNode(const std::string& tableName="",
            const std::string& tableAlias="")
                :tableName(tableName),
                tableAlias(tableAlias){}
};

/**
 * @class WhereClauseNode
 * @brief Represents a condition in the WHERE clause of a query in the AST.
 */
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

/**
 * @struct SelectClauseDescription
 * @brief Describes a single item in a SELECT clause.
 */
struct SelectClauseDescription {
    SelectClauseDescription() = default;
    SelectClauseDescription(const std::string tbl,const std::string col, const std::string alias = "")
        : column(col), table(tbl), alias(alias) {}
    virtual ~SelectClauseDescription() = default;
    std::string column;
    std::string table;
    std::string alias;
};

/**
 * @class SelectClauseNode
 * @brief Represents the SELECT clause of a query in the AST.
 */
class SelectClauseNode {
    public:
        std::vector<SelectClauseDescription> description;

        SelectClauseNode(std::vector<SelectClauseDescription>& description)
            : description(description) {};
};


/**
 * @class Map
 * @brief Represents a mapping or computation in the AST.
 */
class Map {
    public:
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

/**
 * @class AggregateClauseNode
 * @brief Represents an aggregate function call in the AST.
 */
class AggregateClauseNode {
    public:
        std::string aggregateFunction;
        std::string alias;
        std::string table;
        std::string column;

        AggregateClauseNode(
                const std::string& aggrFunc = "",
                const std::string& alias = "",
                const std::string& table = "",
                const std::string& column = ""
                ):
                    aggregateFunction(aggrFunc),
                    alias(alias),
                    table(table),
                    column(column)
                    {};
};

/**
 * @struct GroupByDescription
 * @brief Describes a single item in a GROUP BY clause.
 */
struct GroupByDescription {
    GroupByDescription() = default;
    GroupByDescription(const std::string tbl,const std::string col)
        : column(col), table(tbl) {}

    virtual ~GroupByDescription() = default;

    std::string column;
    std::string table;
};

/**
 * @class GroupByClauseNode
 * @brief Represents the GROUP BY clause of a query in the AST.
 */
class GroupByClauseNode {
public:
    std::vector<GroupByDescription> description;

    GroupByClauseNode(std::vector<GroupByDescription>& description)
        : description(description) {}
};

/**
 * @struct OrderByDescription
 * @brief Describes a single item in an ORDER BY clause.
 */
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

/**
 * @class OrderByClauseNode
 * @brief Represents the ORDER BY clause of a query in the AST.
 */
class OrderByClauseNode {
public:
    std::vector<OrderByDescription> orderByList;

    OrderByClauseNode(const std::vector<OrderByDescription>& list)
        : orderByList(list)
    {}
};

/**
 * @class LimitClauseNode
 * @brief Represents the LIMIT clause of a query in the AST.
 */
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

/**
 * @class SetOperationNode
 * @brief Represents a set operation (e.g., UNION, INTERSECT) in the AST.
 */
class SetOperationNode {
    public:
        std::string setOperation;

        SetOperationNode(const std::string& setOperation)
        : setOperation(setOperation)
        {}
};

/**
 * @class ASTNode
 * @brief The main node class for the Abstract Syntax Tree.
 *
 * Each ASTNode holds a variant that can be one of the specific node types,
 * and it has pointers to its left and right children, forming a tree structure.
 */
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
        LimitClauseNode,
        Map,
        AggregateClauseNode
    > val;

    ASTNode* left;
    ASTNode* right;

    ASTNode()
        : val(std::monostate{}), left(nullptr), right(nullptr) {}

    explicit ASTNode(AggregateClauseNode node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}

    explicit ASTNode(Map node)
        : val(std::move(node)), left(nullptr), right(nullptr) {}

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

/**
 * @brief Creates an AST node from a parser expression.
 * @param expr The expression from the SQL parser.
 * @return A pointer to the newly created ASTNode.
 */
ASTNode* makeExprNode(hsql::Expr* expr = nullptr);

/**
 * @brief Creates an AST node from a parser table reference.
 * @param table The table reference from the SQL parser.
 * @return A pointer to the newly created ASTNode.
 */
ASTNode* makeTableNode(hsql::TableRef* table = nullptr);

/**
 * @brief Prints the AST to the console for debugging.
 * @param root The root of the AST to print.
 */
void printAST(ASTNode* root);

/**
 * @brief Parses a WHERE clause expression recursively.
 * @param expr The expression from the SQL parser.
 * @param q A queue to hold expressions to be processed.
 */
void parseWhere(hsql::Expr* expr, std::queue<hsql::Expr*> &q);

/**
 * @brief Parses a SELECT list expression.
 * @param expr The expression from the SQL parser.
 */
void parseSelect(hsql::Expr* expr);

/**
 * @brief Explores a table reference recursively to build the table part of the AST.
 * @param table The table reference from the SQL parser.
 * @return A pointer to the root of the table-related subtree of the AST.
 */
ASTNode* exploreTable(hsql::TableRef* table);

/**
 * @brief Generates an AST from a SQL query string.
 * @param query The SQL query string.
 * @return A pointer to the root of the generated AST.
 */
ASTNode* generateASTNode(const std::string& query);

/**
 * @brief Creates a WHERE clause node from a parser expression.
 * @param expr The expression from the SQL parser.
 * @return A pointer to the newly created ASTNode.
 */
ASTNode* makeWhereNode(hsql::Expr* expr);

/**
 * @brief Creates an ORDER BY description from a parser order description.
 * @param order The order description from the SQL parser.
 * @return An OrderByDescription struct.
 */
OrderByDescription makeOrderNode(hsql::OrderDescription* order);

/**
 * @brief Creates a GROUP BY description from a parser expression.
 * @param column The expression from the SQL parser.
 * @return A GroupByDescription struct.
 */
GroupByDescription makeGroupByNode(hsql::Expr* column);

/**
 * @brief Parses a full select statement into an AST.
 * @param selectStmt The select statement from the SQL parser.
 * @return A pointer to the root of the generated AST.
 */
ASTNode* parseQueryExpression(const hsql::SelectStatement* selectStmt);

#endif
