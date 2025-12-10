#include <string>
#include <vector>
#include <optional>
#include <variant>

#include <WorkItem.pb.h>

#ifndef BASETYPE_H
#define BASETYPE_H

namespace BaseType {

    struct Table {
        std::string name;
        std::optional<std::string> alias;
        bool isVirtual = false;
        std::string schema;

        Table(
            const std::string& name,
            const std::string& alias = "",
            const bool isVirtual = false,
            const std::string& schema = ""):
                name(name),
                alias(alias),
                isVirtual(isVirtual),
                schema(schema) {};
    };

    // table column with table name, column name and datatype
    struct TableColumn {
        std::string tableName;
        std::string columnName;
        ColumnType columnType;
        std::optional<std::string> alias;

        TableColumn(
            const std::string& tableName,
            const std::string& columnName,
            const ColumnType& columnType,
            const std::string& alias = ""):
                tableName(tableName),
                columnName(columnName),
                columnType(columnType),
                alias(alias) {};
    };

    struct PlanParams {
        std::optional<Table> baseTable;
        std::optional<std::string> filterPredicate;
        std::vector<std::string> sortKeys; // Empty vector if null/empty
        int parallelWorkers = 0;
        std::string index;
        std::optional<std::string> lookupKey;
        std::optional<std::string> subplanName;
    };

    struct Estimates {
        float cardinality;
        float cost;
    };

    struct Measures {
        float cardinality;
        float executionTime;
        std::optional<int> cacheHits;
        std::optional<int> cacheMisses;
    };

    struct JsonRawData {
        std::string nodeType;
        std::optional<std::string> nodeOperator;
        std::optional<std::string> subPlan;

        PlanParams planParams;
        Estimates estimates;
        Measures measures;
    };

    struct OrderDescription {
        TableColumn column;
        bool orderType;
        bool nullordering;

        OrderDescription(
            const TableColumn& column,
            bool orderType = true,
            bool nullordering = false):
                column(column),
                orderType(orderType),
                nullordering(nullordering) {};
    };

    enum Join { INNER_JOIN, LEFT_OUTER_JOIN, RIGHT_OUTER_JOIN, FULL_OUTER_JOIN };

};

#endif