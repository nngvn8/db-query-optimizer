#include <translation/plan_node.hpp>
#include <translation/abstract_ir.hpp>
#include <translation/ir_transformer.hpp>
#include <translation/file_reader.hpp>
#include <translation/parse_query.hpp>

int main(int argc, char* argv[]) {
    std::string base_dir = "/home/martin/University/09_KDB/ws25-optimizer-rust/pb-plans/";
    std::vector<std::string> json_plan_files = {"q1-1-plan.json", "q1-2-plan.json", "q1-3-plan.json", "q2-1-plan.json", "q2-2-plan.json", "q2-3-plan.json", "q3-1-plan.json", "q3-2-plan.json", "q3-3-plan.json", "q3-4-plan.json", "q4-1-plan.json", "q4-2-plan.json", "q4-3-plan.json"};
    std::vector<std::string> ssb_queries = {"q1-1.sql", "q1-2.sql", "q1-3.sql", "q2-1.sql", "q2-2.sql", "q2-3.sql", "q3-1.sql", "q3-2.sql", "q3-3.sql", "q3-4.sql", "q4-1.sql", "q4-2.sql", "q4-3.sql"};
    
    for (size_t i = 0; i < json_plan_files.size(); ++i) {
        const auto& json_plan_file = json_plan_files[i];
        const auto& query_file = ssb_queries[i];
    
        Json::Value queryPlan = read_plan_to_json(base_dir, json_plan_file);
        std::string query = read_ssb_query(base_dir, query_file);


        std::unique_ptr planNodeRoot = std::make_unique<PlanNode>(queryPlan);
        // printDebug(*planNodeRoot);
        // printPlanTree(*planNodeRoot, "", true);
        planNodeRoot = pruneTree(std::move(planNodeRoot));
        SqlQueryData sqlQueryData = parseQuery(query);
        print_query_data(sqlQueryData);
        planNodeRoot = enrichTree(std::move(planNodeRoot), sqlQueryData);
        // printDebug(*planNodeRoot);
        printPlanTree(*planNodeRoot, "", true);
        // std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(planNodeRoot.get());
        // printSequencedPlan(sequenced_plan);
        // break;
    }
}