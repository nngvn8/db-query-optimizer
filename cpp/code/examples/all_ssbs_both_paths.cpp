#include "ir/plan_node.hpp"
#include "ir/abstract_ir.hpp"
#include "util/file_reader.hpp"
#include "ir/get_query_info.hpp"
#include "ir/ir_transformer.hpp"
#include "util/sequentializer.hpp"
#include "ir_optimizations/ir_optimizations.hpp"
#include "util/unique_col_names.hpp"
#include "ir/plan_node_to_dot.hpp"
#include "parser/generate_AST.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <functional>

// Helper to get a fresh IR tree for a query
std::shared_ptr<PlanNode> getFreshIrTree(const std::string& base_dir, const std::string& json_file, const std::string& sql_file) {
    Json::Value queryPlan = read_plan_to_json(base_dir, json_file);
    std::string query = read_ssb_query(base_dir, sql_file);

    std::shared_ptr<PlanNode> planNodeRoot = std::make_shared<PlanNode>(queryPlan);
    planNodeRoot = pruneTree(planNodeRoot);
    SqlQueryData sqlQueryData = parseQuery(query);
    planNodeRoot = enrichTree(planNodeRoot, sqlQueryData);
    AbstractToIr::abstractToIr(planNodeRoot);
    ensureCorrectColumnSetup(planNodeRoot);
    
    return planNodeRoot;
}

// Helper to get a fresh IR tree for a query (AST Based)
std::shared_ptr<PlanNode> getFreshIrTreeFromAst(const std::string& base_dir, const std::string& sql_file) {
    std::string query = read_ssb_query(base_dir, sql_file);
    ASTNode* astRoot = generateASTNode(query);
    std::shared_ptr<PlanNode> planNodeRoot = astToIr(astRoot);
    ensureCorrectColumnSetup(planNodeRoot);
    delete astRoot;
    return planNodeRoot;
}

void runPipelines(std::function<std::shared_ptr<PlanNode>()> getTree, const std::string& prefix, const std::string& sql_file) {
    // 1. Standard Pipeline
    {
        auto root = getTree();
        
        placeSemiJoins(root.get());
        mergeSortIntoGroupIfSubset(&root);
        moveAggIntoGroup(root.get());
        fillMaterializes(root.get());
        uniqueColNames(root.get());
        irToApiData(root.get());
        
        std::string dot_name = prefix + "_standard_" + sql_file + ".dot";
        generatePlanDotFile(*root, dot_name, DotContentType::API_DATA);
    }

    // 2. Late Materialization V2
    {
        auto root = getTree();

        placeSemiJoins(root.get());
        mergeSortIntoGroupIfSubset(&root);
        moveAggIntoGroup(root.get());
        putLateMaterializationV2(root.get());
        grandChildrenOptimization(root.get());
        uniqueColNames(root.get());
        irToApiData(root.get());

        std::string dot_name = prefix + "_late_v2_" + sql_file + ".dot";
        generatePlanDotFile(*root, dot_name, DotContentType::API_DATA);
    }

    // 3. Late Materialization Hybrid
    {
        auto root = getTree();

        placeSemiJoins(root.get());
        mergeSortIntoGroupIfSubset(&root);
        moveAggIntoGroup(root.get());
        putLateMaterializationHybrid(root.get());
        grandChildrenOptimization(root.get());
        uniqueColNames(root.get());
        irToApiData(root.get());

        std::string dot_name = prefix + "_late_hybrid_" + sql_file + ".dot";
        generatePlanDotFile(*root, dot_name, DotContentType::API_DATA);
    }
}

int main() {
    std::string base_dir = "../../pb-plans/";
    std::vector<std::string> json_plan_files = {"q1-1-plan.json", "q1-2-plan.json", "q1-3-plan.json", "q2-1-plan.json", "q2-2-plan.json", "q2-3-plan.json", "q3-1-plan.json", "q3-2-plan.json", "q3-3-plan.json", "q3-4-plan.json", "q4-1-plan.json", "q4-2-plan.json", "q4-3-plan.json"};
    std::vector<std::string> ssb_queries = {"q1-1.sql", "q1-2.sql", "q1-3.sql", "q2-1.sql", "q2-2.sql", "q2-3.sql", "q3-1.sql", "q3-2.sql", "q3-3.sql", "q3-4.sql", "q4-1.sql", "q4-2.sql", "q4-3.sql"};

    for (size_t i = 0; i < json_plan_files.size(); ++i) {
        const auto& json_file = json_plan_files[i];
        const auto& sql_file = ssb_queries[i];
        std::cout << "Processing " << sql_file << "..." << std::endl;

        // Plan Based
        runPipelines([&](){ return getFreshIrTree(base_dir, json_file, sql_file); }, "plan", sql_file);

        // AST Based
        runPipelines([&](){ return getFreshIrTreeFromAst(base_dir, sql_file); }, "ast", sql_file);
    }

    return 0;
}