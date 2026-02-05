#pragma once

#include <vector>
#include <variant>
#include <optional>
#include <string>
#include <set>
#include <type_traits>

#include "ir/base_types.hpp"


struct GroupOp {
    std::vector<bool> sortOrders;
    std::optional<BaseType::TableColumn> aggCol;
    std::optional<BaseType::TableColumn> aggResultCol;
    bool storeExtends = true;

};

struct FetchOp {
    bool wasTableBaseNode;
};

struct AggOp {
    AggFunc aggFunc;
};

struct JoinOp {
    BaseType::Join joinType; // currently not processed by the system
    CompType joinPredicate;
    BaseType::TableColumn outputCol;
};

struct SemiJoinOp {
    BaseType::Join joinType; // currently not processed by the system
    CompType joinPredicate;
    BaseType::TableColumn outputCol;
};

struct FilterOp {
    CompType filterType;
    std::vector<std::variant<uint64_t, float, std::string>> filterArgs;
};

struct SortOp {
    std::vector<BaseType::OrderDescription> columnList;
    std::optional<BaseType::TableColumn> existingIdx;
};

// struct LimitOp currently not supported by system

struct MapOp {
    ArithOp operatorType;
    std::variant<BaseType::TableColumn, uint64_t, float, std::string> partnerVal;
};

struct SetOp {
    RelOp operation;
};

struct SelectOp {
    bool star;
    bool distinct;
    std::optional<BaseType::TableColumn> resultIdx;
    std::vector<std::string> resultHeaders;
};

struct MatOp {

};

// currently not supported by the system
// struct BitmapOp {
//     std::vector<bool> map;
// };
