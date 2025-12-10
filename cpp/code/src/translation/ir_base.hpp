#include <string>
#include <vector>
#include "base_types.hpp"

namespace IR {

    // Table base Nodes

    class TableBaseNode {
    public:
        BaseType::Table table;

        TableBaseNode(const BaseType::Table& table): table(table) {};

        // TODO: AST Translation
    };

    class FetchNode {
    public:
        BaseType::Table table;
        bool printToFile;

        FetchNode(const BaseType::Table& table, const bool printToFile):
            table(table), printToFile(printToFile) {};

        // TODO: AST Translation
    };

    // Statement Nodes

    class StatementNode {
        public:
            BaseType::TableColumn column;
            // TODO extend this if update, delete, insert is added
    };

    class SelectNode /*: public StatementNode*/ {
    public:
        BaseType::TableColumn column;
        bool star;
        std::optional<AggFunc> aggFunction;
        bool distinct;

        SelectNode(
            const bool star,
            const BaseType::TableColumn& column,
            const AggFunc& aggFunc,
            const bool distinct):
                column(column),
                star(star),
                aggFunction(aggFunc),
                distinct(distinct) {};

        // TODO: AST Translation
        /*SelectNode(
                const bool star = false,
                const std::string& table = "",
                const std::string& column = "",
                const std::string& aggrFunc = "",
                const bool distinct = false)
                    :
                    star(star),
                    table(table),
                    column(column),
                    aggregateFunction(aggrFunc),
                    distinct(distinct)
                    {};*/
    };

    class UpdateNode : public StatementNode {
        // TODO add update statements
    };

    class InsertNode : public StatementNode {
        // TODO add insert statements
    };

    class DeleteNode : public StatementNode {
        // TODO add delete statements
    };

    // Database Function Nodes

    class AggNode {
    public:
        BaseType::TableColumn column;
        AggFunc aggFunc;

        AggNode(
            const BaseType::TableColumn& column,
            const AggFunc& aggFunc):
                column(column),
                aggFunc(aggFunc) {};

        // TODO: AST Translation
    };

    class JoinNode {
    public:
        BaseType::Join joinType;
        CompType joinPredicate;
        BaseType::TableColumn leftTableColumn;
        BaseType::TableColumn rightTableColumn;

        JoinNode(
            const BaseType::Join& joinType,
            const CompType& joinPredicate,
            const BaseType::TableColumn& leftTableColumn,
            const BaseType::TableColumn& rightTableColumn):
                joinType(joinType),
                joinPredicate(joinPredicate),
                leftTableColumn(leftTableColumn),
                rightTableColumn(rightTableColumn) {};

        // TODO: AST Translation
        /*JoinNode(
            const std::string& joinType= "",
            const std::string& onLeftTable="",
            const std::string& onLeftTableColumn="",
            const std::string& onRightTable="",
            const std::string& onRightTableColumn="")
                :getJoinType(joinType),
                onLeftTable(onLeftTable),
                onLeftTableColumn(onLeftTableColumn),
                onRightTable(onRightTable),
                onRightTableColumn(onRightTableColumn){}*/
    };

    class FilterNode {
    public:
        BaseType::TableColumn column1;
        CompType filterType;
        std::optional<BaseType::TableColumn> column2;
        std::optional<std::variant<uint64_t, float, std::string>> filterValue;
        BaseType::TableColumn outputColumn;

        FilterNode(
            const BaseType::TableColumn& column1,
            const CompType& filterType,
            const std::optional<BaseType::TableColumn>& column2,
            const std::optional<std::variant<uint64_t, float, std::string>>& filterValue,
            const BaseType::TableColumn& outputColumn):
                column1(column1),
                filterType(filterType),
                column2(column2),
                filterValue(filterValue),
                outputColumn(outputColumn) {};

        // TODO: AST Translation
        /*FilterNode(const std::string& table,
                const std::string& column,
                const std::string& operatorType = "",
                const std::string& table2 = "",
                const std::string& column2 = "",
                const std::string& value = "")
                    :table(table),
                    column(column),
                    operatorType(operatorType),
                    table2(table2),
                    column2(column2),
                    value(value)
                    {};*/
    };

    class GroupByNode {
    public:
        std::vector<BaseType::TableColumn> description;

        GroupByNode(const std::vector<BaseType::TableColumn>& description) : description(description) {};

        // TODO: AST Translation
        // GroupByNode(std::vector<GroupByDescription>& description) : description(description) {}
    };

    class SortOrderNode {
    public:
        std::vector<BaseType::OrderDescription> columnList;

        SortOrderNode(const std::vector<BaseType::OrderDescription>& columnList) : columnList(columnList) {};

        // TODO: AST Translation
        // OrderByNode(const std::vector<OrderByDescription>& list): {};
    };

    class LimitNode {
    public:
        std::string limit;
        std::string offset;

        LimitNode(
            const std::string& limit = "",
            const std::string& offset = ""):
                limit(limit),
                offset(offset) {};

        // TODO: AST Translation
    };

    class MapNode {
    public:
        BaseType::TableColumn column;
        ArithOp operatorType;
        std::variant<BaseType::TableColumn, uint64_t, float, std::string> partnerVal;

        MapNode(
            const BaseType::TableColumn& column,
            const ArithOp& operatorType,
            const std::variant<BaseType::TableColumn, uint64_t, float, std::string>& partnerVal):
                column(column),
                operatorType(operatorType),
                partnerVal(partnerVal) {};

        // TODO: AST Translation
    };

    class SetOperationNode {
    public:
        RelOp operation;
        BaseType::TableColumn innerColumn;
        BaseType::TableColumn outerColumn;

        SetOperationNode(
            const RelOp& operation,
            const BaseType::TableColumn& innerColumn,
            const BaseType::TableColumn& outerColumn):
                operation(operation),
                innerColumn(innerColumn),
                outerColumn(outerColumn) {};

        // TODO: AST Translation
    };

    class ResultNode {
    public:
        std::string fileName;
        std::vector<BaseType::TableColumn> resultColumns;
        BaseType::TableColumn resultIdx;
        std::vector<std::string> resultHeaders;

        ResultNode(
            const std::string& fileName,
            const std::vector<BaseType::TableColumn>& resultColumns,
            const BaseType::TableColumn& resultIdx,
            const std::vector<std::string>& resultHeaders):
                fileName(fileName),
                resultColumns(resultColumns),
                resultIdx(resultIdx),
                resultHeaders(resultHeaders) {};

        // TODO: AST Translation
    };

    // Column Store Specific Nodes

    class MaterializeNode {
    public:
        BaseType::TableColumn idxColumn;
        BaseType::TableColumn filterColumn;

        MaterializeNode(const BaseType::TableColumn& idxColumn, const BaseType::TableColumn& filterColumn):
            idxColumn(idxColumn), filterColumn(filterColumn) {};
    };

    class PositionList {
    public:
        BaseType::TableColumn refColumn;
        std::vector<uint16_t> list;

        PositionList(const BaseType::TableColumn& refColumn, const std::vector<uint16_t>& list):
            refColumn(refColumn), list(list) {};
    };

    class Bitmap {
    public:
        BaseType::TableColumn refColumn;
        std::vector<bool> map;

        Bitmap(const BaseType::TableColumn& refColumn, const std::vector<bool>& map):
            refColumn(refColumn), map(map) {};
    };

};
