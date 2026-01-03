#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <ir/base_types.hpp>

namespace IR {

    class IrBaseNode {
        public:
            // Columns that store input that will be processed within the node
            std::vector<BaseType::TableColumn> inputColumns = {};
            
            // Generic helpers
            const BaseType::TableColumn& column() const { return inputColumns[0]; };
            const BaseType::TableColumn& column2() const { return inputColumns[1]; };
            
            virtual ~IrBaseNode() = default;
    };

    // Statement Nodes

    class StatementNode : public IrBaseNode {
        public:
            BaseType::TableColumn& column() { return inputColumns[0]; }
            // TODO extend this if update, delete, insert is added
    };
    
    class UpdateNode : public StatementNode {
        // TODO add update statements
    };
    
    class InsertNode : public StatementNode {
        // TODO add insert statements
    };
    
    class DeleteNode : public StatementNode {
        // TODO add delete statements
    };
    
    // Database Function Nodes
    
    class SelectNode : public IrBaseNode {
    public:
        bool star;
        bool distinct;

        SelectNode(
            const bool star,
            const BaseType::TableColumn& column,
            const bool distinct):
                star(star),
                distinct(distinct)
            {
                inputColumns.push_back(column);
            };

        const BaseType::TableColumn& column() const { return inputColumns[0]; }
    };

    class TableBaseNode : public IrBaseNode {
    public:
        BaseType::Table table;

        TableBaseNode(const BaseType::Table& table): table(table) {};
    };

    class FetchNode : public IrBaseNode {
    public:
        BaseType::Table table;
        bool printToFile;

        FetchNode(const BaseType::Table& table, const bool printToFile):
            table(table), printToFile(printToFile) {};
    };

    class AggNode : public IrBaseNode {
    public:
        AggFunc aggFunc;

        AggNode(
            const BaseType::TableColumn& column,
            const AggFunc& aggFunc)
            :
                aggFunc(aggFunc) 
            {
                inputColumns.push_back(column);
            };

        const BaseType::TableColumn& column() const { return inputColumns[0]; }
    };

    class JoinNode : public IrBaseNode {
    public:
        BaseType::Join joinType;
        CompType joinPredicate;

        JoinNode(
            const BaseType::Join& joinType,
            const CompType& joinPredicate,
            const BaseType::TableColumn& leftTableColumn,
            const BaseType::TableColumn& rightTableColumn):
                joinType(joinType),
                joinPredicate(joinPredicate)
            {
                inputColumns.push_back(leftTableColumn);
                inputColumns.push_back(rightTableColumn);
            };

        const BaseType::TableColumn& leftTableColumn() const { return inputColumns[0]; }
        const BaseType::TableColumn& rightTableColumn() const { return inputColumns[1]; }
    };

    class FilterNode : public IrBaseNode {
    public:
        CompType filterType;
        std::vector<std::variant<uint64_t, float, std::string>> filterArgs;
        BaseType::TableColumn outputColumn;

        FilterNode(
            const BaseType::TableColumn& column1,
            const CompType& filterType,
            const std::optional<BaseType::TableColumn>& column2,
            const std::vector<std::variant<uint64_t, float, std::string>>& filterArgs,
            const BaseType::TableColumn& outputColumn)
        :
            filterType(filterType),
            filterArgs(filterArgs),
            outputColumn(outputColumn)
        {
            inputColumns.push_back(column1);
            if(column2.has_value()){
                inputColumns.push_back(column2.value());
            }
        };

        const BaseType::TableColumn& column1() const { return inputColumns[0]; }
        
        const std::optional<BaseType::TableColumn> column2() const { 
            if (inputColumns.size() > 1) return inputColumns[1];
            return std::nullopt;
        }
    };

    class GroupByNode : public IrBaseNode {
    public:
        GroupByNode(const std::vector<BaseType::TableColumn>& description) 
        {
            inputColumns = description;
        };

        const std::vector<BaseType::TableColumn>& description() const { return inputColumns; }
        
        // GroupByNode(std::vector<GroupByDescription>& description) : description(description) {}
    };

    class SortOrderNode : public IrBaseNode {
    public:
        // Cannot remove this because OrderDescription contains extra data (ASC/DESC)
        std::vector<BaseType::OrderDescription> columnList;

        SortOrderNode(const std::vector<BaseType::OrderDescription>& columnList) : columnList(columnList) 
        {
            for(const auto& item : columnList){
                inputColumns.push_back(item.column);
            }
        };
        
        // OrderByNode(const std::vector<OrderByDescription>& list): {};
    };

    class LimitNode : public IrBaseNode {
    public:
        std::string limit;
        std::string offset;

        LimitNode(
            const std::string& limit = "",
            const std::string& offset = ""):
                limit(limit),
                offset(offset) {};
    };

    class MapNode : public IrBaseNode {
    public:
        ArithOp operatorType;
        std::variant<BaseType::TableColumn, uint64_t, float, std::string> partnerVal;

        MapNode(
            const BaseType::TableColumn& column,
            const ArithOp& operatorType,
            const std::variant<BaseType::TableColumn, uint64_t, float, std::string>& partnerVal):
                operatorType(operatorType),
                partnerVal(partnerVal) 
            {
                inputColumns.push_back(column);
                if (std::holds_alternative<BaseType::TableColumn>(partnerVal)) {
                    inputColumns.push_back(std::get<BaseType::TableColumn>(partnerVal));
                }
            };

        const BaseType::TableColumn& column() const { return inputColumns[0]; }
    };

    class SetOperationNode : public IrBaseNode {
    public:
        RelOp operation;

        SetOperationNode(
            const RelOp& operation,
            const BaseType::TableColumn& innerColumn,
            const BaseType::TableColumn& outerColumn):
                operation(operation)
            {
                inputColumns.push_back(innerColumn);
                inputColumns.push_back(outerColumn);
            };

        const BaseType::TableColumn& innerColumn() const { return inputColumns[0]; }
        const BaseType::TableColumn& outerColumn() const { return inputColumns[1]; }
    };

    class ResultNode : public IrBaseNode {
    public:
        std::string fileName;
        std::vector<BaseType::TableColumn> resultColumns; // Kept for metadata access
        std::vector<std::string> resultHeaders;

        ResultNode(
            const std::string& fileName,
            const std::vector<BaseType::TableColumn>& resultColumns,
            const BaseType::TableColumn& resultIdx,
            const std::vector<std::string>& resultHeaders):
                fileName(fileName),
                resultColumns(resultColumns),
                resultHeaders(resultHeaders) 
            {
                inputColumns.push_back(resultIdx);
                inputColumns.insert(inputColumns.end(), resultColumns.begin(), resultColumns.end());
            };
            
        const BaseType::TableColumn& resultIdx() const { return inputColumns[0]; }
    };

    // Column Store Specific Nodes

    class MaterializeNode : public IrBaseNode {
    public:
        MaterializeNode(const BaseType::TableColumn& idxColumn, const BaseType::TableColumn& filterColumn)
        {
            inputColumns.push_back(idxColumn);
            inputColumns.push_back(filterColumn);
        };

        const BaseType::TableColumn& idxColumn() const { return inputColumns[0]; }
        const BaseType::TableColumn& filterColumn() const { return inputColumns[1]; }
    };

    class PositionList : public IrBaseNode{
    public:
        std::vector<uint16_t> list;

        PositionList(const BaseType::TableColumn& refColumn, const std::vector<uint16_t>& list):
            list(list) 
        {
            inputColumns.push_back(refColumn);
        };

        const BaseType::TableColumn& refColumn() const { return inputColumns[0]; }
    };

    class Bitmap : public IrBaseNode {
    public:
        std::vector<bool> map;

        Bitmap(const BaseType::TableColumn& refColumn, const std::vector<bool>& map):
            map(map) 
        {
            inputColumns.push_back(refColumn);
        };

        const BaseType::TableColumn& refColumn() const { return inputColumns[0]; }
    };

};