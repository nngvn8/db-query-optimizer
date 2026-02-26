#include <optional>
#include <string>
#include <tuple>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <variant>
#include <memory>
#include <vector>
#include <regex>

#include "ir/abstract_to_ir.hpp"
#include "WorkItem.pb.h"
#include "ir/ir_views.hpp"
#include "ir/catalog.hpp"

static inline std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        ++start;

    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;

    return s.substr(start, end - start);
}

static inline std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return s;
}

std::tuple<std::string, std::string, std::string>
parseJoinCondition(const std::string& input) {
    std::string expr = trim(input);
    std::string upper = toUpper(expr);

    // ---- BETWEEN (added) ----
    size_t pos = upper.find(" BETWEEN ");
    if (pos != std::string::npos) {
        std::string left  = trim(expr.substr(0, pos));
        std::string right = trim(expr.substr(pos + 9)); // length of " BETWEEN "
        return { left, right, "BETWEEN" };
    }

    // ---- IN (added) ----
    pos = upper.find(" IN ");
    if (pos != std::string::npos) {
        std::string left  = trim(expr.substr(0, pos));
        std::string right = trim(expr.substr(pos + 4)); // length of " IN "
        return { left, right, "IN" };
    }

    // ---- Symbol operators ----
    const std::string ops[] = { "<=", ">=", "!=", "=", "<", ">" };

    for (const auto& op : ops) {
        pos = expr.find(op);
        if (pos != std::string::npos) {
            std::string left  = trim(expr.substr(0, pos));
            std::string right = trim(expr.substr(pos + op.length()));
            return { left, right, op };
        }
    }
    throw std::invalid_argument("Unsupported condition");
}

// HELPERS
CompType mapStringToCompType(const std::string& op) {
    if (op == "=") return COMP_EQ;
    if (op == "<") return COMP_LT;
    if (op == "<=") return COMP_LE;
    if (op == ">") return COMP_GT;
    if (op == ">=") return COMP_GE;
    if (op == "!=" || op == "<>") return COMP_NE;
    if (op == "BETWEEN") return COMP_BETWEEN; // Simplification
    if (op == "IN") return COMP_IN;
    return COMP_EQ; // Default fallback
}

std::optional<AggFunc> mapStringToAggFunc(const std::string& func) {
    if (func.empty())
        return std::nullopt;

    std::string f = func;
    std::transform(f.begin(), f.end(), f.begin(), ::toupper);

    if (f == "SUM") return AGG_SUM;
    if (f == "COUNT") return AGG_COUNT;
    if (f == "MIN") return AGG_MIN;
    if (f == "MAX") return AGG_MAX;
    if (f == "AVG") return AGG_AVG;

    return std::nullopt;
}

bool isAggColumn(const std::string& name) {
    std::string start = name.substr(0, 5);
    if (start == "COUNT") {
        return true;
    }
    start = name.substr(0, 3);
    return (start == "SUM" || start == "MIN" || start == "MAX" || start == "AVG");
}

struct ParsedCondition {
    std::string column;
    std::string op;
    std::vector<std::string> arguments;
};

struct ColWithAlias {
    std::string name;
    std::optional<std::string> alias;
};

namespace ConditionParser {
    // helper trim
    static inline std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    }

    ParsedCondition parseCondition(const std::string& input) {
        std::string s = trim(input);
        std::smatch match;

        // BETWEEN
        std::regex betweenRegex(R"(^(\w+)\s+BETWEEN\s+(.+)\s+AND\s+(.+)$)", std::regex::icase);
        if (std::regex_match(s, match, betweenRegex)) {
            return {match[1], "BETWEEN", { trim(match[2]), trim(match[3]) }};
        }

        // IN
        std::regex inRegex(R"(^(\w+)\s+IN\s*\((.+)\)$)", std::regex::icase);
        if (std::regex_match(s, match, inRegex)) {
            std::vector<std::string> args;
            std::string values = match[2];
            std::regex valueRegex(R"([^,]+)");
            auto begin = std::sregex_iterator(values.begin(), values.end(), valueRegex);
            auto end = std::sregex_iterator();

            for (auto it = begin; it != end; ++it) {
                args.push_back(trim(it->str()));
            }

            return {match[1], "IN", args};
        }

        // Standard comparison operators
        std::regex compRegex(R"(^(\w+)\s*(=|<>|!=|<=|>=|<|>)\s*(.+)$)", std::regex::icase);
        if (std::regex_match(s, match, compRegex)) {
            return {match[1], match[2], { trim(match[3]) }};
        }

        throw std::invalid_argument("Invalid SQL condition format");
    }

    std::string getFirstTokenString(const std::string& input) {
        size_t i = 0;

        // Skip leading whitespace
        while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i]))) {
            ++i;
        }

        // Start of token
        size_t start = i;

        // Read until next whitespace
        while (i < input.size() && !std::isspace(static_cast<unsigned char>(input[i]))) {
            ++i;
        }

        return input.substr(start, i - start);
    }

    ColWithAlias extractAlias(const std::string& input) {
        std::string s = trim(input);

        // Make uppercase copy for case-insensitive search
        std::string upper = s;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                    [](unsigned char c){ return std::toupper(c); });

        size_t pos = upper.find(" AS ");
        if (pos == std::string::npos) {
            // NO AS Found
            return { input, std::nullopt };
        }

        // Extract everything after " AS "
        std::string column = s.substr(0, pos);
        std::string alias = s.substr(pos + 4);
        return { column, alias };
    }
}

std::shared_ptr<PlanNode> AbstractToIr::abstractToIr(std::shared_ptr<PlanNode> node) {

    // ==================== SOURCE ====================
    const AbstractSource* source = std::get_if<AbstractSource>(&node->abstractData);
    if (source) {
        BaseType::Table table(source->basetable);

        // Base fetch node
        BaseType::TableColumn baseCol(table, "", ColumnType::TYPE_INTEGER);
        node->irData = FetchView::create(baseCol, true);

        // Filter Node(s)
        for (const auto& filterStr : source->filters) {
            ParsedCondition conditionFields = ConditionParser::parseCondition(filterStr);
            CompType comp = mapStringToCompType(conditionFields.op);

            ColumnType colType = Catalog::getSSBColumnType(source->basetable, conditionFields.column);
            BaseType::TableColumn inputCol(BaseType::Table(table), conditionFields.column, colType);

            std::vector<std::variant<uint64_t,float,std::string>> args;
            for (const std::string& arg : conditionFields.arguments)
                args.push_back(arg);

            auto filterNode = std::make_shared<PlanNode>();
            filterNode->irData = FilterView::create(inputCol, comp, std::nullopt, args, inputCol);

            node->children.push_back(filterNode);
        }
    }

    // ==================== JOIN ====================
    const AbstractJoin* join = std::get_if<AbstractJoin>(&node->abstractData);
    if (join) {
        auto [leftColName, rightColName,joinOp] =
            parseJoinCondition(join->condition);

        ColumnType leftType = Catalog::getSSBColumnType(join->left_table, leftColName);
        ColumnType rightType = Catalog::getSSBColumnType(join->right_table, rightColName);

        BaseType::TableColumn leftCol(BaseType::Table(join->left_table), leftColName, leftType);
        BaseType::TableColumn rightCol(BaseType::Table(join->right_table), rightColName, rightType);
        BaseType::TableColumn outCol(BaseType::Table("JOIN"), leftColName + joinOp + rightColName, ColumnType::TYPE_INTEGER);

        node->irData = JoinView::create(leftCol, rightCol, outCol, BaseType::Join::INNER_JOIN, mapStringToCompType(joinOp));
    }

    // ==================== AGGREGATION ====================
    const AbstractAgg* agg = std::get_if<AbstractAgg>(&node->abstractData);
    if (agg) {
        auto aggFunc = mapStringToAggFunc(agg->agg_type);
        ColumnType inColType = Catalog::getSSBColumnType("", agg->agg_mapping);

        ColumnType outColType;
        switch (*aggFunc) {
            case AGG_COUNT: outColType = ColumnType::TYPE_INTEGER; break;
            case AGG_AVG:   outColType = ColumnType::TYPE_FLOAT;   break;
            default:        outColType = inColType;
        }

        const std::string& firstColName = ConditionParser::getFirstTokenString(agg->agg_mapping);
        BaseType::TableColumn aggInCol(BaseType::Table(Catalog::getTableName(firstColName)), firstColName, inColType);
        BaseType::TableColumn aggOutCol(BaseType::Table("AGG"), agg->agg_alias, outColType);

        node->irData = AggView::create(aggInCol, aggOutCol, *aggFunc);
    }

    // ==================== SORT ====================
    const AbstractSort* sort = std::get_if<AbstractSort>(&node->abstractData);
    if (sort) {
        std::vector<BaseType::OrderDescription> orders;
        std::string sortColString = "SORT_";

        for (size_t i = 0; i < sort->column_names.size(); ++i) {
            ColumnType type = Catalog::getSSBColumnType("", sort->column_names[i]);
            std::string tableName = Catalog::getTableName(sort->column_names[i]);

            BaseType::TableColumn col(BaseType::Table(tableName), sort->column_names[i], type);

            orders.emplace_back(col, sort->asc[i], false);
            sortColString += tableName + "." + sort->column_names[i] + (i < (sort->column_names.size() - 1) ? "_" : "");
        }

        BaseType::TableColumn outCol(BaseType::Table(""), sortColString, ColumnType::TYPE_INTEGER);
        node->irData = SortOrderView::create(orders, outCol);
    }

    // ==================== RESULT (SELECT) ====================
    const AbstractResult* result = std::get_if<AbstractResult>(&node->abstractData);
    if (result) {
        std::vector<BaseType::TableColumn> resultCols;
        for (const auto& colName : result->output_cols) {
            std::string tableName = Catalog::getTableName(colName);
            ColumnType type = Catalog::getSSBColumnType("", colName);
            const ColWithAlias& colAlias = ConditionParser::extractAlias(colName);

            if (tableName == "Default" && isAggColumn(colName)) {
                tableName = "AGG";
            }

            BaseType::TableColumn resultCol(BaseType::Table(tableName), colAlias.name, type, colAlias.alias);
            resultCols.emplace_back(resultCol);
        }
        node->irData = SelectView::create(resultCols);
    }

    // RECURSION
    for (size_t i = 0; i < node->children.size(); i++) {
        abstractToIr(node->children[i]);
    }
    return node;
}
