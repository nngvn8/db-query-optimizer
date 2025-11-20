#include "item_builder.h"

#define DEBUG true

uint32_t currentPlanId = 0;
uint32_t currentItemId = 0;

// TODO: remove if other version works
void setTableColumnType(ColumnMessage* columnMessage, const std::string& tabName, const std::string& colName, int colType) {
    columnMessage->set_tabname(tabName);
    columnMessage->set_colname(colName);
    columnMessage->set_coltype(static_cast<ColumnType>(colType));
}

void setTableColumnType(ColumnMessage* columnMessage, const ItemBuilder::TableColumn* tableColumn) {
    columnMessage->set_tabname(tableColumn->tableName);
    columnMessage->set_colname(tableColumn->columnName);
    columnMessage->set_coltype(tableColumn->columnType);
}

// TODO only for debug, remove in prod
WorkItem ItemBuilder::createWorkItem() {
    return createWorkItem(currentPlanId, currentItemId++, static_cast<OperatorType>(1));
}

WorkItem ItemBuilder::createWorkItem(const OperatorType& operatorType) {
    return createWorkItem(currentPlanId, currentItemId++, operatorType);
}

WorkItem ItemBuilder::createWorkItem(const uint32_t& planId, const uint32_t& itemId, const OperatorType& operatorType) {
    WorkItem workItem;
    workItem.set_planid(planId);
    workItem.set_itemid(itemId);
    workItem.set_operatorid(operatorType);
    return workItem;
}

WorkItem ItemBuilder::createFetchItem(const FetchNode& node) {
    WorkItem workItem = createWorkItem();
    FetchItem* fetchItem = workItem.mutable_fetchdata();

    ColumnMessage* inputColumn = fetchItem->mutable_inputcolumn();
    setTableColumnType(inputColumn, node.inputColumn);

    fetchItem->set_printtofile(node.printToFile);

    return workItem;
}

WorkItem ItemBuilder::createFilterItem(const FilterNode& node) {
    WorkItem workItem = createWorkItem(OperatorType::OP_FILTER);
    FilterItem* filterItem = workItem.mutable_filterdata();

    ColumnMessage* inputColumn = filterItem->mutable_inputcolumn();
    ColumnMessage* outputColumn = filterItem->mutable_outputcolumn();

    setTableColumnType(inputColumn, node.inputColumn);
    setTableColumnType(outputColumn, node.outputColumn);

    filterItem->set_filtertype(node.filterType);

    for (auto filterScalarVal : node.filterArgVals) {
        ScalarValue* scalarVal = filterItem->add_filtervalue();

        if (std::holds_alternative<uint64_t>(filterScalarVal)) {
            IntValue* intVal = scalarVal->mutable_intval();
            intVal->set_value(std::get<uint64_t>(filterScalarVal));

        } else if (std::holds_alternative<float>(filterScalarVal)) {
            FloatValue* floatVal = scalarVal->mutable_floatval();
            floatVal->set_value(std::get<float>(filterScalarVal));

        } else if (std::holds_alternative<std::string>(filterScalarVal)) {
            StringValue* stringVal = scalarVal->mutable_stringval();
            stringVal->set_value(std::get<std::string>(filterScalarVal));
        }
    }
    return workItem;
}

WorkItem ItemBuilder::createJoinItem(const JoinNode& node) {
    WorkItem workItem = createWorkItem(OperatorType::OP_HASHJOIN); // TODO or MERGEJOIN
    JoinItem* joinItem = workItem.mutable_joindata();

    ColumnMessage* innerCol = joinItem->mutable_innercolumn();
    ColumnMessage* outerCol = joinItem->mutable_outercolumn();
    ColumnMessage* outputCol = joinItem->mutable_outputcolumn();

    setTableColumnType(innerCol, node.innerColumn);
    setTableColumnType(outerCol, node.outerColumn);
    setTableColumnType(outputCol, node.outerColumn);

    if (node.joinPredicate) {
        joinItem->set_joinpredicate(*(node.joinPredicate));
    } else {
        joinItem->set_joinpredicate(CompType::COMP_EQ); // TODO default okay?
    }

    return workItem;
}

WorkItem ItemBuilder::createMapItem(const MapNode& node) {
    WorkItem workItem = createWorkItem(OperatorType::OP_MAP);
    MapItem* mapItem = workItem.mutable_mapdata();

    ColumnMessage* inputCol = mapItem->mutable_inputcolumn();
    ColumnMessage* outputCol = mapItem->mutable_outputcolumn();

    setTableColumnType(inputCol, node.inputColumn);
    setTableColumnType(outputCol, node.outputColumn);

    if (node.operatorType) {
        mapItem->set_operatortype(*node.operatorType);
    }

    ScalarValue* scalarVal = mapItem->mutable_staticval();

    if (std::holds_alternative<TableColumn>(node.partnerVal)) {
        ColumnMessage* column = mapItem->mutable_partnercolumn();
        setTableColumnType(column, &std::get<TableColumn>(node.partnerVal));

    } else if (std::holds_alternative<uint64_t>(node.partnerVal)) {
        IntValue* intVal = scalarVal->mutable_intval();
        intVal->set_value(std::get<uint64_t>(node.partnerVal));

    } else if (std::holds_alternative<float>(node.partnerVal)) {
        FloatValue* floatVal = scalarVal->mutable_floatval();
        floatVal->set_value(std::get<float>(node.partnerVal));

    } else if (std::holds_alternative<std::string>(node.partnerVal)) {
        StringValue* stringVal = scalarVal->mutable_stringval();
        stringVal->set_value(std::get<std::string>(node.partnerVal));
    }

    return workItem;
}

WorkItem ItemBuilder::createMaterializeItem(const MaterializeNode& node) {
    WorkItem workItem = createWorkItem(OperatorType::OP_MATERIALIZE);
    MaterializeItem* matItem = workItem.mutable_materializedata();

    ColumnMessage* idxCol = matItem->mutable_indexcolumn();
    ColumnMessage* filterCol = matItem->mutable_filtercolumn();
    ColumnMessage* outputCol = matItem->mutable_outputcolumn();

    setTableColumnType(idxCol, node.idxColumn);
    setTableColumnType(filterCol, node.filterColumn);
    setTableColumnType(outputCol, node.outputColumn);

    return workItem;
}

WorkItem ItemBuilder::createMultiGroupItem(const MultiGroupNode& node) {
    WorkItem workItem = createWorkItem(OperatorType::OP_GROUPBY); // TODO correct?
    MultiGroupItem* multiGrpItem = workItem.mutable_multigroupdata();

    uint idx = 0;
    for (ItemBuilder::TableColumn* column : node.groupColumns) {
        multiGrpItem->add_groupcolumns();
        ColumnMessage* colMessage = multiGrpItem->mutable_groupcolumns(idx);
        setTableColumnType(colMessage, column);
        idx++;
    }

    ColumnMessage* outputIdx = multiGrpItem->mutable_outputindex();
    ColumnMessage* outputClusters = multiGrpItem->mutable_outputclusters();
    ColumnMessage* aggCol = multiGrpItem->mutable_aggregationcolumn();
    ColumnMessage* aggResultCol = multiGrpItem->mutable_aggregationresultcolumn();

    setTableColumnType(outputIdx, node.outputIdx);
    setTableColumnType(outputClusters, node.outputCluster);
    setTableColumnType(aggCol, node.aggColumn);
    setTableColumnType(aggResultCol, node.aggResultColumn);

    multiGrpItem->set_storeextends(node.storeExtends);

    for (const bool& order : node.sortOrders) {
        multiGrpItem->add_sortorders(order);
    }

    return workItem;
}

WorkItem ItemBuilder::createResultItem(const ResultNode& node) {
    WorkItem workItem = createWorkItem();
    ResultItem* resultItem = workItem.mutable_resultdata();

    uint idx = 0;
    for (ItemBuilder::TableColumn* column : node.resultColumns) {
        resultItem->add_resultcolumns();
        ColumnMessage* colMessage = resultItem->mutable_resultcolumns(idx);
        setTableColumnType(colMessage, column);
        idx++;
    }

    resultItem->set_filename(node.filename);

    for (const std::string& header : node.resultHeaders) {
        resultItem->add_resultheader(header);
    }

    return workItem;
}

WorkItem ItemBuilder::createSetOperationItem(const SetOperationNode& node) {
    WorkItem workItem = createWorkItem(OperatorType::OP_SETOPERATION);
    SetOperationItem* setOpItem = workItem.mutable_setdata();

    setOpItem->set_operation(node.operation); // TODO is optional?

    ColumnMessage* innerCol = setOpItem->mutable_innercolumn();
    ColumnMessage* outerCol = setOpItem->mutable_outercolumn();
    ColumnMessage* outputCol = setOpItem->mutable_outputcolumn();

    setTableColumnType(innerCol, node.innerColumn);
    setTableColumnType(outerCol, node.outerColumn);
    setTableColumnType(outputCol, node.outputColumn);

    return workItem;
}

WorkItem ItemBuilder::createSortItem(const SortNode& node) {
    WorkItem workItem = createWorkItem(OperatorType::OP_SORT);
    SortItem* sortItem = workItem.mutable_sortdata();

    uint idx = 0;
    for (ItemBuilder::TableColumn* column : node.inputColumns) {
        sortItem->add_inputcolumns();
        ColumnMessage* colMessage = sortItem->mutable_inputcolumns(idx);
        setTableColumnType(colMessage, column);
        idx++;
    }

    ColumnMessage* idxOutput = sortItem->mutable_indexoutput();
    ColumnMessage* existingIdx = sortItem->mutable_existingindex();

    setTableColumnType(idxOutput, node.idxOutput);
    setTableColumnType(existingIdx, node.existingIdx);

    idx = 0;
    for (const bool& order : node.sortOrders) {
        sortItem->set_sortorder(idx, order);
        idx++;
    }

    return workItem;
}

WorkItem ItemBuilder::createAggItem(const AggNode& node) {
    WorkItem workItem = createWorkItem(OperatorType::OP_AGGREGATE);
    AggItem* aggItem = workItem.mutable_aggdata();

    ColumnMessage* inputCol = aggItem->mutable_inputcolumn();
    ColumnMessage* outputCol = aggItem->mutable_outputcolumn();

    setTableColumnType(inputCol, node.inputColumn);
    setTableColumnType(outputCol, node.outputColumn);

    aggItem->set_aggfunc(node.aggFunc);

    for (const std::string& name : node.groupColumns) {
        aggItem->add_groupcolumns(name);
    }

    return workItem;
}
