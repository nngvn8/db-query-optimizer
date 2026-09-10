/**
 * @file item_builder.hpp
 * @brief Defines the ItemBuilder class for creating WorkItems from the query plan IR.
 *
 * This file contains the declaration of the `ItemBuilder` class, which is responsible
 * for translating the final, optimized query plan IR into a sequence of `WorkItem`
 * objects. These `WorkItem`s represent the individual operations that the database
 * execution engine will perform.
 */

#pragma once

#include <vector>
#include <WorkItem.pb.h>
#include <variant>

#include "ir/base_types.hpp"

class PlanNode;

/**
 * @class ItemBuilder
 * @brief A class for building `WorkItem` objects from the query plan IR.
 */
class ItemBuilder {
    public:
        std::vector<WorkItem*> workList;

        /**
         * @struct FetchNode
         * @brief Describes a fetch operation for the ItemBuilder.
         */
        struct FetchNode {
            BaseType::TableColumn* inputColumn;
            bool printToFile;
        };

        /**
         * @struct FilterNode
         * @brief Describes a filter operation for the ItemBuilder.
         */
        struct FilterNode {
            BaseType::TableColumn* inputColumn;
            BaseType::TableColumn* outputColumn;
            CompType filterType;
            std::vector<std::variant<uint64_t, float, std::string>> filterArgVals;
        };

        /**
         * @struct JoinNode
         * @brief Describes a join operation for the ItemBuilder.
         */
        struct JoinNode {
            BaseType::TableColumn* innerColumn;
            BaseType::TableColumn* outerColumn;
            BaseType::TableColumn* iOutputColumn;
            BaseType::TableColumn* oOutputColumn;
            CompType* joinPredicate;
        };

        /**
         * @struct SemiJoinNode
         * @brief Describes a semi-join operation for the ItemBuilder.
         */
        struct SemiJoinNode {
            BaseType::TableColumn* innerColumn;
            BaseType::TableColumn* outerColumn;
            BaseType::TableColumn* iOutputColumn;
            BaseType::TableColumn* oOutputColumn;
            CompType* joinPredicate;
        };

        /**
         * @struct MapNode
         * @brief Describes a map operation for the ItemBuilder.
         */
        struct MapNode {
            BaseType::TableColumn* inputColumn;
            BaseType::TableColumn* outputColumn;
            ArithOp* operatorType;
            std::variant<BaseType::TableColumn, uint64_t, float, std::string> partnerVal;
        };

        /**
         * @struct MaterializeNode
         * @brief Describes a materialize operation for the ItemBuilder.
         */
        struct MaterializeNode {
            BaseType::TableColumn* idxColumn;
            BaseType::TableColumn* filterColumn;
            BaseType::TableColumn* outputColumn;
        };

        /**
         * @struct MultiGroupNode
         * @brief Describes a multi-group operation for the ItemBuilder.
         */
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

        /**
         * @struct SetOperationNode
         * @brief Describes a set operation for the ItemBuilder.
         */
        struct SetOperationNode {
            RelOp operation;
            BaseType::TableColumn* innerColumn;
            BaseType::TableColumn* outerColumn;
            BaseType::TableColumn* outputColumn;
        };

        /**
         * @struct SortNode
         * @brief Describes a sort operation for the ItemBuilder.
         */
        struct SortNode {
            std::vector<BaseType::TableColumn*> inputColumns;
            BaseType::TableColumn* idxOutput;
            BaseType::TableColumn* existingIdx;
            std::vector<bool> sortOrders;
        };

        /**
         * @struct AggNode
         * @brief Describes an aggregation operation for the ItemBuilder.
         */
        struct AggNode {
            BaseType::TableColumn* inputColumn;
            BaseType::TableColumn* outputColumn;
            AggFunc aggFunc;
            std::vector<std::string> groupColumns;
        };

        /**
         * @struct ResultNode
         * @brief Describes a result operation for the ItemBuilder.
         */
        struct ResultNode {
            std::string filename; // unique name or empty, rather not null might crash
            std::vector<BaseType::TableColumn*> resultColumns; // input columns to display
            BaseType::TableColumn* resultIdx; // position list (do not need to set)
            std::vector<std::string> resultHeaders; // name to display
        };

        /**
         * @brief Creates a TableColumn object.
         */
        BaseType::TableColumn createTableColumn(const std::string& tableName, const std::string& columnName, const ColumnType& columnType) {
            return BaseType::TableColumn(BaseType::Table(tableName), columnName, columnType);
        }

        /**
         * @brief Creates a default WorkItem.
         */
        WorkItem createWorkItem();
        /**
         * @brief Creates a WorkItem with a specific operator type.
         */
        WorkItem createWorkItem(const OperatorType& operatorType);

        /**
         * @brief Creates a WorkItem for a fetch operation.
         */
        WorkItem createFetchItem(const FetchNode& node);
        /**
         * @brief Creates a WorkItem for a filter operation.
         */
        WorkItem createFilterItem(const FilterNode& node);
        /**
         * @brief Creates a WorkItem for a join operation.
         */
        WorkItem createJoinItem(const JoinNode& node);
        /**
         * @brief Creates a WorkItem for a map operation.
         */
        WorkItem createMapItem(const MapNode& node);
        /**
         * @brief Creates a WorkItem for a materialize operation.
         */
        WorkItem createMaterializeItem(const MaterializeNode& node);
        /**
         * @brief Creates a WorkItem for a multi-group operation.
         */
        WorkItem createMultiGroupItem(const MultiGroupNode& node);
        /**
         * @brief Creates a WorkItem for a set operation.
         */
        WorkItem createSetOperationItem(const SetOperationNode& node);
        /**
         * @brief Creates a WorkItem for a sort operation.
         */
        WorkItem createSortItem(const SortNode& node);
        /**
         * @brief Creates a WorkItem for an aggregation operation.
         */
        WorkItem createAggItem(const AggNode& node);
        /**
         * @brief Creates a WorkItem for a result operation.
         */
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

        /**
         * @brief Creates a vector of WorkItems from a sequence of plan nodes.
         * @param nodes The sequence of plan nodes.
         * @param planId The Id for the Item plan.
         * @return A vector of WorkItems.
         */
        std::vector<WorkItem> createWorkItems(std::vector<const PlanNode*>& nodes, int planId);
};
