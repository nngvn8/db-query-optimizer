#include "ir/plan_node.hpp"

class AbstractToIr {
public:
    static std::shared_ptr<PlanNode> abstractToIr(std::shared_ptr<PlanNode> node);
};
