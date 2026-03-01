#include "ir/ir_transformer.hpp"
#include "ir/ir_views.hpp"
#include "translation/item_builder.hpp"
#include <set>
#include <algorithm>

void irToApiDataSub(PlanNode* node, std::set<const PlanNode*>& visited) {
    if (!node || visited.contains(node)) return;

    static BaseType::TableColumn missingCol;

    // Filter Node
    if (auto filterV = node->irData.get_view_if<FilterView>()) {
        ItemBuilder::FilterNode filterStruct;
        filterStruct.inputColumn = &filterV->col1();
        filterStruct.outputColumn = &filterV->outputCol();
        filterStruct.filterType = filterV->filterType();
        filterStruct.filterArgVals = filterV->filterArgs();

        node->apiData = filterStruct;
    }

    // Join Node
    else if (auto joinV = node->irData.get_view_if<JoinView>()) {
        ItemBuilder::JoinNode joinStruct;

        joinStruct.innerColumn = &joinV->inner();
        joinStruct.outerColumn = &joinV->outer();
        joinStruct.iOutputColumn = &joinV->innerOut();
        joinStruct.oOutputColumn = &joinV->outerOut();
        joinStruct.joinPredicate = &joinV->joinPredicate();

        node->apiData = joinStruct;
    }

    // Semi Join Node
    else if (auto joinV = node->irData.get_view_if<SemiJoinView>()) {
        ItemBuilder::SemiJoinNode semiJoinStruct;

        semiJoinStruct.innerColumn = &joinV->inner();
        semiJoinStruct.outerColumn = &joinV->outer();
        semiJoinStruct.iOutputColumn = &joinV->innerOut();
        semiJoinStruct.oOutputColumn = &joinV->outerOut();
        semiJoinStruct.joinPredicate = &joinV->joinPredicate();

        node->apiData = semiJoinStruct;
    }

    // Map Node

    else if (auto mapV = node->irData.get_view_if<MapView>()) {
        ItemBuilder::MapNode mapStruct;

        mapStruct.inputColumn = &mapV->inputCol();
        mapStruct.outputColumn = &mapV->outputCol();
        mapStruct.operatorType = &mapV->operatorType();
        mapStruct.partnerVal = mapV->partnerVal();

        node->apiData = mapStruct;
    }

    // Group
    else if (auto groupV = node->irData.get_view_if<GroupView>()) {
        ItemBuilder::MultiGroupNode groupStruct;

        // Convert values to pointers
        for (auto& col : groupV->groupingCols()) {
            groupStruct.groupColumns.push_back(&col);
        }
        groupStruct.outputIdx = &groupV->outputIdx();
        groupStruct.outputSortIndex = &groupV->outputSortIdx();
        groupStruct.outputCluster = &groupV->outputCluster();
        if (auto aggCol = groupV->aggCol()) {
            if (auto aggResultCol = groupV->aggResultCol()) {
                groupStruct.aggColumn = aggCol;
                groupStruct.aggResultColumn = aggResultCol;
            }
        }
        else {
            groupStruct.aggColumn = &missingCol;
            groupStruct.aggResultColumn = &missingCol;
        }
        groupStruct.storeExtends = false;
        groupStruct.sortOrders = groupV->sortOrders();

        node->apiData = groupStruct;
    }

    // Aggregation
    else if (auto aggV = node->irData.get_view_if<AggView>()) {
        ItemBuilder::AggNode aggStruct;
        aggStruct.inputColumn = &aggV->colToAgg();
        aggStruct.outputColumn = &aggV->aggResultCol();
        aggStruct.aggFunc = aggV->aggFunc();
        // TODO: How to set group columns?
        aggStruct.groupColumns = {};

        node->apiData = aggStruct;
    }

    // Sort
    else if (auto sortV = node->irData.get_view_if<SortOrderView>()) {
        ItemBuilder::SortNode sortStruct;

        for (auto& desc : sortV->orderDescriptions()) {
            sortStruct.inputColumns.push_back(&desc.column);
            sortStruct.sortOrders.push_back(desc.orderType); // assuming bool maps directly
        }
        sortStruct.idxOutput = &sortV->idxOutput();
        sortStruct.existingIdx = sortV->existingIdx() ? &*sortV->existingIdx() : &missingCol;

        node->apiData = sortStruct;
    }

    // Set Operation
    else if (auto setOpV = node->irData.get_view_if<SetOpView>()) {
        ItemBuilder::SetOperationNode setStruct;
        setStruct.operation = setOpV->operation();

        // IR stores inputs in a vector, Builder wants explicit pointers
        setStruct.innerColumn = &setOpV->innerCol();
        setStruct.outerColumn = &setOpV->outerCol();
        setStruct.outputColumn = &setOpV->outputCol();

        node->apiData = setStruct;
    }

    // Materialize
    else if (auto matV = node->irData.get_view_if<MaterializeView>()) {

        ItemBuilder::MaterializeNode matStruct;

        matStruct.idxColumn = &matV->idxCol();
        matStruct.filterColumn = &matV->filterCol();
        matStruct.outputColumn = &matV->outputCol();

        node->apiData = matStruct;
    }

    // Select
    else if (auto selectV = node->irData.get_view_if<SelectView>()) {
        ItemBuilder::ResultNode resultStruct;

        // MISSING INFO: Filename is not in IR.
        resultStruct.filename = "result.csv";

        for (auto& col : selectV->resultCols()) {
            resultStruct.resultColumns.push_back(&col);
        }
        resultStruct.resultHeaders = selectV->resultHeaders();
        resultStruct.resultIdx = selectV->resultIdx().has_value() ? &selectV->resultIdx().value() : &missingCol;

        node->apiData = resultStruct;
    }

    // Erase FetchNodes
    node->children.erase(std::remove_if(node->children.begin(), node->children.end(),
                                        [](const std::shared_ptr<PlanNode>& child) {
                                            return child->irData.is<FetchOp>();
                                        }),
                         node->children.end());

    for (const auto& child : node->children){
        irToApiData(child.get());
    }
}

void irToApiData(PlanNode* node) {
    std::set<const PlanNode*> visited;
    irToApiDataSub(node, visited);
}
