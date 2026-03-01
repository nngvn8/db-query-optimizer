/**
 * @file base_types.hpp
 * @brief Defines fundamental data types used throughout the query processing IR.
 *
 * This file contains the definitions for basic data structures like `Table` and `TableColumn`,
 * which are used to represent schema information. It also includes types that are
 * specifically used during the parsing of JSON query plans.
 */

#include <string>
#include <vector>
#include <optional>
#include <variant>

#include <WorkItem.pb.h>

#ifndef BASETYPE_H
#define BASETYPE_H

/**
 * @namespace BaseType
 * @brief A namespace for fundamental data types used in the query IR.
 */
namespace BaseType {

    // --- Types relevant in general but also used in json parsing ---

    /**
     * @struct Table
     * @brief Represents a table in the database schema.
     */
    struct Table {
        std::string name;
        std::optional<std::string> alias;
        bool isVirtual;
        std::string schema;

        Table() {};

        /**
         * @brief Constructs a Table object.
         * @param name The name of the table.
         * @param alias An optional alias for the table.
         * @param isVirtual Whether the table is virtual.
         * @param schema The schema of the table.
         */
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

        /**
         * @brief Less-than comparison operator for tables.
         * @param other The other table to compare with.
         * @return True if this table is less than the other table, false otherwise.
         */
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

        /**
         * @brief Equality comparison operator for tables.
         * @param other The other table to compare with.
         * @return True if this table is equal to the other table, false otherwise.
         */
        bool operator==(const Table& other) const {
            return name == other.name && alias == other.alias;
        }
    };

    /**
     * @struct TableColumn
     * @brief Represents a column in a table.
     */
    // TODO: let Table Column include table object
    struct TableColumn {
        Table table;
        std::string columnName;
        ColumnType columnType;
        std::optional<std::string> alias;
        TableColumn(){}; // TODO remove later
        /**
         * @brief Constructs a TableColumn object.
         * @param table The table this column belongs to.
         * @param columnName The name of the column.
         * @param columnType The data type of the column.
         * @param alias An optional alias for the column.
         */
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

        /**
         * @brief Less-than comparison operator for table columns.
         * @param other The other table column to compare with.
         * @return True if this table column is less than the other, false otherwise.
         */
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

        /**
         * @brief Equality comparison operator for table columns.
         * @param other The other table column to compare with.
         * @return True if this table column is equal to the other, false otherwise.
         */
        bool operator==(const TableColumn& other) const {
            return table == other.table && columnName == other.columnName;
        }
    };

    // --- Types only relevant for json plan parsing ---

    /**
     * @struct PlanParams
     * @brief Represents the parameters of a plan node parsed from JSON.
     */
    struct PlanParams {
        std::optional<Table> baseTable;
        std::optional<std::string> filterPredicate;
        std::vector<std::string> sortKeys; // Empty vector if null/empty
        int parallelWorkers = 0;
        std::string index;
        std::optional<std::string> lookupKey;
        std::optional<std::string> subplanName;
    };

    /**
     * @struct Estimates
     * @brief Represents the estimated cost and cardinality of a plan node.
     */
    struct Estimates {
        float cardinality;
        float cost;
    };

    /**
     * @struct Measures
     * @brief Represents the measured performance metrics of a plan node.
     */
    struct Measures {
        float cardinality;
        float executionTime;
        std::optional<int> cacheHits;
        std::optional<int> cacheMisses;
    };

    /**
     * @struct JsonRawData
     * @brief Represents the raw data of a plan node parsed from JSON.
     */
    struct JsonRawData {
        std::string nodeType;
        std::optional<std::string> nodeOperator;
        std::optional<std::string> subPlan;

        PlanParams planParams;
        Estimates estimates;
        Measures measures;
    };

    /**
     * @struct OrderDescription
     * @brief Represents the description of an ordering criterion.
     */
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

    /**
     * @enum Join
     * @brief An enum for the different types of joins.
     */
    enum Join { INNER_JOIN, LEFT_OUTER_JOIN, RIGHT_OUTER_JOIN, FULL_OUTER_JOIN };

};

#endif
