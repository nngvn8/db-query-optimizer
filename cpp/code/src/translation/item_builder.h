#pragma once

#include <vector>
#include <WorkItem.pb.h>
#include <variant>
#include <memory>

#include <ir/base_types.hpp>

class PlanNode;

class ItemBuilder {
    public:
        std::vector<WorkItem*> workList;

        struct FetchNode {
            BaseType::TableColumn* inputColumn;
            bool printToFile;
        };

        struct FilterNode {
            BaseType::TableColumn* inputColumn;
            BaseType::TableColumn* outputColumn;
            CompType filterType;
            std::vector<std::variant<uint64_t, float, std::string>> filterArgVals;
        };

        struct JoinNode {
            BaseType::TableColumn* innerColumn;
            BaseType::TableColumn* outerColumn;
            BaseType::TableColumn* iOutputColumn;
            BaseType::TableColumn* oOutputColumn;
            CompType* joinPredicate;
        };

        struct MapNode {
            BaseType::TableColumn* inputColumn;
            BaseType::TableColumn* outputColumn;
            ArithOp* operatorType;
            std::variant<BaseType::TableColumn, uint64_t, float, std::string> partnerVal;
        };

        struct MaterializeNode {
            BaseType::TableColumn* idxColumn;
            BaseType::TableColumn* filterColumn;
            BaseType::TableColumn* outputColumn;
        };

        struct MultiGroupNode {
            std::vector<BaseType::TableColumn*> groupColumns;
            BaseType::TableColumn* outputIdx; // sorted index (compressed _ext)
            BaseType::TableColumn* outputSortIndex;
            BaseType::TableColumn* outputCluster; // cluster sizes (for several aggs with sorted index)
            BaseType::TableColumn* aggColumn; // column to be aggregated
            BaseType::TableColumn* aggResultColumn; // column containing the result of aggregations
            bool storeExtends;
            std::vector<bool> sortOrders;
        };

        struct SetOperationNode {
            RelOp operation;
            BaseType::TableColumn* innerColumn;
            BaseType::TableColumn* outerColumn;
            BaseType::TableColumn* outputColumn;
        };

        struct SortNode {
            std::vector<BaseType::TableColumn*> inputColumns;
            BaseType::TableColumn* idxOutput;
            BaseType::TableColumn* existingIdx;
            std::vector<bool> sortOrders;
        };

        struct AggNode {
            BaseType::TableColumn* inputColumn;
            BaseType::TableColumn* outputColumn;
            AggFunc aggFunc;
            std::vector<std::string> groupColumns;
        };

        struct ResultNode {
            std::string filename; // unique name or empty, rather not null might crash
            std::vector<BaseType::TableColumn*> resultColumns; // input columns to display
            BaseType::TableColumn* resultIdx; // position list (do not need to set)
            std::vector<std::string> resultHeaders; // name to display
        };

        BaseType::TableColumn createTableColumn(const std::string& tableName, const std::string& columnName, const ColumnType& columnType) {
            return BaseType::TableColumn(BaseType::Table(tableName), columnName, columnType);
        }

        WorkItem createWorkItem();
        WorkItem createWorkItem(const OperatorType& operatorType);
        WorkItem createWorkItem(const uint32_t& planId, const uint32_t& itemId, const OperatorType& operatorType);

        WorkItem createFetchItem(const FetchNode& node);
        WorkItem createFilterItem(const FilterNode& node);
        WorkItem createJoinItem(const JoinNode& node);
        WorkItem createMapItem(const MapNode& node);
        WorkItem createMaterializeItem(const MaterializeNode& node);
        WorkItem createMultiGroupItem(const MultiGroupNode& node);
        WorkItem createSetOperationItem(const SetOperationNode& node);
        WorkItem createSortItem(const SortNode& node);
        WorkItem createAggItem(const AggNode& node);
        WorkItem createResultItem(const ResultNode& node);

        WorkItem createFetchItem(const BaseType::TableColumn* inputColumn, const bool printToFile);

        WorkItem createFilterItem(const BaseType::TableColumn* inColumn, const BaseType::TableColumn* outColumn, const CompType& filterType,
            const std::vector<std::variant<uint64_t, float, std::string>>& filterArgVals);

        WorkItem createJoinItem(const BaseType::TableColumn* innerColumn, const BaseType::TableColumn* outerColumn,
            const BaseType::TableColumn* iOutputColumn, const BaseType::TableColumn* oOutputColumn, const CompType* predicate);

        WorkItem createMapItem(const BaseType::TableColumn* inColumn, const BaseType::TableColumn* outColumn, const ArithOp* operatorType,
            const std::variant<BaseType::TableColumn, uint64_t, float, std::string>& partnerVal);

        WorkItem createMaterializeItem(const BaseType::TableColumn* idxColumn, const BaseType::TableColumn* filterColumn,
            const BaseType::TableColumn* outColumn);

        WorkItem createMultiGroupItem(const std::vector<BaseType::TableColumn*>& groupColumns, const BaseType::TableColumn* outIdx,
            const BaseType::TableColumn* outputSortIndex, const BaseType::TableColumn* outCluster, const BaseType::TableColumn* aggColumn,
            const BaseType::TableColumn* aggResultColumn, const bool& storeExtends, const std::vector<bool>& sortOrders);

        WorkItem createSetOperationItem(const RelOp& operation, const BaseType::TableColumn* innerColumn,
            const BaseType::TableColumn* outerColumn, const BaseType::TableColumn* outputColumn);

        WorkItem createSortItem(const std::vector<BaseType::TableColumn*>& inputColumns, const BaseType::TableColumn* idxOutput,
            const BaseType::TableColumn* existingIdx, const std::vector<bool>& sortOrders);

        WorkItem createAggItem(const BaseType::TableColumn* inputColumn, const BaseType::TableColumn* outputColumn,
            const AggFunc& aggFunc, const std::vector<std::string>& groupColumns);

        WorkItem createResultItem(const std::string& file, const std::vector<BaseType::TableColumn*>& resultColumns,
            const BaseType::TableColumn* resultIdx, const std::vector<std::string>& headers);

        std::vector<WorkItem> createWorkItems(std::vector<const PlanNode*>& nodes);
};
