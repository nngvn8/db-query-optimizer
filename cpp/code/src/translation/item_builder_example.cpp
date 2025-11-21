#include <iostream>
#include <string>
#include "item_builder.h"

int SINGLE_TEST;
ItemBuilder* itemBuilder;

ItemBuilder::TableColumn* tc1;
ItemBuilder::TableColumn* tc2;
ItemBuilder::TableColumn* tc3;
ItemBuilder::TableColumn* tc4;

void printName(const std::string& name) {
    std::cout << "[" << name << "]:" << std::endl;
}

void printItem(const WorkItem& item) {
    std::cout << item.DebugString() << std::endl;
}

void printSortItem() {
    printName("Sort Item");
    ItemBuilder::SortNode sortNode;

    sortNode.inputColumns.push_back(tc1);
    sortNode.inputColumns.push_back(tc2);
    sortNode.inputColumns.push_back(tc3);
    sortNode.existingIdx = tc4;
    sortNode.idxOutput = tc1;

    WorkItem workItem = itemBuilder->createSortItem(sortNode);
    printItem(workItem);
}

void printMapItem() {
    printName("Map Item");
    ItemBuilder::MapNode mapNode;
    mapNode.inputColumn = tc1;
    mapNode.outputColumn = tc2;
    ArithOp ao = static_cast<ArithOp>(1);
    mapNode.operatorType = &ao;

    // chose either one
    mapNode.partnerVal = (uint64_t) 10;
    // mapNode.partnerVal = (float) 12.34;
    // mapNode.partnerVal = "abc";
    // mapNode.partnerVal = *tc1;

    WorkItem workItem = itemBuilder->createMapItem(mapNode);
    printItem(workItem);
}

void printFetchItem() {
    printName("Fetch Item");
    ItemBuilder::FetchNode fetchNode;

    fetchNode.inputColumn = tc1;
    fetchNode.printToFile = true;

    WorkItem workItem = itemBuilder->createFetchItem(fetchNode);
    printItem(workItem);
}

void printFilterItem() {
    printName("Filter Item");
    ItemBuilder::FilterNode filterNode;

    filterNode.inputColumn = tc1;
    filterNode.outputColumn = tc2;
    filterNode.filterType = CompType::COMP_GE;
    filterNode.filterArgVals.push_back((uint64_t) 10);
    filterNode.filterArgVals.push_back((float) 12.34);
    filterNode.filterArgVals.push_back(("abcdefg"));

    WorkItem workItem = itemBuilder->createFilterItem(filterNode);
    printItem(workItem);
}

void printJoinItem() {
    printName("Join Item");
    ItemBuilder::JoinNode joinNode;

    joinNode.innerColumn = tc1;
    joinNode.outerColumn = tc2;
    joinNode.outputColumn = tc3;
    joinNode.joinPredicate = nullptr;

    WorkItem workItem = itemBuilder->createJoinItem(joinNode);
    printItem(workItem);
}

void printMatItem() {
    printName("Materialize");
    ItemBuilder::MaterializeNode matNode;

    matNode.idxColumn = tc1;
    matNode.filterColumn = tc2;
    matNode.outputColumn = tc3;

    WorkItem workItem = itemBuilder->createMaterializeItem(matNode);
    printItem(workItem);
}

void printMultiGrpItem() {
    printName("Multi Group");
    ItemBuilder::MultiGroupNode mgNode;

    mgNode.aggColumn = tc1;
    mgNode.aggResultColumn = tc2;
    mgNode.groupColumns.push_back(tc3);
    mgNode.groupColumns.push_back(tc4);
    mgNode.outputCluster = tc1;
    mgNode.outputIdx = tc2;
    mgNode.sortOrders.push_back(true);
    mgNode.sortOrders.push_back(false);
    mgNode.storeExtends = false;

    WorkItem workItem = itemBuilder->createMultiGroupItem(mgNode);
    printItem(workItem);
}

void printSetOpItem() {
    printName("Set Operation");
    ItemBuilder::SetOperationNode setNode;

    setNode.innerColumn = tc1;
    setNode.outerColumn = tc2;
    setNode.outputColumn = tc3;
    setNode.operation = RelOp::REL_INTERSECTION;

    WorkItem workItem = itemBuilder->createSetOperationItem(setNode);
    printItem(workItem);
}

void printAggItem() {
    printName("Aggregate");
    ItemBuilder::AggNode aggNode;

    aggNode.inputColumn = tc1;
    aggNode.outputColumn = tc2;
    aggNode.groupColumns.push_back("stringcol1");
    aggNode.groupColumns.push_back("stringcol2");
    aggNode.aggFunc = AggFunc::AGG_COUNT;

    WorkItem workItem = itemBuilder->createAggItem(aggNode);
    printItem(workItem);
}

void printResultItem() {
    printName("Result");
    ItemBuilder::ResultNode resNode;

    resNode.resultIdx = tc1;
    resNode.resultColumns.push_back(tc2);
    resNode.resultColumns.push_back(tc3);
    resNode.resultColumns.push_back(tc4);
    resNode.resultHeaders.push_back("header1");
    resNode.resultHeaders.push_back("header2");
    resNode.filename = "this/is/a/filename.txt";

    WorkItem workItem = itemBuilder->createResultItem(resNode);
    printItem(workItem);
}

void printFetchItemWithoutNode() {
    printName("Fetch with Params instead of Node");
    WorkItem workItem = itemBuilder->createFetchItem(tc1, true);
    printItem(workItem);
}

void printAllItems() {
    printSortItem();
    printMapItem();
    printFetchItem();
    printFetchItemWithoutNode();
    printFilterItem();
    printJoinItem();
    printMatItem();
    printMultiGrpItem();
    printSetOpItem();
    printAggItem();
    printResultItem();
}

int main() {
    itemBuilder = new ItemBuilder();

    tc1 = new ItemBuilder::TableColumn { "employees", "id", ColumnType::TYPE_INTEGER };
    tc2 = new ItemBuilder::TableColumn { "employees", "names", ColumnType::TYPE_STRING };
    tc3 = new ItemBuilder::TableColumn { "employees", "float", ColumnType::TYPE_FLOAT };
    tc4 = new ItemBuilder::TableColumn { "cities", "names", ColumnType::TYPE_STRING };

    if (SINGLE_TEST && SINGLE_TEST >= 0 && SINGLE_TEST <= 11) {
        switch (SINGLE_TEST) {
            case 0: printSortItem(); break;
            case 1: printMapItem(); break;
            case 2: printFetchItem(); break;
            case 3: printFetchItemWithoutNode(); break;
            case 4: printFilterItem(); break;
            case 5: printJoinItem(); break;
            case 6: printMatItem(); break;
            case 7: printMultiGrpItem(); break;
            case 8: printSetOpItem(); break;
            case 9: printAggItem(); break;
            case 10: printResultItem(); break;
            default: break;
        }
    } else {
        printAllItems();
    }

    delete itemBuilder;
    delete tc1;
    delete tc2;
    delete tc3;
    delete tc4;
    return 0;
}
