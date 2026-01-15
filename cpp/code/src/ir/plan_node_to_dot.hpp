#pragma once

#include <string>
#include <ir/plan_node.hpp>

enum class DotContentType {
    IR_DATA,
    API_DATA
};

/**
 * @brief Generates a DOT file from the PlanNode tree.
 * 
 * @param root The root of the PlanNode tree.
 * @param filename The output filename for the DOT file.
 * @param contentType The type of data to visualize (IR or API).
 */
void generatePlanDotFile(const PlanNode& root, const std::string& filename, DotContentType contentType = DotContentType::IR_DATA);
