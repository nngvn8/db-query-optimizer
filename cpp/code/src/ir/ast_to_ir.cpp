#include "ir/ir_transformer.hpp"
#include "ir/transform_helpers.hpp"
#include "util/catalog.hpp"
#include "ir/ir_views.hpp"
#include <iostream>

std::shared_ptr<PlanNode> astToIr(ASTNode* ast) {
    if (!ast) return nullptr;

    auto node = std::make_shared<PlanNode>();

    // SELECT Node
    if (auto e = std::get_if<SelectClauseNode>(&ast->val)) {
        std::vector<BaseType::TableColumn> selectCols;
        for (const auto& desc : e->description) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            BaseType::TableColumn col(desc.table, desc.column, type, desc.alias);
            selectCols.push_back(col);
        }
        node->irData = SelectView::create(selectCols);
    }
    // Order By
    else if (auto e = std::get_if<OrderByClauseNode>(&ast->val)) {
        std::vector<BaseType::OrderDescription> orders;
        std::vector<BaseType::TableColumn> sortCols;

        for (const auto& desc : e->orderByList) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            bool isAsc = (desc.ordertype != "DESC");
            bool isNullsFirst = (desc.nullordering == "FIRST");

            BaseType::TableColumn sortCol(desc.table, desc.column, type);

            orders.emplace_back(
                sortCol,
                isAsc,
                isNullsFirst
            );

            sortCols.push_back(sortCol);
        }
        BaseType::TableColumn outCol = SortOrderView::generateOutCol(sortCols);
        node->irData = SortOrderView::create(orders, outCol);
    }
    // Group By
    else if (auto e = std::get_if<GroupByClauseNode>(&ast->val)) {
        
        // Transform to list of table columns
        std::vector<BaseType::TableColumn> groups;
        for (const auto& desc : e->description) {
            ColumnType type = Catalog::getSSBColumnType(desc.table, desc.column);
            groups.emplace_back(desc.table, desc.column, type);
        }

        // Generate grouping ir data
        BaseType::TableColumn outCol = GroupView::generateOutCol(groups);
        node->irData = GroupView::create(groups, outCol);
    }
    // Aggregation
    else if (auto e = std::get_if<AggregateClauseNode>(&ast->val)) {
        std::optional<AggFunc> aggFunc = IrTransformHelpers::mapStringToAggFunc(e->aggregateFunction);
        if (aggFunc.has_value()) {
            ColumnType inColType = Catalog::getSSBColumnType(e->table, e->column);
            ColumnType outColType;
            switch (aggFunc.value()) {
                case AGG_COUNT:
                    outColType = ColumnType::TYPE_INTEGER;
                    break;
                case AGG_AVG:
                    outColType = ColumnType::TYPE_FLOAT;
                    break;
                case AGG_MIN:
                case AGG_MAX:
                case AGG_SUM:
                    outColType = inColType;
                    break;
            }
            BaseType::TableColumn aggColIn(e->table, e->column, inColType, e->alias);
            std::string aggColOutName = e->aggregateFunction + "(" + e->column + ")";
            BaseType::TableColumn aggColOut(BaseType::Table("AGG"), aggColOutName, outColType, e->alias);
            node->irData = AggView::create(aggColIn, aggColOut, aggFunc.value());
        }
        else {
            std::cout << "Aggregation Function of AST tree could not be parsed!" << std::endl;
        }
    }
    // Map
    else if (auto e = std::get_if<Map>(&ast->val)) {
        ColumnType colTypeInput1 = Catalog::getSSBColumnType(e->table1, e->column1);
        ColumnType colTypeInput2 = Catalog::getSSBColumnType(e->table2, e->column2);
        ArithOp op = IrTransformHelpers::mapStringToArithOp(e->operatorType);

        ColumnType outColType;
        if (colTypeInput1 == ColumnType::TYPE_STRING || colTypeInput2 == ColumnType::TYPE_STRING
            || (op == ARITH_MOD && !(colTypeInput1 == ColumnType::TYPE_INTEGER && colTypeInput2 == ColumnType::TYPE_INTEGER))) {
            throw std::runtime_error("Invalid types for arithmetic operation");
        }
        if (colTypeInput1 == ColumnType::TYPE_FLOAT || colTypeInput2 == ColumnType::TYPE_FLOAT) {
            outColType = ColumnType::TYPE_FLOAT;
        }
        else {
            outColType = ColumnType::TYPE_INTEGER;
        }

        BaseType::TableColumn inputCol(e->table1, e->column1, colTypeInput1);
        BaseType::TableColumn partnerVal(e->table2, e->column2, colTypeInput2);
        BaseType::TableColumn outCol(BaseType::Table("MAP"), e->column1 + e->operatorType + e->column2, outColType);
        node->irData = MapView::create(inputCol, op, partnerVal, outCol);
    }
    // WHERE / Filter Node
    else if (auto e = std::get_if<WhereClauseNode>(&ast->val)) {
        // LOOKUP: Get type for the Input Column
        ColumnType colType = Catalog::getSSBColumnType(e->table, e->column);
        BaseType::TableColumn inputCol(e->table, e->column, colType);
        std::optional<BaseType::TableColumn> col2 = std::nullopt;

        std::vector<std::variant<uint64_t, float, std::string>> filterArgs;
        CompType opType = IrTransformHelpers::mapStringToCompType(e->operatorType);

        // TODO: Or capabilities limited by ast parsing: Always or of two equalities
        if (e->operatorType == "OR") {
            opType = CompType::COMP_IN;

            filterArgs.push_back(IrTransformHelpers::parseValueByType(e->value, colType));
            if (!e->value2.empty()) {
                filterArgs.push_back(IrTransformHelpers::parseValueByType(e->value2, colType));
            }
        }
        else if (e->operatorType == "BETWEEN") {
            opType = CompType::COMP_BETWEEN;

            filterArgs.push_back(IrTransformHelpers::parseValueByType(e->value, colType));
            filterArgs.push_back(IrTransformHelpers::parseValueByType(e->value2, colType));
        }
        // Column based filter (or join)
        else if (!e->column.empty() && !e->column2.empty()) {
            std::string table2 = !e->table2.empty() ? e->table2 : e->table;
            ColumnType col2Type = Catalog::getSSBColumnType(table2, e->column2);
            col2 = BaseType::TableColumn(table2, e->column2, col2Type);
        }
        // Single value filter
        else {
            filterArgs.push_back(IrTransformHelpers::parseValueByType(e->value, colType));
        }

        node->irData = FilterView::create(
            inputCol,
            opType,
            col2,
            filterArgs
        );

    }
    // JOIN Node
    else if (auto e = std::get_if<TableJoinNode>(&ast->val)) {
        ColumnType leftType = Catalog::getSSBColumnType(e->onLeftTable, e->onLeftTableColumn);

        // TODO: proper type inference as right value might not be column
        ColumnType rightType = Catalog::getSSBColumnType(e->onRightTable, e->onRightTableColumn);

        BaseType::TableColumn leftCol(e->onLeftTable, e->onLeftTableColumn, leftType);
        BaseType::TableColumn rightCol(e->onRightTable, e->onRightTableColumn, rightType);

        node->irData = JoinView::create(
            leftCol,
            rightCol,
            IrTransformHelpers::mapStringToJoinType(e->joinType),
            CompType::COMP_EQ
        );
    }
    // Set Ops
    else if (auto e = std::get_if<SetOperationNode>(&ast->val)) {
        BaseType::TableColumn dummy; // Still dummy as AST has no columns here
        BaseType::TableColumn outCol(
            BaseType::Table(""),
            e->setOperation,
            ColumnType::TYPE_POSLIST
        );
        node->irData = SetOpView::create(
            dummy,
            dummy,
            outCol,
            IrTransformHelpers::mapStringToRelOp(e->setOperation)
        );
    }
    // Base Table
    else if (auto e = std::get_if<TableBaseNode>(&ast->val)) {
        BaseType::Table table(e->tableName, e->tableAlias);
        node->irData = FetchView::create(BaseType::TableColumn(table, "", ColumnType::TYPE_INTEGER), true);
    }

    // Recursion
    if (ast->left) {
        node->children.push_back(astToIr(ast->left));
    }
    if (ast->right) {
        node->children.push_back(astToIr(ast->right));
    }

    // Set Ops: set input columns;
    if (node->irData.is<SetOp>()) {
        std::vector<BaseType::TableColumn>& inputColumns = node->irData.inputColumns;
        inputColumns[0] = node->children[0]->irData.outputCols[0];
        inputColumns[1] = node->children[1]->irData.outputCols[0];

        // Propagate the type from the input to the output column (SetOps preserve type)
        node->irData.outputCols[0].columnType = inputColumns[0].columnType;
        if (node->irData.outputCols[0].table.name.empty()) {
            node->irData.outputCols[0].table = inputColumns[0].table;
        }
    }

    return node;
}
