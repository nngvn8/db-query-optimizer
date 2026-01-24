#pragma once

#include <vector>
#include <variant>
#include <stdexcept>
#include <optional>
#include <type_traits>

#include <ir/base_types.hpp>
#include <ir/ir_base.hpp>
struct GroupOp {

};

struct FetchOp {
    bool wasTableBaseNode;
};

struct AggOp {
    AggFunc aggFunc;
};

struct JoinOp {
    BaseType::Join joinType;
    CompType joinPredicate;
    std::set<BaseType::Table> tablesBelow;
};

struct FilterOp {
    CompType filterType;
    std::vector<std::variant<uint64_t, float, std::string>> filterArgs;
};

struct SortOp {

};

struct LimitOp {
    std::string limit;
    std::string offset;
};

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
    std::vector<std::string> resultHeaders;
};

struct MatOp {

};

struct BitmapOp {

};


struct IrData {
    std::vector<BaseType::TableColumn> inputColumns;
    std::vector<BaseType::TableColumn> outputCols;

    using OpInfo = std::variant<std::monostate, JoinOp, GroupOp, FetchOp, AggOp, FilterOp, SortOp, LimitOp, MapOp, SetOp, SelectOp, MatOp, BitmapOp>;
    OpInfo opInfo;

    template<typename T> bool is() const { return std::holds_alternative<T>(opInfo);};
    template<typename ViewT> std::optional<ViewT> get_view_if();
};

class JoinView {
    IrData& data;
    JoinOp& op;

public:
    explicit JoinView(IrData& data) 
        : data(data), op(std::get<JoinOp>(data.opInfo)) {}

    // Factory to create data
    static IrData create(const BaseType::TableColumn& inner, 
                         const BaseType::TableColumn& outer, 
                         const BaseType::TableColumn& out,
                         JoinOp op = {}) {
        IrData irData;
        irData.inputColumns = {inner, outer};
        irData.outputCols = {out};
        irData.opInfo = op;
        return irData;
    }

    // Accessors
    BaseType::TableColumn& inner() { return data.inputColumns[0]; }
    BaseType::TableColumn& outer() { return data.inputColumns[1]; }
    BaseType::TableColumn& output() { return data.outputCols[0]; }
    
    JoinOp& getDetails() { return op; }
};

class GroupView { 
    IrData& data;
    GroupOp& op;

public:
    explicit GroupView(IrData& data) 
        : data(data), op(std::get<GroupOp>(data.opInfo)) {}

    // Factory to create data
    static IrData create(const BaseType::TableColumn& description,
                         const BaseType::TableColumn& outputCol) {
                            IrData irData;
                            irData.inputColumns = {description};
                            irData.outputCols = {outputCol};
                            return irData;
                         }

    // Accessors
    std::vector<BaseType::TableColumn>& descriptions() { return data.inputColumns; }
    BaseType::TableColumn& outIdx() { return data.outputCols[0]; }
    BaseType::TableColumn& outCluster() { return data.outputCols[1]; }
};

class SelectView {
    IrData& data;
    SelectOp& op;

public:
    explicit SelectView(IrData& data) 
        : data(data), op(std::get<SelectOp>(data.opInfo)) {}

    // Factory to create data
    static IrData create(std::vector<BaseType::TableColumn>& resultCols,
                         bool star = false,
                         bool distinct = false,
                         std::string fileName = "",
                         const std::vector<std::string>& resultHeaders = {}) {
        IrData irData;
        irData.inputColumns = resultCols;
        irData.outputCols = resultCols;
        irData.opInfo = SelectOp{star, distinct, resultHeaders};
        return irData;
    }

    // Accessors
    std::vector<BaseType::TableColumn>& resultCols() { return data.inputColumns; }
    std::vector<std::string>& resultHeaders() { return op.resultHeaders; }
    bool star() { return op.star; }
    bool distinct() { return op.distinct; }
};

struct AggView {
    IrData& data;
    AggOp& op;

    explicit AggView(IrData& data) 
        : data(data), op(std::get<AggOp>(data.opInfo)) {}

    // Factory to create data
    static IrData create(const BaseType::TableColumn& inputCol,
                         const BaseType::TableColumn& outputCol,
                         AggFunc aggFunc) {
        IrData irData;
        irData.inputColumns = {inputCol};
        irData.outputCols = {outputCol};
        irData.opInfo = AggOp{aggFunc};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& inputCol() { return data.inputColumns[0]; }
    BaseType::TableColumn& outputCol() { return data.outputCols[0]; }
    AggFunc aggFunc() { return op.aggFunc; }

}

template<typename ViewT>
std::optional<ViewT> IrData::get_view_if() {
    if constexpr (std::is_same_v<ViewT, JoinView>) {
        if (std::holds_alternative<JoinOp>(opInfo)) {
            return JoinView(*this);
        }
    } else if constexpr (std::is_same_v<ViewT, GroupView>) {
        if (std::holds_alternative<GroupOp>(opInfo)) {
            return GroupView(*this);
        }
    }
    return std::nullopt;
}
