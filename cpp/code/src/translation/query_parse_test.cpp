#pragma once

#include <translation/plan_node.hpp>
#include <translation/abstract_ir.hpp>
#include <translation/ir_transformer.hpp>
#include <translation/file_reader.hpp>

int main(int argc, char* argv[]) {
    std::vector<std::string> file_names = {"q1-1-plan.json", "q1-2-plan.json", "q1-3-plan.json", "q2-1-plan.json", "q2-2-plan.json", "q2-3-plan.json", "q3-1-plan.json", "q3-2-plan.json", "q3-3-plan.json", "q3-4-plan.json", "q4-1-plan.json", "q4-2-plan.json", "q4-3-plan.json"};
    std::string base_dir = "/home/martin/University/09_KDB/ws25-optimizer-rust/pb-plans/";
    
    for (const std::string& file_name : file_names) {
    
        Json::Value queryPlan = read_plan_to_json(base_dir, file_name);

        std::unique_ptr planNodeRoot = std::make_unique<PlanNode>(queryPlan);
        // printDebug(*planNodeRoot);
        // printPlanTree(*planNodeRoot, "", true);
        planNodeRoot = pruneTree(std::move(planNodeRoot));
        // printDebug(*planNodeRoot);
        printPlanTree(*planNodeRoot, "", true);
        // std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(planNodeRoot.get());
        // printSequencedPlan(sequenced_plan);
        // break;
    }
}