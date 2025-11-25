#include "item_builder.h"

// TODO: remove if other version works
void setTableColumnType(ColumnMessage* columnMessage, const std::string& tabName, const std::string& colName, int colType) {
    columnMessage->set_tabname(tabName);
    columnMessage->set_colname(colName);
    columnMessage->set_coltype(static_cast<ColumnType>(colType));
}

void setTableColumnType(ColumnMessage* columnMessage, ItemBuilder::TableColumn& tableColumn) {
    columnMessage->set_tabname(tableColumn.table);
    columnMessage->set_colname(tableColumn.column);
    columnMessage->set_coltype(static_cast<ColumnType>(tableColumn.columnType));
}

WorkItem ItemBuilder::createWorkItem() {
    WorkItem workItem;
    workItem.set_planid(1);
    workItem.set_itemid(1);
    workItem.set_operatorid(static_cast<OperatorType>(1));
    return workItem;
}

FetchItem* ItemBuilder::createFetchItem(ASTNode* node) {
    return nullptr;
}

FilterItem* ItemBuilder::createFilterItem(FilterNode* node) {
    if (node == nullptr)
        return nullptr;

    WorkItem workItem = createWorkItem();

    FilterItem* filterItem = workItem.mutable_filterdata();
    ColumnMessage* inputColumn = filterItem->mutable_inputcolumn();
    ColumnMessage* outputColumn = filterItem->mutable_outputcolumn();

    setTableColumnType(inputColumn, node->tableColumn);
    setTableColumnType(outputColumn, "tabl1", "column1", 1);

    filterItem->set_filtertype(static_cast<CompType>(node->filterType));
    auto filterValue = filterItem->add_filtervalue();

    switch (node->filterType) {
        case 0: {
            IntValue* value = filterValue->mutable_intval();
            value->set_value(1);
        } break;
        case 1: {
            FloatValue* value = filterValue->mutable_floatval();
            value->set_value(13.37);
        } break;
        case 2: {
            StringValue* value = filterValue->mutable_stringval();
            value->set_value("str");
        } break;
        case 3: {
            // ScalarValue* value = filterValue;
        } break;
        default:
            break;
    }
    return filterItem;
}

JoinItem* ItemBuilder::createJoinItem(JoinNode* node) {
    WorkItem workItem = createWorkItem();
    JoinItem* joinItem = workItem.mutable_joindata();

    ColumnMessage* innerCol = joinItem->mutable_innercolumn();
    ColumnMessage* outerCol = joinItem->mutable_outercolumn();
    ColumnMessage* outputCol = joinItem->mutable_outputcolumn();

    setTableColumnType(innerCol, node->innerTableColumn);
    setTableColumnType(outerCol, node->outerTableColumn);
    setTableColumnType(outputCol, "tabl3", "column3", 3);

    joinItem->set_joinpredicate(static_cast<CompType>(1));
    return joinItem;
}

MapItem* ItemBuilder::createMapItem(ASTNode* node) {
    return nullptr;
}

MaterializeItem* ItemBuilder::createMaterializeItem(ASTNode* node) {
    return nullptr;
}

MultiGroupItem* ItemBuilder::createMultiGroupItem(ASTNode* node) {
    return nullptr;
}

ResultItem* ItemBuilder::createResultItem(ASTNode* node) {
    return nullptr;
}

SetOperationItem* ItemBuilder::createSetOperationItem(ASTNode* node) {
    return nullptr;
}

SortItem* ItemBuilder::createSortItem(ASTNode* node) {
    return nullptr;
}

AggItem* ItemBuilder::createAggItem(ASTNode* node) {
    return nullptr;
}
