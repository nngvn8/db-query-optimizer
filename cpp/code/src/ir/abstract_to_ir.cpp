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
#include "ir/ir_transformer.hpp"

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

std::string removeAllWhitespace(const std::string& s) {
    std::string result;
    result.reserve(s.size());

    std::copy_if(s.begin(), s.end(), std::back_inserter(result),
                 [](unsigned char c){ return !std::isspace(c); });

    return result;
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

bool containsArithMapOp(const std::string& str) {
    return str.find_first_of("+-*/%") != std::string::npos;
}

std::tuple<std::string, char, std::string>
parseMapping(const std::string& input) {
    std::string lhs, rhs;
    char op = 0;

    size_t i = 0;

    // Skip leading spaces
    while (i < input.size() && std::isspace(input[i])) ++i;

    // Parse left operand
    while (i < input.size() &&
           input[i] != '+' &&
           input[i] != '-' &&
           input[i] != '*' &&
           input[i] != '/' &&
           input[i] != '%') {
        lhs += input[i++];
    }

    if (i >= input.size())
        throw std::invalid_argument("No operator found");

    // Trim trailing space from lhs
    while (!lhs.empty() && std::isspace(lhs.back()))
        lhs.pop_back();

    op = input[i++];

    if (op != '+' && op != '-' && op != '*' && op != '/' && op != '%')
        throw std::invalid_argument("Invalid operator");

    // Skip spaces after operator
    while (i < input.size() && std::isspace(input[i])) ++i;

    // Parse right operand
    while (i < input.size()) {
        rhs += input[i++];
    }

    // Trim trailing space from rhs
    while (!rhs.empty() && std::isspace(rhs.back()))
        rhs.pop_back();

    if (lhs.empty() || rhs.empty())
        throw std::invalid_argument("Invalid expression format");

    return {lhs, op, rhs};
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

    // RECURSION
    for (size_t i = 0; i < node->children.size(); i++) {
        abstractToIr(node->children[i]);
    }

    // ==================== SOURCE ====================
    const AbstractSource* source = std::get_if<AbstractSource>(&node->abstractData);
    if (source) {
        BaseType::Table table(source->basetable);

        std::vector<std::shared_ptr<PlanNode>> filterNodes;

        // Create Filter Node(s)
        for (const auto& filterStr : source->filters) {
            std::shared_ptr<PlanNode> filterNode = std::make_shared<PlanNode>();

            ParsedCondition conditionFields = ConditionParser::parseCondition(filterStr);
            CompType comp = IrTransformHelpers::mapStringToCompType(conditionFields.op);

            ColumnType colType = Catalog::getSSBColumnType(source->basetable, conditionFields.column);
            BaseType::TableColumn inputCol(BaseType::Table(table), conditionFields.column, colType);

            std::vector<std::variant<uint64_t,float,std::string>> args;
            for (const std::string& arg : conditionFields.arguments)
                args.push_back(arg);

            filterNode->irData = FilterView::create(inputCol, comp, std::nullopt, args, inputCol);
            filterNodes.push_back(filterNode);
        }

        // Create fetch node
        auto fetchNode = std::make_shared<PlanNode>();
        BaseType::TableColumn baseCol(table, "", ColumnType::TYPE_INTEGER);
        fetchNode->irData = FetchView::create(baseCol, true);

        // There is at least one filter
        if (!filterNodes.empty()) {
            // Wire filter nodes
            for (size_t i = 0; i < filterNodes.size() - 1; ++i) {
                filterNodes[i]->children.push_back(filterNodes[i+1]);
            }

            // Append fetch node as last child
            filterNodes.back()->children.push_back(fetchNode);

            // Make this node the first filter node
            node->irData = filterNodes[0]->irData;
            node->children = filterNodes[0]->children;
        } 
        
        // There is only the fetch node and not filters
        else {
            node->irData = fetchNode->irData;
            node->children.clear();
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

        node->irData = JoinView::create(leftCol, rightCol, outCol, BaseType::Join::INNER_JOIN, IrTransformHelpers::mapStringToCompType(joinOp));
    }

    // ==================== AGGREGATION ====================
    const AbstractAgg* agg = std::get_if<AbstractAgg>(&node->abstractData);
    if (agg) {

        std::optional<BaseType::TableColumn> mapOutCol;

        std::vector<IrData> abstractAggItems;

        // Generate map irData if map data present
        if (containsArithMapOp(agg->agg_mapping)) {
            auto [aggIn1, op, aggIn2] = parseMapping(agg->agg_mapping);

            const std::string aggTable1 = Catalog::getTableName(aggIn1);
            ColumnType aggInColType1 = Catalog::getSSBColumnType(aggTable1, aggIn1);
            BaseType::TableColumn aggInCol1(aggTable1, aggIn1, aggInColType1);

            const std::string aggTable2 = Catalog::getTableName(aggIn2);
            ColumnType aggInColType2 = Catalog::getSSBColumnType(aggTable2, aggIn2);
            BaseType::TableColumn aggInCol2(aggTable2, aggIn2, aggInColType2);

            std::string opStr(1, op);
            ArithOp aggOp = IrTransformHelpers::mapStringToArithOp(opStr);

            ColumnType mapOutColType;
            if (aggInColType1 == ColumnType::TYPE_STRING || aggInColType2 == ColumnType::TYPE_STRING
                || (op == ARITH_MOD && !(aggInColType1 == ColumnType::TYPE_INTEGER && aggInColType2 == ColumnType::TYPE_INTEGER))) {
                throw std::runtime_error("Invalid types for arithmetic operation");
            }
            if (aggInColType1 == ColumnType::TYPE_FLOAT || aggInColType2 == ColumnType::TYPE_FLOAT) {
                mapOutColType = ColumnType::TYPE_FLOAT;
            }
            else {
                mapOutColType = ColumnType::TYPE_INTEGER;
            }

            BaseType::TableColumn mapOut(BaseType::Table("MAP"), aggIn1 + opStr + aggIn2, mapOutColType);
            IrData mapIrData = MapView::create(aggInCol1, aggOp, aggInCol2, mapOut);

            abstractAggItems.push_back(mapIrData);
            mapOutCol = mapOut;
        } 

        // generate aggregation IrData if aggregation present
        if (auto aggFunc = IrTransformHelpers::mapStringToAggFunc(agg->agg_type)) {

            BaseType::TableColumn inCol;
            if (mapOutCol.has_value()) {
                inCol = mapOutCol.value();
            } else {
                const std::string& firstColName = ConditionParser::getFirstTokenString(agg->agg_mapping);
                inCol = BaseType::TableColumn(BaseType::Table(Catalog::getTableName(firstColName)), firstColName, Catalog::getSSBColumnType("", firstColName));
            }

            // Compute type of output column of aggFunc
            ColumnType outColType;
            switch (*aggFunc) {
                case AGG_COUNT: outColType = ColumnType::TYPE_INTEGER; break;
                case AGG_AVG:   outColType = ColumnType::TYPE_FLOAT;   break;
                default:        outColType = inCol.columnType;
            }

            std::string aggOutColName = agg->agg_type + "(" + inCol.columnName + ")";
            BaseType::TableColumn aggOutCol(BaseType::Table("AGG"), aggOutColName, outColType, agg->agg_alias);
            IrData aggIrData = AggView::create(inCol, aggOutCol, *aggFunc);

            abstractAggItems.push_back(aggIrData);
        }

        // Generate grouping IrData if grouping information is present
        if (!agg->grouping_cols.empty()) {

            // Generate list of table columns            
            std::vector<BaseType::TableColumn> groups;
            for (const auto& colName : agg->grouping_cols) {
                std::string tableName = Catalog::getTableName(colName);
                ColumnType type = Catalog::getSSBColumnType(tableName, colName);
                groups.emplace_back(tableName, colName, type);
            }

            // Generate grouping ir data
            BaseType::TableColumn outCol = GroupView::generateOutCol(groups);
            IrData groupByIrData = GroupView::create(groups, outCol);
            abstractAggItems.push_back(groupByIrData);
        }

        // Build and wire nodes
        if (node->children.size() != 1) 
            throw std::runtime_error("Abstract aggregation must always have exactly one child");

        std::shared_ptr<PlanNode> prevChild = node->children[0];
        for (const auto& aggItem : abstractAggItems) {
            std::shared_ptr<PlanNode> aggNode = std::make_shared<PlanNode>();
            aggNode->irData = aggItem;
            aggNode->children.push_back(prevChild);
            prevChild = aggNode;
        }

        node->irData = prevChild->irData;
        node->children = prevChild->children;
    }

    // ==================== SORT ====================
    const AbstractSort* sort = std::get_if<AbstractSort>(&node->abstractData);
    if (sort) {
        std::vector<BaseType::OrderDescription> orders;
        std::string sortColString = "SORT_";

        for (size_t i = 0; i < sort->column_names.size(); ++i) {
            std::string colName = sort->column_names[i];
            ColumnType type = Catalog::getSSBColumnType("", colName);
            std::string tableName = Catalog::getTableName(colName);

            if (tableName == "Default" && isAggColumn(colName)) {
                tableName = "AGG";
            }

            std::optional<std::string> alias_opt = sort->aliases[i].empty() ? std::nullopt : std::make_optional(sort->aliases[i]);
            BaseType::TableColumn col(BaseType::Table(tableName), colName, type, alias_opt);

            orders.emplace_back(col, sort->asc[i], false);
            sortColString += tableName + "." + sort->column_names[i] + (i < (sort->column_names.size() - 1) ? "_" : "");
        }

        BaseType::TableColumn outCol(BaseType::Table("SORT"), sortColString, ColumnType::TYPE_INTEGER);
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

            std::string colTrimName = removeAllWhitespace(colAlias.name);
            BaseType::TableColumn resultCol(BaseType::Table(tableName), colTrimName, type, colAlias.alias);
            resultCols.emplace_back(resultCol);
        }
        node->irData = SelectView::create(resultCols);
    }

    return node;
}
