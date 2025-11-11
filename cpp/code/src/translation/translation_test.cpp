#include <vector>
#include <WorkItem.pb.h>
#include "generate_AST.h"

// Does not compile, don't run it

unsigned int planId = 1;
unsigned int itemId = 1;

class WorkNode {
    public:
        unsigned int planId;
        unsigned int itemId;
        unsigned int operatorId;
        std::string name;
};

class FilterNode : public WorkNode {
    public:
        FilterNode() {
            operatorId = 1; // OperatorType.OP_FILTER;
        }

        std::string inputTable;
        std::string inputColumn;
        std::string outputTable;
        std::string outputColumn;
};

std::vector<WorkNode*> workNodes;

WorkNode* transformNode(ASTNode* node) {
    WorkNode* workNode = new WorkNode();

    workNode->planId = planId;
    workNode->itemId = itemId;

    return workNode;
}

void setTableColumnType(ColumnMessage& columnMessage, const std::string& tabName, const std::string& colName, int colType) {
    innerCol->set_tabname(tabName);
    innerCol->set_colname(colName);
    innerCol->set_coltype(static_cast<ColumnType>(colType)); // replace with ::ColumnType?
}

WorkItem* createWorkItem() {
    WorkItem workItem;
    workItem.set_planid(1);
    workItem.set_itemid(1);
    workItem.set_operatorid(static_cast<OperatorType>(1));
    return workItem;
}

FilterItem* createFilterItem(ASTNode* astNode) {
    if (astNode == nullptr)
        return nullptr;

    WorkItem workItem = createWorkItem();

    FilterItem* filterItem = workItem.mutable_filterdata();
    ColumnMessage* inputColumn = filterItem->mutable_inputcolumn();
    ColumnMessage* outputColumn = filterItem->mutable_outputcolumn();

    inputColumn->set_tabname(to_string(astNode->left)); // smthing like that
    inputColumn->set_colname("");
    inputColumn->set_coltype(static_cast<ColumnType>(1));

    outputColumn->set_tabname("");
    outputColumn->set_colname("");
    outputColumn->set_coltype(static_cast<ColumnType>(1));

    return filterItem;
}

JoinItem* createJoinItem(ASTNode* node) {
    WorkItem workItem = createWorkItem();

    JoinItem* joinItem = workItem.mutable_joindata();
    ColumnMessage* innerCol = joinItem->mutable_innercolumn();
    ColumnMessage* outerCol = joinItem->mutable_outercolumn();
    ColumnMessage* outputCol = joinItem->mutable_outputcolumn();

    /* innerCol->set_tabname(tuddbs::Utility::generateRandomString());
    innerCol->set_colname(tuddbs::Utility::generateRandomString());
    innerCol->set_coltype(static_cast<ColumnType>(tuddbs::Utility::generateRandomNumber(0, 4))); */

    setTableColumnType(innerCol, "tabl1", "column1", 1);

    /* outerCol->set_tabname(tuddbs::Utility::generateRandomString());
    outerCol->set_colname(tuddbs::Utility::generateRandomString());
    outerCol->set_coltype(static_cast<ColumnType>(tuddbs::Utility::generateRandomNumber(0, 4))); */

    setTableColumnType(outerCol, "tabl2", "column2", 2);

    /* outputCol->set_tabname(tuddbs::Utility::generateRandomString());
    outputCol->set_colname(tuddbs::Utility::generateRandomString());
    outputCol->set_coltype(static_cast<ColumnType>(5)); */

    setTableColumnType(outputCol, "tabl3", "column3", 3);

    joinItem->set_joinpredicate(static_cast<CompType>(tuddbs::Utility::generateRandomNumber(0, 5)));

    return joinItem;
}

/* FetchItem* createFetchItem() {
    FetchItem fetchItem =
} */

std::vector<WorkItem*> createQueryPlan(std::vector<ASTNode*> nodes) {
    std::vector<WorkItem*> items;

    for (ASTNode* astNode : nodes) {
        switch (astNode.type) {
            case FILTER:
                items.push_back(createFilterItem());
                break;

            default:
                std::cout << "Error: Node type not found." << std::endl;
                // TODO: Error handling
                break;
        }
    }
    return items;
}

/* itemType="int_filter",
        planId=planId, itemId=3, operatorId=WorkItem.OP_FILTER,
        inputTable="dates", inputColumn="d_year", inputType=WorkItem.TYPE_INTEGER,
        outputTable="intermediate", outputColumn="q_1_1_it_3", outputType=WorkItem.TYPE_BITMASK,
        filterType=WorkItem.COMP_EQ, filterArgVals=[1993]) */
