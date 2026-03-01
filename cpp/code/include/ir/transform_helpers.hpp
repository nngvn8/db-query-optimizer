/**
 * @file transform_helpers.hpp
 * @brief Provides helper functions for transforming and parsing elements of the query plan IR.
 *
 * This file contains utility functions for string manipulation, parsing different parts of a query,
 * and mapping strings to IR-specific types.
 */

#pragma once

#include <string>
#include <optional>
#include <variant>
#include <tuple>
#include <vector>
#include "ir/base_types.hpp"
#include "ir/ir_types.hpp"

/**
 * @namespace IrTransformHelpers
 * @brief A namespace for IR transformation helper functions.
 */
namespace IrTransformHelpers {
    /**
     * @brief Maps a string representation of a comparison operator to a CompType enum.
     * @param op The string representation of the operator (e.g., "=", "<", ">").
     * @return The corresponding CompType.
     */
    CompType mapStringToCompType(const std::string& op);

    /**
     * @brief Maps a string representation of a join type to a BaseType::Join enum.
     * @param type The string representation of the join type (e.g., "inner", "left").
     * @return The corresponding BaseType::Join.
     */
    BaseType::Join mapStringToJoinType(std::string type);

    /**
     * @brief Maps a string representation of a relational operator to a RelOp enum.
     * @param op The string representation of the operator (e.g., "join", "scan").
     * @return The corresponding RelOp.
     */
    RelOp mapStringToRelOp(std::string op);

    /**
     * @brief Maps a string representation of an aggregate function to an AggFunc enum.
     * @param func The string representation of the function (e.g., "sum", "avg").
     * @return An optional containing the corresponding AggFunc, or std::nullopt if no mapping is found.
     */
    std::optional<AggFunc> mapStringToAggFunc(std::string func);

    /**
     * @brief Maps a string representation of an arithmetic operator to an ArithOp enum.
     * @param op The string representation of the operator (e.g., "+", "-", "*").
     * @return The corresponding ArithOp.
     */
    ArithOp mapStringToArithOp(const std::string& op);

    /**
     * @brief Parses a string value into a variant based on the given column type.
     * @param val The string value to parse.
     * @param type The ColumnType of the value.
     * @return A std::variant containing the parsed value as a uint64_t, float, or std::string.
     */
    std::variant<uint64_t, float, std::string> parseValueByType(const std::string& val, ColumnType type);

    /**
     * @brief Trims leading and trailing whitespace from a string.
     * @param s The string to trim.
     * @return The trimmed string.
     */
    std::string trim(const std::string& s);

    /**
     * @brief Converts a string to uppercase.
     * @param s The string to convert.
     * @return The uppercase string.
     */
    std::string toUpper(std::string s);

    /**
     * @brief Removes all whitespace characters from a string.
     * @param s The string to process.
     * @return The string with all whitespace removed.
     */
    std::string removeAllWhitespace(const std::string& s);

    /**
     * @brief Parses a join condition string into its constituent parts.
     * @param input The join condition string (e.g., "table1.col1 = table2.col2").
     * @return A tuple containing the left column, the operator, and the right column.
     */
    std::tuple<std::string, std::string, std::string> parseJoinCondition(const std::string& input);

    /**
     * @brief Checks if a string contains an arithmetic mapping operator.
     * @param str The string to check.
     * @return True if the string contains an arithmetic mapping operator, false otherwise.
     */
    bool containsArithMapOp(const std::string& str);

    /**
     * @brief Parses a mapping string into its constituent parts.
     * @param input The mapping string (e.g., "col1 + 5").
     * @return A tuple containing the left operand, the operator, and the right operand.
     */
    std::tuple<std::string, char, std::string> parseMapping(const std::string& input);

    /**
     * @brief Checks if a column name represents an aggregated column.
     * @param name The name of the column.
     * @return True if the column is an aggregate, false otherwise.
     */
    bool isAggColumn(const std::string& name);

    /**
     * @struct ParsedCondition
     * @brief Represents a parsed selection condition.
     */
    struct ParsedCondition {
        /// The column on which the condition is applied.
        std::string column;
        /// The comparison operator.
        std::string op;
        /// The arguments of the condition.
        std::vector<std::string> arguments;
    };

    /**
     * @struct ColWithAlias
     * @brief Represents a column with an optional alias.
     */
    struct ColWithAlias {
        /// The name of the column.
        std::string name;
        /// The alias of the column, if it exists.
        std::optional<std::string> alias;
    };

    /**
     * @namespace ConditionParser
     * @brief A namespace for functions related to parsing selection conditions.
     */
    namespace ConditionParser {
        /**
         * @brief Parses a condition string into a ParsedCondition struct.
         * @param input The condition string to parse.
         * @return A ParsedCondition struct.
         */
        ParsedCondition parseCondition(const std::string& input);

        /**
         * @brief Gets the first token from a string.
         * @param input The string to process.
         * @return The first token as a string.
         */
        std::string getFirstTokenString(const std::string& input);

        /**
         * @brief Extracts a column and its optional alias from a string.
         * @param input The string to parse.
         * @return A ColWithAlias struct.
         */
        ColWithAlias extractAlias(const std::string& input);
    }
}
