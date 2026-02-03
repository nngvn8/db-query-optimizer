#include "col_opt/col_opt.hpp"

#include "ir/ir_views.hpp"
#include "ir/catalog.hpp"


void placeSemiJoins(PlanNode* node, std::set<BaseType::Table> tablesNeededLater) {
    if (!node) return;

    // Tables this node needs (input and output columns are the same, doesn't matter which one we traverse)
    std::set<BaseType::Table> tablesThisNode;
    for (const BaseType::TableColumn& col : node->irData.inputColumns) {
        tablesThisNode.insert(col.table);
    }

    // If encountering join
    if (auto joinV = node->irData.get_view_if<JoinView>()) {
        bool innerTableNeededLater = tablesNeededLater.contains(joinV->inner().table);
        bool outerTableNeededLater = tablesNeededLater.contains(joinV->outer().table);

        // If only one table needed later
        if (!(innerTableNeededLater && outerTableNeededLater)) {

            const BaseType::TableColumn& colToDiscard = innerTableNeededLater ? joinV->outer() : joinV->inner();

            // If column is unique - Continue only if it is and  therefore only severs as filter and does not multiply any rows
            if (Catalog::isUnique(colToDiscard.table.name, colToDiscard.columnName)) {
                IrData semiJoinData = SemiJoinView::create(node->irData);
                SemiJoinView semiJoinV(semiJoinData);

                // Swap inner and outer, if inner is the one needed later
                if (innerTableNeededLater && !outerTableNeededLater) {
                    std::swap(semiJoinV.inner(), semiJoinV.outer());
                    std::swap(semiJoinV.innerOut(), semiJoinV.outerOut());
                }
                node->irData = semiJoinData;
            }
        }
    }

    // Add tables of this node to tables needed later
    tablesNeededLater.insert(tablesThisNode.begin(), tablesThisNode.end());


    // RECURSE
    for (const auto& child : node->children) {
        placeSemiJoins(child.get(), tablesNeededLater);
    }

}