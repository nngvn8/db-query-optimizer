#include "ir/transform_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <stdexcept>

namespace IrTransformHelpers {

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

    BaseType::Join mapStringToJoinType(std::string type) {
        std::transform(type.begin(), type.end(), type.begin(), ::toupper);
        if (type.find("LEFT") != std::string::npos) return BaseType::LEFT_OUTER_JOIN;
        if (type.find("RIGHT") != std::string::npos) return BaseType::RIGHT_OUTER_JOIN;
        if (type.find("FULL") != std::string::npos) return BaseType::FULL_OUTER_JOIN;
        return BaseType::INNER_JOIN;
    }

    RelOp mapStringToRelOp(std::string op) {
        std::transform(op.begin(), op.end(), op.begin(), ::toupper);
        if (op == "INTERSECT") return REL_INTERSECTION;
        if (op == "EXCEPT") return REL_NEGATION;
        return REL_UNION;
    }

    std::optional<AggFunc> mapStringToAggFunc(std::string func) {
        if (func.empty()) return std::nullopt;
        std::transform(func.begin(), func.end(), func.begin(), ::toupper);
        if (func == "SUM") return AGG_SUM;
        if (func == "COUNT") return AGG_COUNT;
        if (func == "MIN") return AGG_MIN;
        if (func == "MAX") return AGG_MAX;
        if (func == "AVG") return AGG_AVG;
        return std::nullopt;
    }

    ArithOp mapStringToArithOp(const std::string& op) {
        if (op == "+") return ARITH_ADD;
        if (op == "-") return ARITH_SUB;
        if (op == "*") return ARITH_MUL;
        if (op == "/") return ARITH_DIV;
        if (op == "%") return ARITH_MOD;
        return ARITH_ADD;
    }

    std::variant<uint64_t, float, std::string> parseValueByType(const std::string& val, ColumnType type) {
        if (type == ColumnType::TYPE_INTEGER) {
            try { return static_cast<uint64_t>(std::stoull(val)); }
            catch (...) { return static_cast<uint64_t>(0); }
        }
        if (type == ColumnType::TYPE_FLOAT) {
            try { return std::stof(val); }
            catch (...) { return 0.0f; }
        }
        // Fallback / String
        return val;
    }

    std::string trim(const std::string& s) {
        size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
            ++start;

        size_t end = s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
            --end;

        return s.substr(start, end - start);
    }

    std::string toUpper(std::string s) {
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

    std::tuple<std::string, std::string, std::string> parseJoinCondition(const std::string& input) {
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

    std::tuple<std::string, char, std::string> parseMapping(const std::string& input) {
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

    namespace ConditionParser {
        // helper trim
        static inline std::string _trim(const std::string& s) {
            size_t start = s.find_first_not_of(" \t\n\r");
            size_t end = s.find_last_not_of(" \t\n\r");
            return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        }

        ParsedCondition parseCondition(const std::string& input) {
            std::string s = _trim(input);
            std::smatch match;

            // BETWEEN
            std::regex betweenRegex(R"(^(\w+)\s+BETWEEN\s+(.+)\s+AND\s+(.+)$)", std::regex::icase);
            if (std::regex_match(s, match, betweenRegex)) {
                return {match[1], "BETWEEN", { _trim(match[2]), _trim(match[3]) }};
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
                    args.push_back(_trim(it->str()));
                }

                return {match[1], "IN", args};
            }

            // Standard comparison operators
            std::regex compRegex(R"(^(\w+)\s*(=|<>|!=|<=|>=|<|>)\s*(.+)$)", std::regex::icase);
            if (std::regex_match(s, match, compRegex)) {
                return {match[1], match[2], { _trim(match[3]) }};
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
            std::string s = _trim(input);

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
}
