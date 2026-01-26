#pragma once

#include <vector>
#include <variant>
#include <stdexcept>
#include <optional>
#include <type_traits>

#include <ir/base_types.hpp>
#include <ir/ir_base.hpp>
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
    BaseType::Join joinType;
    CompType joinPredicate;
    std::set<BaseType::Table> tablesBelow;
};

struct FilterOp {
    CompType filterType;
    std::vector<std::variant<uint64_t, float, std::string>> filterArgs;
};

struct SortOp {
    std::vector<BaseType::OrderDescription> columnList;
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


struct IrData {
    std::vector<BaseType::TableColumn> inputColumns;
    std::vector<BaseType::TableColumn> outputCols;

    using OpInfo = std::variant<std::monostate, JoinOp, GroupOp, FetchOp, AggOp, FilterOp, SortOp, MapOp, SetOp, SelectOp, MatOp>;
    OpInfo opInfo;

    template<typename T> bool is() const { return std::holds_alternative<T>(opInfo);};
    template<typename ViewT> std::optional<ViewT> get_view_if();
};

class JoinView {
    IrData& data;
    JoinOp& op;

public:
    using OpType = JoinOp;

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

class SelectView {
    IrData& data;
    SelectOp& op;

public:
    using OpType = SelectOp;

    explicit SelectView(IrData& data) 
        : data(data), op(std::get<SelectOp>(data.opInfo)) {}

    // Factory to create data
    static IrData create(const std::vector<BaseType::TableColumn>& resultCols,
                         bool star = false,
                         bool distinct = false,
                         std::string fileName = "",
                         std::optional<BaseType::TableColumn> resultIdx = std::nullopt,
                         const std::vector<std::string>& resultHeaders = {}) {
        IrData irData;
        irData.inputColumns = resultCols;
        irData.outputCols = resultCols;
        irData.opInfo = SelectOp{star, distinct, resultIdx, resultHeaders};
        return irData;
    }

    // Accessors
    std::vector<BaseType::TableColumn>& resultCols() { return data.inputColumns; }
    std::vector<std::string>& resultHeaders() { return op.resultHeaders; }
    bool& star() { return op.star; }
    bool& distinct() { return op.distinct; }
    std::optional<BaseType::TableColumn>& resultIdx() { return op.resultIdx; }
};

class AggView {
    IrData& data;
    AggOp& op;

public:
    using OpType = AggOp;

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
    BaseType::TableColumn& colToAgg() { return data.inputColumns[0]; }
    BaseType::TableColumn& aggResultCol() { return data.outputCols[0]; }
    AggFunc& aggFunc() { return op.aggFunc; }

};

class FetchView {
    IrData& data;
    FetchOp& op;

public:
    using OpType = FetchOp;

    explicit FetchView(IrData& data) 
        : data(data), op(std::get<FetchOp>(data.opInfo)) {}

    // Factory to create data
    static IrData create(const BaseType::TableColumn& inputCol, bool wasTableBaseNode = false) {
        IrData irData;
        irData.inputColumns = {inputCol};
        irData.outputCols = {inputCol};
        irData.opInfo = FetchOp{wasTableBaseNode};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& inputCol() { return data.inputColumns[0]; }
    BaseType::TableColumn& outputCol() { return data.outputCols[0]; }

};

class FilterView {
    IrData& data;
    FilterOp& op;

public:
    using OpType = FilterOp;

    explicit FilterView(IrData& data) 
        : data(data), op(std::get<FilterOp>(data.opInfo)) {}

    // Factory to create data
    static IrData create(
        const BaseType::TableColumn& col1,
        const CompType& filterType,
        const std::optional<BaseType::TableColumn>& col2,
        const std::vector<std::variant<uint64_t, float, std::string>>& filterArgs,
        const BaseType::TableColumn& outputCol
    ) {
        IrData irData;
        irData.inputColumns = {col1};
        if(col2.has_value()){
            irData.inputColumns.push_back(col2.value());
        }
        irData.outputCols = {outputCol};
        irData.opInfo = FilterOp{filterType, filterArgs};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& col1() { return data.inputColumns[0]; }
    BaseType::TableColumn* col2() { return data.inputColumns.size() > 1 ? &data.inputColumns[1] : nullptr; }
    CompType& filterType() { return op.filterType; }
    std::vector<std::variant<uint64_t, float, std::string>>& filterArgs() { return op.filterArgs; }

};


class GroupView {
    IrData& data;
    GroupOp& op;

public:
    using OpType = GroupOp;

    explicit GroupView(IrData& data) 
        : data(data), op(std::get<GroupOp>(data.opInfo)) {}

    
    // Factory to create data (no aggregation)
    static IrData create(const std::vector<BaseType::TableColumn>& groupingCols, const BaseType::TableColumn& outputCol) {
        IrData irData;
        irData.inputColumns = {groupingCols};
        irData.outputCols = {outputCol};
        std::vector<bool> sortOrders(groupingCols.size(), true);
        irData.opInfo = GroupOp{sortOrders};
        return irData;
    }

    // Factory to create data (with aggregation)
    static IrData create(const std::vector<BaseType::TableColumn>& groupingCols, const BaseType::TableColumn& outputCol, const BaseType::TableColumn& aggCol, const BaseType::TableColumn& aggResultCol) {
        IrData irData;
        irData.inputColumns = groupingCols;
        irData.outputCols = {outputCol};
        std::vector<bool> sortOrders(groupingCols.size(), true);
        irData.opInfo = GroupOp{sortOrders, aggCol, aggResultCol};
        return irData;
    }

    // Accessors
    std::vector<BaseType::TableColumn>& groupingCols() { return data.inputColumns; }
    BaseType::TableColumn& outputIdx() { return data.outputCols[0]; }
    BaseType::TableColumn& outputIdxExt() {
        BaseType::TableColumn outIdxExt = outputIdx();
        outIdxExt.columnName += "_ext";
        return outIdxExt;
    }
    BaseType::TableColumn& outputCluster() {     
        BaseType::TableColumn outCluster = outputIdx();
        outCluster.columnName += "_clus";
        return outCluster;
    }
    std::vector<bool>& sortOrders() { return op.sortOrders; }
    std::optional<BaseType::TableColumn>& aggCol() { return op.aggCol; }
    std::optional<BaseType::TableColumn>& aggResultCol() { return op.aggResultCol; }
    bool& storeExtends() { return op.storeExtends; }

};

class SortOrderView {
    IrData& data;
    SortOp& op;

public:
    using OpType = SortOp;

    explicit SortOrderView(IrData& data) 
        : data(data), op(std::get<SortOp>(data.opInfo)) {}

    // Factory to create data
    static IrData create(const std::vector<BaseType::OrderDescription>& orderDescriptions, const BaseType::TableColumn& idxOutput) {
        IrData irData;

        irData.inputColumns = std::vector<BaseType::TableColumn>(orderDescriptions.size());
        for (int i = 0; i < orderDescriptions.size(); ++i) {
            irData.inputColumns[i] = orderDescriptions[i].column;
        }

        irData.outputCols = {idxOutput};
        irData.opInfo = SortOp{orderDescriptions};
        return irData;
    }
};

// class Limit - omitted !! (not supported by system)


class SetOpView {
    IrData& data;
    SetOp& op;

public:
    using OpType = SetOp;

    explicit SetOpView(IrData& data) 
        : data(data), op(std::get<SetOp>(data.opInfo)) {}

    // Factory to create data
    static IrData create(
        const BaseType::TableColumn& inner,
        const BaseType::TableColumn& outer,
        const BaseType::TableColumn& output,
        const RelOp& operation
    ) {
        IrData irData;
        irData.inputColumns = {inner, outer};
        irData.outputCols = {output};
        irData.opInfo = SetOp{operation};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& inner() { return data.inputColumns[0]; }
    BaseType::TableColumn& outer() { return data.inputColumns[1]; }
    BaseType::TableColumn& output() { return data.outputCols[0]; }
    RelOp& operation() { return op.operation; }
    
};

class MaterializeView {
    IrData& data;
    MatOp& op;

public:
    using OpType = MatOp;

    explicit MaterializeView(IrData& data) 
        : data(data), op(std::get<MatOp>(data.opInfo)) {}
    
    static IrData create(const BaseType::TableColumn& idxCol, const BaseType::TableColumn& filterCol, const BaseType::TableColumn& outputCol) {
        IrData irData;
        irData.inputColumns = {idxCol, filterCol};
        irData.outputCols = {outputCol};
        irData.opInfo = MatOp{};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& idxCol() { return data.inputColumns[0]; }
    BaseType::TableColumn& filterCol() { return data.inputColumns[1]; }
};

class MapView {
    IrData& data;
    MapOp& op;

public:
    using OpType = MapOp;

    explicit MapView(IrData& data) 
        : data(data), op(std::get<MapOp>(data.opInfo)) {}

    static IrData create(
        const BaseType::TableColumn& column,
        const ArithOp& operatorType,
        const std::variant<BaseType::TableColumn, uint64_t, float, std::string>& partnerVal,
        const BaseType::TableColumn& outputCol
    ) {
        IrData irData;
        irData.inputColumns = {column};
        if (std::holds_alternative<BaseType::TableColumn>(partnerVal)) {
            irData.inputColumns.push_back(std::get<BaseType::TableColumn>(partnerVal));
        }
        irData.outputCols = {outputCol};
        irData.opInfo = MapOp{operatorType, partnerVal};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& column() { return data.inputColumns[0]; }
    BaseType::TableColumn& outputCol() { return data.outputCols[0]; }
    ArithOp& operatorType() { return op.operatorType; }
    std::variant<BaseType::TableColumn, uint64_t, float, std::string>& partnerVal() { return op.partnerVal; }  
};


template<typename ViewT>
std::optional<ViewT> IrData::get_view_if() {
    // Generic implementation: checks if the variant holds the OpType defined by the View
    if (std::holds_alternative<typename ViewT::OpType>(opInfo)) {
        return ViewT(*this);
    }
    return std::nullopt;
}
