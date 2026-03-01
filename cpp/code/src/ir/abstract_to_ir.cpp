#include "ir/ir_transformer.hpp"
#include "ir/transform_helpers.hpp"
#include "WorkItem.pb.h"
#include "ir/ir_views.hpp"
#include "util/catalog.hpp"

#include <optional>
#include <string>
#include <tuple>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <variant>
#include <memory>
#include <vector>

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

            IrTransformHelpers::ParsedCondition conditionFields = IrTransformHelpers::ConditionParser::parseCondition(filterStr);
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
            IrTransformHelpers::parseJoinCondition(join->condition);

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
        if (IrTransformHelpers::containsArithMapOp(agg->agg_mapping)) {
            auto [aggIn1, op, aggIn2] = IrTransformHelpers::parseMapping(agg->agg_mapping);

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
                const std::string& firstColName = IrTransformHelpers::ConditionParser::getFirstTokenString(agg->agg_mapping);
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
                if (tableName == "Default" && IrTransformHelpers::isAggColumn(colName)) {
                    tableName = "AGG";
                }
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
        
        // Generate TableColumns for all columns to be sorted
        std::vector<BaseType::TableColumn> sortCols;
        for (size_t i = 0; i < sort->column_names.size(); ++i) {
            std::string colName = sort->column_names[i];
            ColumnType type = Catalog::getSSBColumnType("", colName);
            std::string tableName = Catalog::getTableName(colName);

            if (tableName == "Default" && IrTransformHelpers::isAggColumn(colName)) {
                tableName = "AGG";
            }

            std::optional<std::string> alias_opt = sort->aliases[i].empty() ? std::nullopt : std::make_optional(sort->aliases[i]);
            BaseType::TableColumn col(BaseType::Table(tableName), colName, type, alias_opt);

            orders.emplace_back(col, sort->asc[i], false);
            sortCols.push_back(col);
        }

        BaseType::TableColumn outCol = SortOrderView::generateOutCol(sortCols);
        node->irData = SortOrderView::create(orders, outCol);
    }

    // ==================== RESULT (SELECT) ====================
    const AbstractResult* result = std::get_if<AbstractResult>(&node->abstractData);
    if (result) {
        std::vector<BaseType::TableColumn> resultCols;
        for (const auto& colName : result->output_cols) {
            std::string tableName = Catalog::getTableName(colName);
            ColumnType type = Catalog::getSSBColumnType("", colName);
            const IrTransformHelpers::ColWithAlias& colAlias = IrTransformHelpers::ConditionParser::extractAlias(colName);

            if (tableName == "Default" && IrTransformHelpers::isAggColumn(colName)) {
                tableName = "AGG";
            }

            std::string colTrimName = IrTransformHelpers::removeAllWhitespace(colAlias.name);
            BaseType::TableColumn resultCol(BaseType::Table(tableName), colTrimName, type, colAlias.alias);
            resultCols.emplace_back(resultCol);
        }
        node->irData = SelectView::create(resultCols);
    }

    return node;
}
