#include <iostream>
#include <string>
#include <parser/generate_AST.h>
#include <ir/ir_transformer.hpp>
#include <parser/generate_dot.h>
#include <translation/item_builder.h>
#include <sequentializer/sequentializer.hpp>

using namespace std;

int main() {
    const std::string query = R"SQL(
        SELECT d_year, s_city, p_brand, SUM(lo_revenue - lo_supplycost) AS PROFIT
            FROM dates, customer, supplier, part, lineorder
            WHERE
                lo_custkey = c_custkey
            AND lo_suppkey = s_suppkey
            AND lo_partkey = p_partkey
            AND lo_orderdate = d_datekey
            AND s_nation = 'UNITED STATES'
            AND (
                        d_year = 1997
                    OR d_year = 1998
                )
            AND p_category = 'MFGR#14'
            GROUP BY d_year, s_city, p_brand
            ORDER BY d_year, s_city, p_brand;
        )SQL";
    
    // Parse SQL and optimize
    auto root = generateASTNode(query);
    printAST(root);
    // generateDotFile(root,"testpic12.dot");
    // to generate PNG do this in command line : dot -Tpng testpic7.dot -o ast.png
    std::cout<<"\n\n";
    
    // Generate IR tree for second optimizer
    unique_ptr<PlanNode> ir_root = astToIr(root);
    printPlanTree(*ir_root, 2);
    std::cout<<"\n\n";

    // Fill Materializes
    fillMaterializes(ir_root.get());
    printPlanTree(*ir_root, 2);
    std::cout<<"\n\n";

    // Map to Api (Physical) Data
    irToApiData(ir_root.get());
    printPlanTree(*ir_root, 3);

    // Sequentialize 
    std::vector<const PlanNode*> sequenced_plan = to_sequence_children_list<PlanNode>(ir_root.get());
    printSequencedPlan(sequenced_plan);
    
    // Create WorkItems
    ItemBuilder itemBuilder;
    std::vector<WorkItem> workItems = itemBuilder.createWorkItems(sequenced_plan);

    return 0;
}
