#include <iostream>
#include <string>
#include "generate_AST.h"
#include "predicate_pushdown.h"
#include <ir/ir_transformer.hpp>
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

    auto root = generateASTNode(query);
    // printAST(root);
    unique_ptr<PlanNode> ir_root = astToIr(root);
    printPlanTree(*ir_root, "", true);

    std::cout<<"\n\n";
}
