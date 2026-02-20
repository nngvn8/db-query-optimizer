#include <string>
#include <vector>
#include <optional>
#include <variant>


#include <WorkItem.pb.h>

#ifndef BASETYPE_H
#define BASETYPE_H

namespace BaseType {
    
    // --- Types relevant in general but also used in json parsing ---

    // Representation of a table: name, alias, isVirtual, schema
    struct Table {
        std::string name;
        std::optional<std::string> alias;
        bool isVirtual;
        std::string schema;

        Table() {};

        Table(
            const std::string& name,
            std::optional<std::string> alias = std::nullopt, 
            const bool isVirtual = false,
            const std::string& schema = "")
        : 
            name(name),
            alias((alias.has_value() && alias->empty()) ? std::nullopt : alias),
            isVirtual(isVirtual),
            schema(schema) 
        {};

        bool operator<(const Table& other) const {
            // 1. Compare Schema (Tables in different schemas are different)
            if (schema != other.schema) {
                return schema < other.schema;
            }
            // 2. Compare Table Name
            if (name != other.name) {
                return name < other.name;
            }
            // 3. Compare Alias (std::optional has built-in comparison)
            if (alias != other.alias) {
                return alias < other.alias;
            }
            // 4. Compare isVirtual (bool comparison: false < true)
            return isVirtual < other.isVirtual;
        }

        bool operator==(const Table& other) const {
            return name == other.name && alias == other.alias;
        }

    };

    // Table column with table name, column name and datatype
    // TODO: let Table Column include table object
    struct TableColumn {
        Table table;
        std::string columnName;
        ColumnType columnType;
        std::optional<std::string> alias;
        TableColumn(){}; // TODO remove later
        TableColumn(
            const Table& table, // can pass string of table name as Table constructible via string
            const std::string& columnName,
            const ColumnType& columnType, // from workitem
            const std::optional<std::string>& alias = std::nullopt)
        :
            table(table),
            columnName(columnName),
            columnType(columnType),
            alias((alias.has_value() && alias->empty()) ? std::nullopt : alias)
        {
            // if (columnName.empty()) {
            //     throw std::runtime_error("Column name cannot be empty when creating a column");
            // }

        };

        bool operator<(const TableColumn& other) const {
            // 1. Compare Table Name
            if (table.name != other.table.name) {
                return table.name < other.table.name;
            }
            // 2. If tables are same, compare Column Name
            if (columnName != other.columnName) {
                return columnName < other.columnName;
            }
            // 3. If both are same, compare Alias (optional, depending on your logic)
            // return alias < other.alias;
            return false;
        }

        bool operator==(const TableColumn& other) const {
            return table == other.table && columnName == other.columnName;
        }
    };

    // --- Types only relevant for json plan parsing ---

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