/**
 * @file ir_views.hpp
 * @brief Defines view classes for accessing and manipulating different IR operator types.
 *
 * This file provides a set of classes that act as "views" into an `IrData` object.
 * Each view corresponds to a specific operator type (e.g., Join, Filter, Group)
 * and provides a type-safe and convenient API for accessing and modifying the
 * operator's parameters. This avoids manual and error-prone `std::get` calls on
 * the `IrData::opInfo` variant.
 *
 * Each view class typically provides:
 * - A constructor that takes a reference to an `IrData` object.
 * - A static `create` factory function to construct a new `IrData` object for that operator type.
 * - Accessor methods for the operator's specific fields.
 */

#pragma once

#include "ir/ir_types.hpp"
#include "ir/plan_node.hpp"

/**
 * @class JoinView
 * @brief A view for the Join operator.
 */
class JoinView {
    IrData& data;
    JoinOp& op;

public:
    using OpType = JoinOp;

    /**
     * @brief Constructs a JoinView from an IrData object.
     * @param data The IrData object, which must contain a JoinOp.
     */
    explicit JoinView(IrData& data)
        : data(data), op(std::get<JoinOp>(data.opInfo)) {}

    /**
     * @brief Creates an IrData object for a Join operation.
     * @param inner The inner join column.
     * @param outer The outer join column.
     * @param joinType The type of join.
     * @param joinPredicate The join predicate.
     * @return An IrData object representing the Join operation.
     */
    static IrData create(const BaseType::TableColumn& inner,
                         const BaseType::TableColumn& outer,
                         const BaseType::Join& joinType,
                         const CompType& joinPredicate) {
        IrData irData;
        irData.outputsPosList = true;
        irData.outputsMatVals = false;
        irData.inputColumns = {inner, outer};

        BaseType::TableColumn o_inner = inner;
        BaseType::TableColumn o_outer = outer;
        o_inner.columnType = ColumnType::TYPE_POSLIST;
        o_outer.columnType = ColumnType::TYPE_POSLIST;

        irData.outputCols = {o_inner, o_outer};
        irData.opInfo = JoinOp{joinType, joinPredicate};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& inner() { return data.inputColumns[0]; }
    BaseType::TableColumn& outer() { return data.inputColumns[1]; }
    BaseType::TableColumn& innerOut() { return data.outputCols[0]; }
    BaseType::TableColumn& outerOut() { return data.outputCols[1]; }
    BaseType::Join& joinType() { return op.joinType; }
    CompType& joinPredicate() { return op.joinPredicate; }
};

/**
 * @class SemiJoinView
 * @brief A view for the SemiJoin operator.
 */
class SemiJoinView {
    IrData& data;
    SemiJoinOp& op;

public:
    using OpType = SemiJoinOp;

    explicit SemiJoinView(IrData& data)
        : data(data), op(std::get<SemiJoinOp>(data.opInfo)) {}

    /**
     * @brief Creates an IrData object for a SemiJoin operation.
     */
    static IrData create(const BaseType::TableColumn& inner,
                         const BaseType::TableColumn& outer,
                         const BaseType::Join& joinType,
                         const CompType& joinPredicate) {
        IrData irData;
        irData.outputsPosList = true;
        irData.outputsMatVals = false;
        irData.inputColumns = {inner, outer};

        BaseType::TableColumn o_inner = inner;
        BaseType::TableColumn o_outer = outer;
        o_inner.columnType = ColumnType::TYPE_POSLIST;
        o_outer.columnType = ColumnType::TYPE_POSLIST;

        irData.outputCols = {o_inner, o_outer};
        irData.opInfo = SemiJoinOp{joinType, joinPredicate};
        return irData;
    }

    /**
     * @brief Creates an IrData object for a SemiJoin operation based on an existing Join operation.
     */
    static IrData create(IrData& data) {
        IrData irData;
        irData.outputsPosList = true;
        irData.outputsMatVals = false;
        irData.inputColumns = data.inputColumns;
        irData.outputCols = data.outputCols;

        BaseType::TableColumn o_inner = irData.outputCols[0];
        BaseType::TableColumn o_outer = irData.outputCols[1];
        o_inner.columnType = ColumnType::TYPE_POSLIST;
        o_outer.columnType = ColumnType::TYPE_POSLIST;
        irData.outputCols = {o_inner, o_outer};

        JoinOp op = std::get<JoinOp>(data.opInfo);
        irData.opInfo = SemiJoinOp{op.joinType, op.joinPredicate};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& inner() { return data.inputColumns[0]; }
    BaseType::TableColumn& outer() { return data.inputColumns[1]; }
    BaseType::TableColumn& innerOut() { return data.outputCols[0]; }
    BaseType::TableColumn& outerOut() { return data.outputCols[1]; }
    BaseType::Join& joinType() { return op.joinType; }
    CompType& joinPredicate() { return op.joinPredicate; }
};

/**
 * @class SelectView
 * @brief A view for the Select operator.
 */
class SelectView {
    IrData& data;
    SelectOp& op;

public:
    using OpType = SelectOp;

    explicit SelectView(IrData& data)
        : data(data), op(std::get<SelectOp>(data.opInfo)) {}

    /**
     * @brief Creates an IrData object for a Select operation.
     */
    static IrData create(const std::vector<BaseType::TableColumn>& resultCols,
                         std::string fileName = "",
                         std::optional<BaseType::TableColumn> resultIdx = std::nullopt,
                         std::vector<std::string> resultHeaders = {}) {
        IrData irData;
        irData.outputsPosList = false;
        irData.outputsMatVals = true;
        irData.inputColumns = resultCols;
        irData.outputCols = resultCols;

        if (resultHeaders.empty()) {
            for (const auto& resCol : resultCols) {
                std::string header = resCol.alias.value_or(resCol.columnName);
                resultHeaders.push_back(header);
            }
        }
        irData.opInfo = SelectOp{resultIdx, resultHeaders};
        return irData;
    }

    // Accessors
    std::vector<BaseType::TableColumn>& resultCols() { return data.inputColumns; }
    std::vector<std::string>& resultHeaders() { return op.resultHeaders; }
    std::optional<BaseType::TableColumn>& resultIdx() { return op.resultIdx; }
};

/**
 * @class AggView
 * @brief A view for the Aggregation operator.
 */
class AggView {
    IrData& data;
    AggOp& op;

public:
    using OpType = AggOp;

    explicit AggView(IrData& data)
        : data(data), op(std::get<AggOp>(data.opInfo)) {}

    /**
     * @brief Creates an IrData object for an Aggregation operation.
     */
    static IrData create(const BaseType::TableColumn& inputCol,
                         const BaseType::TableColumn& outputCol,
                         AggFunc aggFunc) {
        IrData irData;
        irData.outputsPosList = false;
        irData.outputsMatVals = true;
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

/**
 * @class FetchView
 * @brief A view for the Fetch operator.
 */
class FetchView {
    IrData& data;
    FetchOp& op;

public:
    using OpType = FetchOp;

    explicit FetchView(IrData& data)
        : data(data), op(std::get<FetchOp>(data.opInfo)) {}

    /**
     * @brief Creates an IrData object for a Fetch operation.
     */
    static IrData create(const BaseType::TableColumn& inputCol, bool wasTableBaseNode = false) {
        IrData irData;
        irData.outputsPosList = false;
        irData.outputsMatVals = true;
        irData.inputColumns = {inputCol};
        irData.outputCols = {inputCol};
        irData.opInfo = FetchOp{wasTableBaseNode};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& inputCol() { return data.inputColumns[0]; }
    BaseType::TableColumn& outputCol() { return data.outputCols[0]; }
    bool& wasTableBaseNode() { return op.wasTableBaseNode; }


};

/**
 * @class FilterView
 * @brief A view for the Filter operator.
 */
class FilterView {
    IrData& data;
    FilterOp& op;

public:
    using OpType = FilterOp;

    explicit FilterView(IrData& data)
        : data(data), op(std::get<FilterOp>(data.opInfo)) {}

    /**
     * @brief Creates an IrData object for a Filter operation.
     */
    static IrData create(
        const BaseType::TableColumn& col1,
        const CompType& filterType,
        const std::optional<BaseType::TableColumn>& col2,
        const std::vector<std::variant<uint64_t, float, std::string>>& filterArgs,
        const ColumnType& outputType = ColumnType::TYPE_POSLIST
    ) {
        IrData irData;
        irData.outputsPosList = true;
        irData.outputsMatVals = false;
        irData.inputColumns = {col1};
        // TODO: should this be pushed as an input column??
        if (col2.has_value()) {
            irData.inputColumns.push_back(col2.value());
        }
        BaseType::TableColumn outputCol = col1;
        outputCol.columnType = outputType;
        irData.outputCols = {outputCol};
        irData.opInfo = FilterOp{filterType, filterArgs};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& col1() { return data.inputColumns[0]; }
    BaseType::TableColumn* col2() { return data.inputColumns.size() > 1 ? &data.inputColumns[1] : nullptr; }
    BaseType::TableColumn& outputCol() { return data.outputCols[0]; }
    CompType& filterType() { return op.filterType; }
    std::vector<std::variant<uint64_t, float, std::string>>& filterArgs() { return op.filterArgs; }

};

/**
 * @class GroupView
 * @brief A view for the Group operator.
 */
class GroupView {
    IrData& data;
    GroupOp& op;

public:
    using OpType = GroupOp;

    explicit GroupView(IrData& data)
        : data(data), op(std::get<GroupOp>(data.opInfo)) {}


    /**
     * @brief Creates an IrData object for a Group operation without aggregation.
     */
    static IrData create(const std::vector<BaseType::TableColumn>& groupingCols, const BaseType::TableColumn& outputCol) {
        IrData irData;
        irData.outputsPosList = true;
        irData.outputsMatVals = true;
        irData.inputColumns = groupingCols;
        BaseType::TableColumn outSrtIdx = outputCol;
        BaseType::TableColumn outCluster = outputCol;
        outSrtIdx.columnName += "_srt";
        outCluster.columnName += "_clus";
        irData.outputCols = {outputCol, outSrtIdx, outCluster};
        std::vector<bool> sortOrders(groupingCols.size(), true);
        irData.opInfo = GroupOp{sortOrders, false};
        return irData;
    }

    /**
     * @brief Creates an IrData object for a Group operation with aggregation.
     */
    static IrData create(const std::vector<BaseType::TableColumn>& groupingCols, const BaseType::TableColumn& outputCol, const BaseType::TableColumn& aggCol, const BaseType::TableColumn& aggResultCol) {
        IrData irData;
        irData.outputsPosList = true;
        irData.outputsMatVals = true;
        irData.inputColumns = groupingCols;
        irData.inputColumns.push_back(aggCol);
        BaseType::TableColumn outSrtIdx = outputCol;
        BaseType::TableColumn outCluster = outputCol;
        outSrtIdx.columnName += "_srt";
        outCluster.columnName += "_clus";
        irData.outputCols = {outputCol, outSrtIdx, outCluster, aggResultCol};
        std::vector<bool> sortOrders(groupingCols.size(), true);
        irData.opInfo = GroupOp{sortOrders, true};
        return irData;
    }

    // Accessors
    std::vector<BaseType::TableColumn>& groupingCols() { return data.inputColumns; }
    BaseType::TableColumn& outputIdx() { return data.outputCols[0]; }
    BaseType::TableColumn& outputSortIdx() { return data.outputCols[1]; }
    BaseType::TableColumn& outputCluster() { return data.outputCols[2]; }
    std::vector<bool>& sortOrders() { return op.sortOrders; }
    bool& storeExtends() { return op.storeExtends; }
    /**
     * @brief Gets the aggregation column, if it exists.
     * @return A pointer to the aggregation column, or nullptr if there is no aggregation.
     */
    BaseType::TableColumn* aggCol() {
        if (op.hasAgg && !data.inputColumns.empty()) return &data.inputColumns.back();
        return nullptr;
    }
    /**
     * @brief Gets the aggregation result column, if it exists.
     * @return A pointer to the aggregation result column, or nullptr if there is no aggregation.
     */
    BaseType::TableColumn* aggResultCol() {
        if (op.hasAgg && !data.outputCols.empty()) return &data.outputCols.back();
        return nullptr;
    }
    /**
     * @brief Sets or updates the aggregation for this Group operation.
     * @param aggCol The column to aggregate.
     * @param aggResultCol The column to store the aggregation result.
     */
    void setAgg(const BaseType::TableColumn& aggCol, const BaseType::TableColumn& aggResultCol) {
        if (op.hasAgg) {
            data.inputColumns.back() = aggCol;
            data.outputCols.back() = aggResultCol;
        } else {
            op.hasAgg = true;
            data.inputColumns.push_back(aggCol);
            data.outputCols.push_back(aggResultCol);
        }
    }
    /**
     * @brief Removes the aggregation from this Group operation.
     */
    void removeAgg() {
        if (op.hasAgg) {
            op.hasAgg = false;
            data.inputColumns.pop_back();
            data.outputCols.pop_back();
        }
    }

    /**
     * @brief Generates a string for the output column based on the grouping columns.
     */
    static std::string generateOutColString(const std::vector<BaseType::TableColumn>& groupingCols) {
        std::vector<BaseType::TableColumn> groups;
        std::stringstream ss;
        int i = 0;
        for (const auto& gCol : groupingCols) {
            // Extend grouping string
            char tablePrefix = gCol.table.name.empty() ? '?' : gCol.table.name[0];
            ss << tablePrefix << "." << gCol.columnName;
            if (i < groupingCols.size() - 1) ss << "_";
            i++;
        }
        std::string groupingColString = ss.str();
        return groupingColString;
    }

    /**
     * @brief Generates an output column for the group operation.
     */
    static BaseType::TableColumn generateOutCol(const std::vector<BaseType::TableColumn>& groupingCols) {
        BaseType::TableColumn outCol(BaseType::Table("GROUP"), generateOutColString(groupingCols), ColumnType::TYPE_POSLIST);
        return outCol;
    }
};

/**
 * @class SortOrderView
 * @brief A view for the Sort operator.
 */
class SortOrderView {
    IrData& data;
    SortOp& op;

public:
    using OpType = SortOp;

    explicit SortOrderView(IrData& data)
        : data(data), op(std::get<SortOp>(data.opInfo)) {}

    /**
     * @brief Creates an IrData object for a Sort operation.
     */
    static IrData create(const std::vector<BaseType::OrderDescription>& orderDescriptions, const BaseType::TableColumn& idxOutput, std::optional<BaseType::TableColumn> existingIdx = std::nullopt) {
        IrData irData;
        irData.outputsPosList = true;
        irData.outputsMatVals = false;
        irData.inputColumns.reserve(orderDescriptions.size() + (existingIdx.has_value() ? 1 : 0));

        for (const auto& desc : orderDescriptions) {
            irData.inputColumns.push_back(desc.column);
        }

        if (existingIdx.has_value()) {
            irData.inputColumns.push_back(existingIdx.value());
        }

        irData.outputCols = {idxOutput};
        irData.opInfo = SortOp{orderDescriptions, existingIdx};
        return irData;
    }

    // Accessors
    std::vector<BaseType::OrderDescription>& orderDescriptions() { return op.columnList; }
    std::vector<BaseType::TableColumn>& sortCols() { return data.inputColumns; }
    BaseType::TableColumn& idxOutput() { return data.outputCols[0]; }
    std::optional<BaseType::TableColumn>& existingIdx() { return op.existingIdx; }

    /**
     * @brief Generates a string for the output column based on the sort columns.
     */
    static std::string generateOutColString(const std::vector<BaseType::TableColumn>& sortCols) {
        std::stringstream ss;
        int i = 0;
        for (const auto& col : sortCols) {
            char tablePrefix = col.table.name.empty() ? '?' : col.table.name[0];
            ss << tablePrefix << "." << col.columnName;
            if (i < sortCols.size() - 1) ss << "_";
            i++;
        }
        return ss.str();
    }

    /**
     * @brief Generates an output column for the sort operation.
     */
    static BaseType::TableColumn generateOutCol(const std::vector<BaseType::TableColumn>& sortCols) {
        BaseType::TableColumn outCol(BaseType::Table("SORT"), generateOutColString(sortCols), ColumnType::TYPE_POSLIST);
        return outCol;
    }
};

// class Limit - omitted !! (not supported by system)

/**
 * @class SetOpView
 * @brief A view for Set operations (e.g., UNION, INTERSECT, EXCEPT).
 */
class SetOpView {
    IrData& data;
    SetOp& op;

public:
    using OpType = SetOp;

    explicit SetOpView(IrData& data)
        : data(data), op(std::get<SetOp>(data.opInfo)) {}

    /**
     * @brief Creates an IrData object for a Set operation.
     */
    static IrData create(
        const BaseType::TableColumn& inner,
        const BaseType::TableColumn& outer,
        const BaseType::TableColumn& output,
        const RelOp& operation
    ) {
        IrData irData;
        irData.outputsPosList = true;
        irData.outputsMatVals = false;
        irData.inputColumns = {inner, outer};
        irData.outputCols = {output};
        irData.opInfo = SetOp{operation};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& innerCol() { return data.inputColumns[0]; }
    BaseType::TableColumn& outerCol() { return data.inputColumns[1]; }
    BaseType::TableColumn& outputCol() { return data.outputCols[0]; }
    RelOp& operation() { return op.operation; }

};

/**
 * @class MaterializeView
 * @brief A view for the Materialize operator.
 */
class MaterializeView {
    IrData& data;
    MatOp& op;

public:
    using OpType = MatOp;

    explicit MaterializeView(IrData& data)
        : data(data), op(std::get<MatOp>(data.opInfo)) {}

    /**
     * @brief Creates an IrData object for a Materialize operation.
     */
    static IrData create(const BaseType::TableColumn& idxCol, const BaseType::TableColumn& filterCol, const BaseType::TableColumn& outputCol, bool outputsPosList = true) {
        IrData irData;
        irData.outputsPosList = outputsPosList;
        irData.outputsMatVals = !outputsPosList;
        irData.inputColumns = {idxCol, filterCol};
        BaseType::TableColumn out = outputCol;
        if (outputsPosList) {
            out.columnType = ColumnType::TYPE_POSLIST;
        }
        irData.outputCols = {out};
        irData.opInfo = MatOp{};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& idxCol() { return data.inputColumns[0]; }
    BaseType::TableColumn& filterCol() { return data.inputColumns[1]; }
    BaseType::TableColumn& outputCol() { return data.outputCols[0]; }

};

/**
 * @class MapView
 * @brief A view for the Map operator (arithmetic operations).
 */
class MapView {
    IrData& data;
    MapOp& op;

public:
    using OpType = MapOp;

    explicit MapView(IrData& data)
        : data(data), op(std::get<MapOp>(data.opInfo)) {}

    /**
     * @brief Creates an IrData object for a Map operation.
     */
    static IrData create(
        const BaseType::TableColumn& column,
        const ArithOp& operatorType,
        const std::variant<BaseType::TableColumn, uint64_t, float, std::string>& partnerVal,
        const BaseType::TableColumn& outputCol
    ) {
        IrData irData;
        irData.outputsPosList = false;
        irData.outputsMatVals = true;
        irData.inputColumns = {column};
        if (std::holds_alternative<BaseType::TableColumn>(partnerVal)) {
            irData.inputColumns.push_back(std::get<BaseType::TableColumn>(partnerVal));
            irData.opInfo = MapOp{operatorType, std::monostate{}};
        } 
        else if (std::holds_alternative<uint64_t>(partnerVal)) {
            irData.opInfo = MapOp{operatorType, std::get<uint64_t>(partnerVal)};
        } 
        else if (std::holds_alternative<float>(partnerVal)) {
            irData.opInfo = MapOp{operatorType, std::get<float>(partnerVal)};
        } 
        else {
            irData.opInfo = MapOp{operatorType, std::get<std::string>(partnerVal)};
        }
        irData.outputCols = {outputCol};
        return irData;
    }

    // Accessors
    BaseType::TableColumn& inputCol() { return data.inputColumns[0]; }
    BaseType::TableColumn& outputCol() { return data.outputCols[0]; }
    ArithOp& operatorType() { return op.operatorType; }
    
    // Getter
    std::variant<BaseType::TableColumn, uint64_t, float, std::string> partnerVal() const {
        if (std::holds_alternative<std::monostate>(op.partnerVal)) {
            return data.inputColumns.at(1);
        }
        if (std::holds_alternative<uint64_t>(op.partnerVal)) {
            return std::get<uint64_t>(op.partnerVal);
        }
        if (std::holds_alternative<float>(op.partnerVal)) {
            return std::get<float>(op.partnerVal);
        }
        return std::get<std::string>(op.partnerVal);
    }

    // Setter
    void partnerVal(const std::variant<BaseType::TableColumn, uint64_t, float, std::string>& val) {
        if (std::holds_alternative<BaseType::TableColumn>(val)) {
            if (data.inputColumns.size() > 1) {
                data.inputColumns[1] = std::get<BaseType::TableColumn>(val);
            } else {
                data.inputColumns.push_back(std::get<BaseType::TableColumn>(val));
            }
            op.partnerVal = std::monostate{};
        } else {
            if (data.inputColumns.size() > 1) {
                data.inputColumns.erase(data.inputColumns.begin() + 1);
            }
            if (std::holds_alternative<uint64_t>(val)) {
                op.partnerVal = std::get<uint64_t>(val);
            } else if (std::holds_alternative<float>(val)) {
                op.partnerVal = std::get<float>(val);
            } else if (std::holds_alternative<std::string>(val)) {
                op.partnerVal = std::get<std::string>(val);
            }
        }
    }
};
