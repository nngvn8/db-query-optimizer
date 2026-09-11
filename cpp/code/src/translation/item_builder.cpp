#include "translation/item_builder.hpp"
#include "ir/plan_node.hpp"
#define DEBUG true

uint32_t currentPlanId = 0;
uint32_t currentItemId = 0;

void setTableColumnType(ColumnMessage* columnMessage, const BaseType::TableColumn* tableColumn) {
    columnMessage->set_tabname(tableColumn->table.name);
    columnMessage->set_colname(tableColumn->columnName);
    columnMessage->set_coltype(tableColumn->columnType);
    columnMessage->set_isbase(tableColumn->isBaseColumn);
}

// WORK ITEM

// TODO only for debug, remove in prod
WorkItem ItemBuilder::createWorkItem() {
    return createWorkItem(static_cast<OperatorType>(1));
}

WorkItem ItemBuilder::createWorkItem(const OperatorType& operatorType) {
    WorkItem workItem;
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
    return createJoinItem(node.innerColumn, node.outerColumn, node.iOutputColumn, node.oOutputColumn, node.joinPredicate);
}

WorkItem ItemBuilder::createJoinItem(const BaseType::TableColumn* innerColumn, const BaseType::TableColumn* outerColumn,
    const BaseType::TableColumn* iOutputColumn, const BaseType::TableColumn* oOutputColumn, const CompType* predicate)
{
    WorkItem workItem = createWorkItem(OperatorType::OP_HASHJOIN); // TODO or MERGEJOIN
    JoinItem* joinItem = workItem.mutable_joindata();

    ColumnMessage* innerCol = joinItem->mutable_innercolumn();
    ColumnMessage* outerCol = joinItem->mutable_outercolumn();
    ColumnMessage* iOutputCol = joinItem->mutable_ioutputcolumn();
    ColumnMessage* oOutputCol = joinItem->mutable_ooutputcolumn();

    setTableColumnType(innerCol, innerColumn);
    setTableColumnType(outerCol, outerColumn);
    setTableColumnType(iOutputCol, iOutputColumn);
    setTableColumnType(oOutputCol, oOutputColumn);

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

    // TODO: if verified, change this from hotfix to proper fix
    // idxCol and filterCol were most likely confused in the implementation logic (hotfix here)
    ColumnMessage* idxCol = matItem->mutable_filtercolumn();
    ColumnMessage* filterCol = matItem->mutable_indexcolumn();
    ColumnMessage* outputCol = matItem->mutable_outputcolumn();

    setTableColumnType(idxCol, idxColumn);
    setTableColumnType(filterCol, filterColumn);
    setTableColumnType(outputCol, outColumn);

    return workItem;
}

/// MULTI GROUP ITEM

WorkItem ItemBuilder::createMultiGroupItem(const MultiGroupNode& node) {
    return createMultiGroupItem(node.groupColumns, node.outputIdx, node.outputSortIndex, node.outputCluster, node.aggColumn,
        node.aggResultColumn, node.storeExtends, node.sortOrders);
}

WorkItem ItemBuilder::createMultiGroupItem(const std::vector<BaseType::TableColumn*>& groupColumns, const BaseType::TableColumn* outIdx,
    const BaseType::TableColumn* outputSortIndex, const BaseType::TableColumn* outCluster, const BaseType::TableColumn* aggColumn,
    const BaseType::TableColumn* aggResultColumn, const bool& storeExtends, const std::vector<bool>& sortOrders)
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
    WorkItem workItem = createWorkItem(OperatorType::OP_RESULT);
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


std::vector<WorkItem> ItemBuilder::createWorkItems(std::vector<const PlanNode*>& nodes, int planId) {
    
    std::map<const BaseType::TableColumn, int> columnProducerMap;
    std::vector<WorkItem> workItems;
    int itemId = 1;

    for (const auto& node : nodes) {
        WorkItem w;

        if (auto* item = std::get_if<ItemBuilder::FetchNode>(&node->apiData)) {
            w = ItemBuilder::createFetchItem(*item);
        }
        else if (auto* item = std::get_if<ItemBuilder::MaterializeNode>(&node->apiData)) {
            w = ItemBuilder::createMaterializeItem(*item);
        }
        else if (auto* item = std::get_if<ItemBuilder::FilterNode>(&node->apiData)) {
            w = ItemBuilder::createFilterItem(*item);
        }
        else if (auto* item = std::get_if<ItemBuilder::JoinNode>(&node->apiData)) {
            w = ItemBuilder::createJoinItem(*item);
        }
        else if (auto* item = std::get_if<ItemBuilder::MapNode>(&node->apiData)) {
            w = ItemBuilder::createMapItem(*item);
        }
        else if (auto* item = std::get_if<ItemBuilder::MultiGroupNode>(&node->apiData)) {
            w = ItemBuilder::createMultiGroupItem(*item);
        }
        else if (auto* item = std::get_if<ItemBuilder::SetOperationNode>(&node->apiData)) {
            w = ItemBuilder::createSetOperationItem(*item);
        }
        else if (auto* item = std::get_if<ItemBuilder::SortNode>(&node->apiData)) {
            w = ItemBuilder::createSortItem(*item);
        }
        else if (auto* item = std::get_if<ItemBuilder::AggNode>(&node->apiData)) {
            w = ItemBuilder::createAggItem(*item);
        }
        else if (auto* item = std::get_if<ItemBuilder::ResultNode>(&node->apiData)) {
            w = ItemBuilder::createResultItem(*item);
        }

        w.set_itemid(itemId);
        w.set_planid(planId);

        // Deduplication in case node has multiple input columns from same producer (e.g. joins)
        std::set<int> dependencies;
        for (const auto& col : node->irData.inputColumns) {
            // if (!col.isBaseColumn){
            //     dependencies.insert(columnProducerMap[col]);
            auto it = columnProducerMap.find(col);
            if (it != columnProducerMap.end()) {
                dependencies.insert(it->second);
            }
            // else {
                // Hard error: Plan wiring bug!
                // throw std::runtime_error(
                //     "Plan error: Intermediate column '" + col.table.name + "." + col.columnName + 
                //     "' is consumed by node with itemId " + std::to_string(itemId) + 
                //     ", but was never produced by any preceding WorkItem!"
                // );
            // }
        }

        // Add dependencies of this node
        for (const auto& dep : dependencies){
            w.add_dependson(dep);
        }

        // Register columns this node produces
        for (const auto& col : node->irData.outputCols) {
            columnProducerMap[col] = itemId;
        }

        workItems.push_back(w);
        itemId++;
    }

    return workItems;
}