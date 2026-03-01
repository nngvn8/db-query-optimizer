#pragma once

#include <string>
#include <optional>
#include <variant>
#include <tuple>
#include <vector>
#include "ir/base_types.hpp"
#include "ir/ir_types.hpp"

namespace IrTransformHelpers {
    CompType mapStringToCompType(const std::string& op);
    BaseType::Join mapStringToJoinType(std::string type);
    RelOp mapStringToRelOp(std::string op);
    std::optional<AggFunc> mapStringToAggFunc(std::string func);
    ArithOp mapStringToArithOp(const std::string& op);
    std::variant<uint64_t, float, std::string> parseValueByType(const std::string& val, ColumnType type);

    std::string trim(const std::string& s);
    std::string toUpper(std::string s);
    std::string removeAllWhitespace(const std::string& s);

    std::tuple<std::string, std::string, std::string> parseJoinCondition(const std::string& input);
    bool containsArithMapOp(const std::string& str);
    std::tuple<std::string, char, std::string> parseMapping(const std::string& input);
    bool isAggColumn(const std::string& name);

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
        ParsedCondition parseCondition(const std::string& input);
        std::string getFirstTokenString(const std::string& input);
        ColWithAlias extractAlias(const std::string& input);
    }
}
