/**
 * @file catalog.hpp
 * @brief Defines the interface for a catalog of database schema information.
 *
 * This catalog provides functions to retrieve metadata about tables and columns,
 * such as column types and uniqueness constraints.
 */

#include <WorkItem.pb.h>
#include "ir/base_types.hpp"

/**
 * @namespace Catalog
 * @brief A namespace for catalog-related functions.
 */
namespace Catalog {
    /**
     * @brief Gets the column type for a given table and column in the SSB schema.
     * @param tableName The name of the table.
     * @param columnName The name of the column.
     * @return The ColumnType of the specified column.
     */
    ColumnType getSSBColumnType(std::string tableName, std::string columnName);

    /**
     * @brief Gets the table name for a given column name.
     *
     * This function assumes that column names are unique across all tables in the schema.
     *
     * @param columnName The name of the column.
     * @return The name of the table containing the column.
     */
    std::string getTableName(std::string columnName);

    /**
     * @brief Checks if a column has a uniqueness constraint.
     * @param tableName The name of the table.
     * @param columnName The name of the column.
     * @return True if the column is unique, false otherwise.
     */
    bool isUnique(std::string tableName, std::string columnName);

    /**
     * @brief Compares two table columns and returns the column from the smaller table based on SSB table size ranking.
     * @param col1 First column to compare.
     * @param col2 Second column to compare.
     * @return The column belonging to the smaller table.
     */
    BaseType::TableColumn getSmallerColumn(const BaseType::TableColumn& col1, const BaseType::TableColumn& col2);
}