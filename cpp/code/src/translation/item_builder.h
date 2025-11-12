#include <vector>
#include <WorkItem.pb.h>

#include "generate_AST.h"

class ItemBuilder {

    public:
        struct TableColumn {
            std::string table;
            std::string column;
            ColumnType columnType;
        };

        struct FilterNode {
            TableColumn tableColumn;
            CompType filterType;
            int filterArgVals[];
        };

        struct JoinNode {
            TableColumn innerTableColumn;
            TableColumn outerTableColumn;
        };

        WorkItem createWorkItem();
        FetchItem* createFetchItem(ASTNode* node);
        FilterItem* createFilterItem(FilterNode* node);
        JoinItem* createJoinItem(JoinNode* node);
        MapItem* createMapItem(ASTNode* node);
        MaterializeItem* createMaterializeItem(ASTNode* node);
        MultiGroupItem* createMultiGroupItem(ASTNode* node);
        ResultItem* createResultItem(ASTNode* node);
        SetOperationItem* createSetOperationItem(ASTNode* node);
        SortItem* createSortItem(ASTNode* node);
        AggItem* createAggItem(ASTNode* node);
};
