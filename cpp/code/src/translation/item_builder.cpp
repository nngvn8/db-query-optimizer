#include "item_builder.h"
#include <ir/plan_node.hpp>
#define DEBUG true

uint32_t currentPlanId = 0;
uint32_t currentItemId = 0;

// TODO: remove if other version works
void setTableColumnType(ColumnMessage* columnMessage, const std::string& tabName, const std::string& colName, int colType) {
    columnMessage->set_tabname(tabName);
    columnMessage->set_colname(colName);
    columnMessage->set_coltype(static_cast<ColumnType>(colType));
}

void setTableColumnType(ColumnMessage* columnMessage, const BaseType::TableColumn* tableColumn) {
    columnMessage->set_tabname(tableColumn->table.name);
    columnMessage->set_colname(tableColumn->columnName);
    columnMessage->set_coltype(tableColumn->columnType);
}

// WORK ITEM

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

// FETCH ITEM

WorkItem ItemBuilder::createFetchItem(const FetchNode& node) {
    return createFetchItem(node.inputColumn, node.printToFile);
}

WorkItem ItemBuilder::createFetchItem(const BaseType::TableColumn* inputColumn, const bool printToFile) {
    WorkItem workItem = createWorkItem();
    FetchItem* fetchItem = workItem.mutable_fetchdata();

    ColumnMessage* inputColumnMsg = fetchItem->mutable_inputcolumn();
    setTableColumnType(inputColumnMsg, inputColumn);

    fetchItem->set_printtofile(printToFile);
    return workItem;
}

// FILTER ITEM

WorkItem ItemBuilder::createFilterItem(const FilterNode& node) {
    return createFilterItem(node.inputColumn, node.outputColumn, node.filterType, node.filterArgVals);
}

WorkItem ItemBuilder::createFilterItem(const BaseType::TableColumn* inColumn, const BaseType::TableColumn* outColumn,
    const CompType& filterType, const std::vector<std::variant<uint64_t, float, std::string>>& filterArgVals)
{
    WorkItem workItem = createWorkItem(OperatorType::OP_FILTER);
    FilterItem* filterItem = workItem.mutable_filterdata();

    ColumnMessage* inputColumn = filterItem->mutable_inputcolumn();
    ColumnMessage* outputColumn = filterItem->mutable_outputcolumn();

    setTableColumnType(inputColumn, inColumn);
    setTableColumnType(outputColumn, outColumn);

    filterItem->set_filtertype(filterType);

    for (auto filterScalarVal : filterArgVals) {
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

// JOIN ITEM

WorkItem ItemBuilder::createJoinItem(const JoinNode& node) {
    return createJoinItem(node.innerColumn, node.outerColumn, node.outputColumn, node.joinPredicate);
}

WorkItem ItemBuilder::createJoinItem(const BaseType::TableColumn* innerColumn, const BaseType::TableColumn* outerColumn,
    const BaseType::TableColumn* outColumn, const CompType* predicate)
{
    WorkItem workItem = createWorkItem(OperatorType::OP_HASHJOIN); // TODO or MERGEJOIN
    JoinItem* joinItem = workItem.mutable_joindata();

    ColumnMessage* innerCol = joinItem->mutable_innercolumn();
    ColumnMessage* outerCol = joinItem->mutable_outercolumn();
    ColumnMessage* outputCol = joinItem->mutable_outputcolumn();

    setTableColumnType(innerCol, innerColumn);
    setTableColumnType(outerCol, outerColumn);
    setTableColumnType(outputCol, outColumn);

    if (predicate) {
        joinItem->set_joinpredicate(*predicate);
    } else {
        joinItem->set_joinpredicate(CompType::COMP_EQ);
    }
    return workItem;
}

// MAP ITEM

WorkItem ItemBuilder::createMapItem(const MapNode& node) {
    return createMapItem(node.inputColumn, node.outputColumn, node.operatorType, node.partnerVal);
}

WorkItem ItemBuilder::createMapItem(const BaseType::TableColumn* inColumn, const BaseType::TableColumn* outColumn, const ArithOp* operatorType,
    const std::variant<BaseType::TableColumn, uint64_t, float, std::string>& partnerVal)
{
    WorkItem workItem = createWorkItem(OperatorType::OP_MAP);
    MapItem* mapItem = workItem.mutable_mapdata();

    ColumnMessage* inputCol = mapItem->mutable_inputcolumn();
    ColumnMessage* outputCol = mapItem->mutable_outputcolumn();

    setTableColumnType(inputCol, inColumn);
    setTableColumnType(outputCol, outColumn);

    if (operatorType) {
        mapItem->set_operatortype(*operatorType);
    }

    ScalarValue* scalarVal = mapItem->mutable_staticval();

    if (std::holds_alternative<BaseType::TableColumn>(partnerVal)) {
        ColumnMessage* column = mapItem->mutable_partnercolumn();
        setTableColumnType(column, &std::get<BaseType::TableColumn>(partnerVal));

    } else if (std::holds_alternative<uint64_t>(partnerVal)) {
        IntValue* intVal = scalarVal->mutable_intval();
        intVal->set_value(std::get<uint64_t>(partnerVal));

    } else if (std::holds_alternative<float>(partnerVal)) {
        FloatValue* floatVal = scalarVal->mutable_floatval();
        floatVal->set_value(std::get<float>(partnerVal));

    } else if (std::holds_alternative<std::string>(partnerVal)) {
        StringValue* stringVal = scalarVal->mutable_stringval();
        stringVal->set_value(std::get<std::string>(partnerVal));
    }
    return workItem;
}

// MATERIALIZE ITEM

WorkItem ItemBuilder::createMaterializeItem(const MaterializeNode& node) {
    return createMaterializeItem(node.idxColumn, node.filterColumn, node.outputColumn);
}

WorkItem ItemBuilder::createMaterializeItem(const BaseType::TableColumn* idxColumn, const BaseType::TableColumn* filterColumn,
    const BaseType::TableColumn* outColumn)
{
    WorkItem workItem = createWorkItem(OperatorType::OP_MATERIALIZE);
    MaterializeItem* matItem = workItem.mutable_materializedata();

    ColumnMessage* idxCol = matItem->mutable_indexcolumn();
    ColumnMessage* filterCol = matItem->mutable_filtercolumn();
    ColumnMessage* outputCol = matItem->mutable_outputcolumn();

    setTableColumnType(idxCol, idxColumn);
    setTableColumnType(filterCol, filterColumn);
    setTableColumnType(outputCol, outColumn);

    return workItem;
}

/// MULTI GROUP ITEM

WorkItem ItemBuilder::createMultiGroupItem(const MultiGroupNode& node) {
    return createMultiGroupItem(node.groupColumns, node.outputIdx, node.outputCluster, node.aggColumn,
        node.aggResultColumn, node.storeExtends, node.sortOrders);
}

WorkItem ItemBuilder::createMultiGroupItem(const std::vector<BaseType::TableColumn*>& groupColumns, const BaseType::TableColumn* outIdx,
    const BaseType::TableColumn* outCluster, const BaseType::TableColumn* aggColumn, const BaseType::TableColumn* aggResultColumn,
    const bool& storeExtends, const std::vector<bool>& sortOrders)
{
    WorkItem workItem = createWorkItem(OperatorType::OP_GROUPBY); // TODO correct?
    MultiGroupItem* multiGrpItem = workItem.mutable_multigroupdata();

    uint idx = 0;
    for (BaseType::TableColumn* column : groupColumns) {
        multiGrpItem->add_groupcolumns();
        ColumnMessage* colMessage = multiGrpItem->mutable_groupcolumns(idx);
        setTableColumnType(colMessage, column);
        idx++;
    }

    ColumnMessage* outputIdx = multiGrpItem->mutable_outputindex();
    ColumnMessage* outputClusters = multiGrpItem->mutable_outputclusters();
    ColumnMessage* aggCol = multiGrpItem->mutable_aggregationcolumn();
    ColumnMessage* aggResultCol = multiGrpItem->mutable_aggregationresultcolumn();

    setTableColumnType(outputIdx, outIdx);
    setTableColumnType(outputClusters, outCluster);
    setTableColumnType(aggCol, aggColumn);
    setTableColumnType(aggResultCol, aggResultColumn);

    multiGrpItem->set_storeextends(storeExtends);

    for (const bool& order : sortOrders) {
        multiGrpItem->add_sortorders(order);
    }
    return workItem;
}

// SET OPERATION ITEM

WorkItem ItemBuilder::createSetOperationItem(const SetOperationNode& node) {
    return createSetOperationItem(node.operation, node.innerColumn, node.outerColumn, node.outputColumn);
}

WorkItem ItemBuilder::createSetOperationItem(const RelOp& operation, const BaseType::TableColumn* innerColumn,
    const BaseType::TableColumn* outerColumn, const BaseType::TableColumn* outputColumn)
{
    WorkItem workItem = createWorkItem(OperatorType::OP_SETOPERATION);
    SetOperationItem* setOpItem = workItem.mutable_setdata();

    setOpItem->set_operation(operation); // TODO is optional?

    ColumnMessage* innerCol = setOpItem->mutable_innercolumn();
    ColumnMessage* outerCol = setOpItem->mutable_outercolumn();
    ColumnMessage* outputCol = setOpItem->mutable_outputcolumn();

    setTableColumnType(innerCol, innerColumn);
    setTableColumnType(outerCol, outerColumn);
    setTableColumnType(outputCol, outputColumn);

    return workItem;
}

// SORT ITEM

WorkItem ItemBuilder::createSortItem(const SortNode& node) {
    return createSortItem(node.inputColumns, node.idxOutput, node.existingIdx, node.sortOrders);
}

WorkItem ItemBuilder::createSortItem(const std::vector<BaseType::TableColumn*>& inputColumns, const BaseType::TableColumn* idxOutput,
    const BaseType::TableColumn* existingIdx, const std::vector<bool>& sortOrders)
{
    WorkItem workItem = createWorkItem(OperatorType::OP_SORT);
    SortItem* sortItem = workItem.mutable_sortdata();

    uint idx = 0;
    for (BaseType::TableColumn* column : inputColumns) {
        sortItem->add_inputcolumns();
        ColumnMessage* colMessage = sortItem->mutable_inputcolumns(idx);
        setTableColumnType(colMessage, column);
        idx++;
    }

    ColumnMessage* idxOut = sortItem->mutable_indexoutput();
    ColumnMessage* exIdx = sortItem->mutable_existingindex();

    setTableColumnType(idxOut, idxOutput);
    setTableColumnType(exIdx, existingIdx);

    // idx = 0;
    for (const bool& order : sortOrders) {
        // sortItem->set_sortorder(idx, order);
        // idx++;
        sortItem->add_sortorder(order);
    }
    return workItem;
}

// AGGREGATE ITEM

WorkItem ItemBuilder::createAggItem(const AggNode& node) {
    return createAggItem(node.inputColumn, node.outputColumn, node.aggFunc, node.groupColumns);
}

WorkItem ItemBuilder::createAggItem(const BaseType::TableColumn* inputColumn, const BaseType::TableColumn* outputColumn,
    const AggFunc& aggFunc, const std::vector<std::string>& groupColumns)
{
    WorkItem workItem = createWorkItem(OperatorType::OP_AGGREGATE);
    AggItem* aggItem = workItem.mutable_aggdata();

    ColumnMessage* inputCol = aggItem->mutable_inputcolumn();
    ColumnMessage* outputCol = aggItem->mutable_outputcolumn();

    setTableColumnType(inputCol, inputColumn);
    setTableColumnType(outputCol, outputColumn);

    aggItem->set_aggfunc(aggFunc);

    for (const std::string& name : groupColumns) {
        aggItem->add_groupcolumns(name);
    }
    return workItem;
}

// RESULT ITEM

WorkItem ItemBuilder::createResultItem(const ResultNode& node) {
    return createResultItem(node.filename, node.resultColumns, node.resultIdx, node.resultHeaders);
}

WorkItem ItemBuilder::createResultItem(const std::string& file, const std::vector<BaseType::TableColumn*>& resultColumns,
    const BaseType::TableColumn* resultIdx, const std::vector<std::string>& headers)
{
    WorkItem workItem = createWorkItem();
    ResultItem* resultItem = workItem.mutable_resultdata();

    uint idx = 0;
    for (BaseType::TableColumn* column : resultColumns) {
        resultItem->add_resultcolumns();
        ColumnMessage* colMessage = resultItem->mutable_resultcolumns(idx);
        setTableColumnType(colMessage, column);
        idx++;
    }

    resultItem->set_filename(file);

    for (const std::string& header : headers) {
        resultItem->add_resultheader(header);
    }
    return workItem;
}


std::vector<WorkItem> ItemBuilder::createWorkItems(std::vector<const PlanNode*>& nodes) {

    std::vector<WorkItem> workItems;

    for (const auto& node : nodes) {

        if (auto* item = std::get_if<ItemBuilder::FetchNode>(&node->apiData)) {
            WorkItem w = ItemBuilder::createFetchItem(*item);
            workItems.push_back(w);
        }
        else if (auto* item = std::get_if<ItemBuilder::MaterializeNode>(&node->apiData)) {
            WorkItem w = ItemBuilder::createMaterializeItem(*item);
            workItems.push_back(w);
        }
        else if (auto* item = std::get_if<ItemBuilder::FilterNode>(&node->apiData)) {
            WorkItem w = ItemBuilder::createFilterItem(*item);
            workItems.push_back(w);
        }
        else if (auto* item = std::get_if<ItemBuilder::JoinNode>(&node->apiData)) {
            WorkItem w = ItemBuilder::createJoinItem(*item);
            workItems.push_back(w);
        }
        else if (auto* item = std::get_if<ItemBuilder::MapNode>(&node->apiData)) {
            WorkItem w = ItemBuilder::createMapItem(*item);
            workItems.push_back(w);
        }
        else if (auto* item = std::get_if<ItemBuilder::MultiGroupNode>(&node->apiData)) {
            WorkItem w = ItemBuilder::createMultiGroupItem(*item);
            workItems.push_back(w);
        }
        else if (auto* item = std::get_if<ItemBuilder::SetOperationNode>(&node->apiData)) {
            WorkItem w = ItemBuilder::createSetOperationItem(*item);
            workItems.push_back(w);
        }
        else if (auto* item = std::get_if<ItemBuilder::SortNode>(&node->apiData)) {
            WorkItem w = ItemBuilder::createSortItem(*item);
            workItems.push_back(w);
        }
        else if (auto* item = std::get_if<ItemBuilder::AggNode>(&node->apiData)) {
            WorkItem w = ItemBuilder::createAggItem(*item);
            workItems.push_back(w);
        }
        else if (auto* item = std::get_if<ItemBuilder::ResultNode>(&node->apiData)) {
            WorkItem w = ItemBuilder::createResultItem(*item);
            workItems.push_back(w);
        }
        
        std::cout << " -- ";
            printNode(*node, 2);
            std::cout << std::endl;
    }

    return workItems;
}
