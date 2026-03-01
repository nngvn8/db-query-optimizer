/**
 * @file plan_node_to_dot.hpp
 * @brief Declares the function for generating a DOT file representation of a query plan.
 *
 * This file provides the interface for visualizing a query plan tree by converting it
 * into the DOT graph description language. The resulting DOT file can then be used
 * by tools like Graphviz to generate a visual representation of the plan.
 */

#pragma once

#include <string>

#include "ir/plan_node.hpp"

/**
 * @enum DotContentType
 * @brief Specifies the type of content to be visualized in the DOT file.
 */
enum class DotContentType {
    /// Visualize the main optimization IR data.
    IR_DATA,
    /// Visualize the final API data.
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
