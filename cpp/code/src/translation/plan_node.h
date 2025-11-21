#include <iostream>
#include <vector>
#include <jsoncpp/json/value.h>

struct BaseTable {
    std::string fullName;
    std::string alias;
    bool isVirtual;
    std::string schema;
};

struct PlanParams {
    std::unique_ptr<BaseTable> baseTable;
    std::string filterPredicate;
    std::unique_ptr<std::vector<std::string>> sortKeys;
    int parallelWorkers;
    std::string index;
    std::string lookupKey;
    std::string suplanName;
};

struct Estimates {
    float cardinality;
    float cost;
};

struct Measures {
    float cardinality;
    float executionTime;
    int cacheHits;
    int cacheMisses;
};

class PlanNode {
    public:
        std::string nodeType;
        std::unique_ptr<std::string> nodeOperator;
        std::unique_ptr<std::string> physNodeOperator; // TODO: implement mapping

        std::vector<std::unique_ptr<PlanNode>> children;

        PlanParams planParams;
        Estimates estimates;
        Measures measures;

        std::unique_ptr<std::string> subPlan;

        std::unique_ptr<PlanNode> next;

        PlanNode() {};
        PlanNode(const Json::Value& queryPlan);

        ~PlanNode() {};

        void setBaseTable(std::unique_ptr<BaseTable>& node, const Json::Value& jsonData);
        void setEstimates(Estimates& planNode, const Json::Value& jsonData);
        void setMeasures(Measures& measures, const Json::Value& jsonData);
        void setPlanParams(PlanParams& node, const Json::Value& jsonData);
};
