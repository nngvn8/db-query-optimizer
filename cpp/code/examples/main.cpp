#include <iostream>
#include <string>
#include "parser/generate_AST.hpp"
#include "ir/ir_transformer.hpp"
#include "parser/generate_dot.hpp"
#include "translation/item_builder.hpp"
#include "util/sequentializer.hpp"
#include "ir/plan_node_to_dot.hpp"
#include "util/unique_col_names.hpp"
#include "ir_optimizations/ir_optimizations.hpp"


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
    generateDotFile(root,"ast.dot");


    shared_ptr<PlanNode> ir_root;
    std::vector<const PlanNode*> sequenced_plan;
    ItemBuilder itemBuilder;
    std::vector<WorkItem> workItems;

    // ############# STANDARD APPROACH ################################
    // Generate IR tree for second optimizer
    ir_root = astToIr(root);
    generatePlanDotFile(*ir_root, "ir_plan.dot", DotContentType::IR_DATA);
    
    ensureCorrectColumnSetup(ir_root);

    // Place semi joins
    placeSemiJoins(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_semi_j.dot", DotContentType::IR_DATA);

    mergeSortIntoGroupIfSubset(&ir_root);
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
    sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());
    // printSequencedPlan(sequenced_plan);

    // Create WorkItems
    workItems = itemBuilder.createWorkItems(sequenced_plan);

    // ##################### LATE MATERIALIZATION APPROACH #################33
    // Generate IR tree for second optimizer
    ir_root = astToIr(root);
    generatePlanDotFile(*ir_root, "ir_plan.dot", DotContentType::IR_DATA);
    
    ensureCorrectColumnSetup(ir_root);

    // Place semi joins
    placeSemiJoins(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_semi_j_l.dot", DotContentType::IR_DATA);
    
    mergeSortIntoGroupIfSubset(&ir_root);
    generatePlanDotFile(*ir_root, "ir_plan_remove_sort.dot", DotContentType::IR_DATA);

    // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_agg_opt_l.dot", DotContentType::IR_DATA);

    // Put Late Materialization
    putLateMaterialization(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_l.dot", DotContentType::IR_DATA);

    // GrandchildrenOptimization
    grandChildrenOptimization(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_l_gco.dot", DotContentType::IR_DATA);

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

    ensureCorrectColumnSetup(ir_root);

    // Place semi joins
    placeSemiJoins(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_semi_j_l2.dot", DotContentType::IR_DATA);
    
    mergeSortIntoGroupIfSubset(&ir_root);
    generatePlanDotFile(*ir_root, "ir_plan_remove_sort.dot", DotContentType::IR_DATA);

    // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_agg_opt_l2.dot", DotContentType::IR_DATA);

    // Put Late Materialization
    putLateMaterializationV2(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_l2.dot", DotContentType::IR_DATA);

    // GrandchildrenOptimization
    grandChildrenOptimization(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_l2_gco.dot", DotContentType::IR_DATA);

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

    ensureCorrectColumnSetup(ir_root);

    // Place semi joins
    placeSemiJoins(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_semi_j_lh.dot", DotContentType::IR_DATA);
    
    mergeSortIntoGroupIfSubset(&ir_root);
    generatePlanDotFile(*ir_root, "ir_plan_remove_sort.dot", DotContentType::IR_DATA);

    // Move single sum aggregations into group item
    moveAggIntoGroup(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_agg_opt_lh.dot", DotContentType::IR_DATA);

    // Put Late Materialization
    putLateMaterializationHybrid(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_lh.dot", DotContentType::IR_DATA);

    // GrandchildrenOptimization
    grandChildrenOptimization(ir_root.get());
    generatePlanDotFile(*ir_root, "ir_plan_mat_lh_gco.dot", DotContentType::IR_DATA);

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
