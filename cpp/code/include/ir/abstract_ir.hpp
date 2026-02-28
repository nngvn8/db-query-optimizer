#pragma once
#include <string>
#include <vector>
#include <variant>
#include "ir/plan_node.hpp"

struct AbstractSource {
    std::string basetable;
    std::vector<std::string> filters;
};

struct AbstractJoin {
    std::string left_table;
    std::string right_table;
    std::string condition;
};

struct AbstractAgg {
    std::string agg_type;
    std::string agg_mapping;
    std::string agg_alias;
};

struct AbstractSort {
    std::vector<std::string> column_names;
    std::vector<std::string> aliases;
    std::vector<bool> asc;

};

struct AbstractResult {
    std::vector<std::string> output_cols;
};

struct GetNodeName {
    std::string operator()(const std::monostate&) { return "Empty"; }
    std::string operator()(const AbstractSource&) { return "AbstractSource"; }
    std::string operator()(const AbstractJoin&)   { return "AbstractJoin"; }
    std::string operator()(const AbstractAgg&)    { return "AbstractAgg"; }
    std::string operator()(const AbstractSort&)   { return "AbstractSort"; }
    std::string operator()(const AbstractResult&) { return "AbstractResult"; }
};