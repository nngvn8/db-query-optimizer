#pragma once

#include <string>
#include <vector>
namespace IrNode {

    // TODO: materialization, map, separation aggregate from select node (because this is the case both in the json as well as well as in the work items)

    class JoinNode {
    public:
        std::string joinType;
        std::string onLeftTable;
        std::string onLeftTableColumn;
        std::string onRightTable;
        std::string onRightTableColumn;

        JoinNode(
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

    class BaseTableNode {
    public:
        std::string tableName;
        std::string tableAlias;
        
        BaseTableNode(const std::string& tableName="",
                const std::string& tableAlias="")
                    :tableName(tableName),
                    tableAlias(tableAlias){}
    };

    class FilterNode {
        public:
            std::string table;
            std::string column;
            std::string operatorType;
            std::string table2;
            std::string column2;
            std::string value;
            
            FilterNode(const std::string& table,
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
                        {};
    };

    class SelectNode {
        public:
            bool star;
            std::string table;
            std::string column;
            std::string aggregateFunction;
            bool distinct;
            
            SelectNode(
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
                        {};
    
    };

    struct GroupByDescription {
        GroupByDescription() = default;
        GroupByDescription(const std::string col, const std::string tbl)
            : column(col), table(tbl) {}

        virtual ~GroupByDescription() = default;

        std::string column;
        std::string table;
    };

    class GroupByNode {
    public:
        std::vector<GroupByDescription> description; 

        GroupByNode(std::vector<GroupByDescription>& description)
            : description(description) {}
    };

    struct OrderByDescription {
        std::string column;
        std::string table;
        std::string ordertype;
        std::string nullordering;

        OrderByDescription(
            const std::string& column = "",
            const std::string& table = "",
            const std::string& ordertype = "",
            const std::string& nullordering = ""
        ) : column(column),
            table(table),
            ordertype(ordertype),
            nullordering(nullordering)
        {}
    };

    class OrderByNode {
    public:
        std::vector<OrderByDescription> orderByList;
        
        OrderByNode(const std::vector<OrderByDescription>& list)
            : orderByList(list)
        {}
    };

    class LimitNode { // currently not 
        public:
            std::string limit;
            std::string offset;

            LimitNode(
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

}