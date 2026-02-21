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
            SELECT c_nation, s_nation, d_year, SUM(lo_revenue) AS REVENUE
            FROM customer, lineorder, supplier, dates
            WHERE lo_custkey = c_custkey
            AND lo_suppkey = s_suppkey
            AND lo_orderdate = d_datekey
            AND c_region = 'ASIA'
            AND s_region = 'ASIA'
            AND d_year >= 1992
            AND d_year <= 1997
            GROUP BY c_nation, s_nation, d_year
            ORDER BY d_year ASC, REVENUE DESC;
        )SQL";

    // Generate png files for *.dot files using cli: dot -Tpng <file_name>.dot -o <name_for_img>.png

    // Parse SQL and optimize
    auto root = generateASTNode(query);
    generateDotFile(root,"ast.dot");


    // ############# STANDARD APPROACH ################################
    // Generate IR tree for second optimizer
    shared_ptr<PlanNode> ir_root = astToIr(root);
    generatePlanDotFile(*ir_root, "ir_plan.dot", DotContentType::IR_DATA);

    // Place semi joins
    std::set<BaseType::Table> tablesNeededLater;
    placeSemiJoins(ir_root.get(), tablesNeededLater);
    generatePlanDotFile(*ir_root, "ir_plan_semi_j.dot", DotContentType::IR_DATA);

    removeSortIfSubsetGroup(&ir_root);
    generatePlanDotFile(*ir_root, "ir_plan_remove_sort.dot", DotContentType::IR_DATA);

    // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_agg_opt.dot", DotContentType::IR_DATA);

    // Fill Materializes
    fillMaterializes(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat.dot", DotContentType::IR_DATA);

    // Rename columns
    uniqueColNames(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_num.dot", DotContentType::IR_DATA);

    // Map to Api (Physical) Data
    irToApiData(ir_root.get());
    generatePlanDotFile(*ir_root, "api_plan.dot", DotContentType::API_DATA);

    // Sequentialize
    std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());
    // printSequencedPlan(sequenced_plan);

    // Create WorkItems
    ItemBuilder itemBuilder;
    std::vector<WorkItem> workItems = itemBuilder.createWorkItems(sequenced_plan);

    // ##################### LATE MATERIALIZATION APPROACH #################33
    // Generate IR tree for second optimizer
    ir_root = astToIr(root);
    generatePlanDotFile(*ir_root, "ir_plan.dot", DotContentType::IR_DATA);

    // Place semi joins
    placeSemiJoins(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_semi_j_l.dot", DotContentType::IR_DATA);

    removeSortIfSubsetGroup(&ir_root);
    generatePlanDotFile(*ir_root, "ir_plan_remove_sort.dot", DotContentType::IR_DATA);

    // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_agg_opt_l.dot", DotContentType::IR_DATA);

    // Put Late Materialization
    putLateMaterialization(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_l.dot", DotContentType::IR_DATA);

    // Rename columns
    uniqueColNames(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_num_l.dot", DotContentType::IR_DATA);

    // Map to Api (Physical) Data
    irToApiData(ir_root.get());
    generatePlanDotFile(*ir_root, "api_plan_l.dot", DotContentType::API_DATA);

    // Sequentialize
    sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());
    // printSequencedPlan(sequenced_plan);

    // Create WorkItems
    workItems = itemBuilder.createWorkItems(sequenced_plan);

    // ##################### LATE MATERIALIZATION V2 APPROACH #################33
    // Generate IR tree for second optimizer
    ir_root = astToIr(root);
    generatePlanDotFile(*ir_root, "ir_plan.dot", DotContentType::IR_DATA);

    // Place semi joins
    placeSemiJoins(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_semi_j_l2.dot", DotContentType::IR_DATA);
    
    removeSortIfSubsetGroup(&ir_root);
    generatePlanDotFile(*ir_root, "ir_plan_remove_sort.dot", DotContentType::IR_DATA);

    // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_agg_opt_l2.dot", DotContentType::IR_DATA);

    // Put Late Materialization
    putLateMaterialization(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_l2.dot", DotContentType::IR_DATA);

    // Rename columns
    uniqueColNames(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_num_l2.dot", DotContentType::IR_DATA);

    // Map to Api (Physical) Data
    irToApiData(ir_root.get());
    generatePlanDotFile(*ir_root, "api_plan_l2.dot", DotContentType::API_DATA);

    // Sequentialize
    sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());
    // printSequencedPlan(sequenced_plan);

    // Create WorkItems
    workItems = itemBuilder.createWorkItems(sequenced_plan);

    // ##################### LATE MATERIALIZATION HYBRID APPROACH #################33
    // Generate IR tree for second optimizer
    ir_root = astToIr(root);
    generatePlanDotFile(*ir_root, "ir_plan.dot", DotContentType::IR_DATA);

    // Place semi joins
    placeSemiJoins(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_semi_j_lh.dot", DotContentType::IR_DATA);
    
    removeSortIfSubsetGroup(&ir_root);
    generatePlanDotFile(*ir_root, "ir_plan_remove_sort.dot", DotContentType::IR_DATA);

    // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_agg_opt_lh.dot", DotContentType::IR_DATA);

    // Put Late Materialization
    putLateMaterializationV2(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_lh.dot", DotContentType::IR_DATA);

    // Rename columns
    uniqueColNames(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_num_lh.dot", DotContentType::IR_DATA);

    // Map to Api (Physical) Data
    irToApiData(ir_root.get());
    generatePlanDotFile(*ir_root, "api_plan_lh.dot", DotContentType::API_DATA);

    // Sequentialize
    sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());
    // printSequencedPlan(sequenced_plan);

    // Create WorkItems
    workItems = itemBuilder.createWorkItems(sequenced_plan);

    return 0;
}
