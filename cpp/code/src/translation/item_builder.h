#include <vector>
#include <WorkItem.pb.h>

#include "generate_AST.h"

class ItemBuilder {
    public:
        std::vector<WorkItem*> workList;

        struct TableColumn {
            std::string tableName;
            std::string columnName;
            ColumnType columnType;
        };

        struct FilterNode {
            TableColumn* inputColumn;
            TableColumn* outputColumn;
            CompType filterType;
            std::vector<std::variant<uint64_t, float, std::string>> filterArgVals;
        };

        struct MultiGroupNode {
            std::vector<TableColumn*> groupColumns;
            TableColumn* outputIdx;
            TableColumn* outputCluster;
            TableColumn* aggColumn;
            TableColumn* aggResultColumn;
            bool storeExtends;
            std::vector<bool> sortOrders;
        };

        struct JoinNode {
            TableColumn* innerColumn;
            TableColumn* outerColumn;
            TableColumn* outputColumn;
            CompType* joinPredicate;
        };

        struct AggNode {
            TableColumn* inputColumn;
            TableColumn* outputColumn;
            AggFunc aggFunc;
            std::vector<std::string> groupColumns;
        };

        struct SortNode {
            std::vector<TableColumn*> inputColumns;
            TableColumn* idxOutput;
            TableColumn* existingIdx;
            std::vector<bool> sortOrders;
        };

        struct MapNode {
            TableColumn* inputColumn;
            TableColumn* outputColumn;
            ArithOp* operatorType;
            std::variant<TableColumn, uint64_t, float, std::string> partnerVal;
        };

        struct MaterializeNode {
            TableColumn* idxColumn;
            TableColumn* filterColumn;
            TableColumn* outputColumn;
        };

        struct SetOperationNode {
            RelOp operation;
            TableColumn* innerColumn;
            TableColumn* outerColumn;
            TableColumn* outputColumn;
        };

        struct FetchNode {
            TableColumn* inputColumn;
            bool printToFile;
        };

        struct ResultNode {
            std::string filename;
            std::vector<TableColumn*> resultColumns;
            TableColumn* resultIdx;
            std::vector<std::string> resultHeaders;
        };

        TableColumn createTableColumn(const std::string& tableName, const std::string& columnName, const ColumnType& columnType) {
            TableColumn tableCol;
            tableCol.tableName = tableName;
            tableCol.columnName = columnName;
            tableCol.columnType = columnType;
            return tableCol;
        }

        WorkItem createWorkItem();
        WorkItem createWorkItem(const OperatorType& operatorType);
        WorkItem createWorkItem(const uint32_t& planId, const uint32_t& itemId, const OperatorType& operatorType);

        WorkItem createFilterItem(const FilterNode& node);
        WorkItem createMultiGroupItem(const MultiGroupNode& node);
        WorkItem createJoinItem(const JoinNode& node);
        WorkItem createAggItem(const AggNode& node);
        WorkItem createSortItem(const SortNode& node);
        WorkItem createMapItem(const MapNode& node);
        WorkItem createMaterializeItem(const MaterializeNode& node);
        WorkItem createSetOperationItem(const SetOperationNode& node);
        WorkItem createFetchItem(const FetchNode& node);
        WorkItem createResultItem(const ResultNode& node);

        WorkItem createFilterItem(const TableColumn* inColumn, const TableColumn* outColumn, const CompType& filterType,
            const std::vector<std::variant<uint64_t, float, std::string>>& filterArgVals);

        WorkItem createJoinItem(const TableColumn* innerColumn, const TableColumn* outerColumn,
            const TableColumn* outColumn, const CompType* predicate);

        WorkItem createMapItem(const TableColumn* inColumn, const TableColumn* outColumn, const ArithOp* operatorType,
            const std::variant<TableColumn, uint64_t, float, std::string>& partnerVal);

        WorkItem createMaterializeItem(const TableColumn* idxColumn, const TableColumn* filterColumn,
            const TableColumn* outColumn);

        WorkItem createMultiGroupItem(const std::vector<TableColumn*>& groupColumns, const TableColumn* outIdx,
            const TableColumn* outCluster, const TableColumn* aggColumn, const TableColumn* aggResultColumn,
            const bool& storeExtends, const std::vector<bool>& sortOrders);

        WorkItem createResultItem(const std::string& file, const std::vector<TableColumn*>& resultColumns,
            const TableColumn* resultIdx, const std::vector<std::string>& headers);

        WorkItem createSetOperationItem(const RelOp& operation, const TableColumn* innerColumn,
            const TableColumn* outerColumn, const TableColumn* outputColumn);

        WorkItem createSortItem(const std::vector<TableColumn*>& inputColumns, const TableColumn* idxOutput,
            const TableColumn* existingIdx, const std::vector<bool>& sortOrders);

        WorkItem createAggItem(const TableColumn* inputColumn, const TableColumn* outputColumn,
            const AggFunc& aggFunc, const std::vector<std::string>& groupColumns);

        WorkItem createFetchItem(const TableColumn* inputColumn, const bool printToFile);
};
