#include <iostream>
#include <string>
#include "parser/generate_AST.hpp"
#include "ir/ir_transformer.hpp"
#include "parser/generate_dot.hpp"
#include "translation/item_builder.hpp"
#include "util/sequentializer.hpp"
#include "ir/plan_node_to_dot.hpp"
#include "util/unique_col_names.hpp"
#include "col_opt/col_opt.hpp"


using namespace std;

int main() {
    const std::string query = R"SQL(
            SELECT d_year, s_nation, p_category, SUM(lo_revenue - lo_supplycost) AS PROFIT
            FROM dates, customer, supplier, part, lineorder
            WHERE lo_custkey = c_custkey
            AND lo_suppkey = s_suppkey
            AND lo_partkey = p_partkey
            AND lo_orderdate = d_datekey
            AND c_region = 'AMERICA'
            AND s_region = 'AMERICA'
            AND (d_year = 1997 OR d_year = 1998)
            AND (p_mfgr = 'MFGR#1' OR p_mfgr = 'MFGR#2')
            GROUP BY d_year, s_nation, p_category
            ORDER BY d_year, s_nation, p_category;
        )SQL";

    // Generate png files for *.dot files using cli: dot -Tpng <file_name>.dot -o <name_for_img>.png

    // Parse SQL and optimize
    auto root = generateASTNode(query);
    // printAST(root);
    generateDotFile(root,"testpic12.dot");
    std::cout<<"\n\n";

    // Generate IR tree for second optimizer
    shared_ptr<PlanNode> ir_root = astToIr(root);
    generatePlanDotFile(*ir_root, "ir_plan.dot", DotContentType::IR_DATA);
    // printPlanTree(*ir_root, 2);
    std::cout<<"\n\n";

    // Place semi joins
    std::set<BaseType::Table> tablesNeededLater;
    placeSemiJoins(ir_root.get(), tablesNeededLater);
    // generatePlanDotFile(*ir_root, "ir_plan_semi_j.dot", DotContentType::IR_DATA);

    // Put Late Materialization
    putLateMaterialization(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_late.dot", DotContentType::IR_DATA);

    // Fill Materializes
    // fillMaterializes(ir_root.get());
    // generatePlanDotFile(*ir_root, "ir_plan_mat.dot", DotContentType::IR_DATA);
    // printPlanTree(*ir_root, 2);
    // std::cout<<"\n\n";

    // // Rename columns
    uniqueColNames(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_num.dot", DotContentType::IR_DATA);
    // printPlanTree(*ir_root, 2);
    // // std::cout<<"\n\n";

    // // Map to Api (Physical) Data
    irToApiData(ir_root.get());
    generatePlanDotFile(*ir_root, "api_plan.dot", DotContentType::API_DATA);
    // printPlanTree(*ir_root, 3);

    // // Sequentialize
    std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());
    printSequencedPlan(sequenced_plan);

    // // Create WorkItems
    ItemBuilder itemBuilder;
    std::vector<WorkItem> workItems = itemBuilder.createWorkItems(sequenced_plan);

    return 0;
}
