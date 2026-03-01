/**
 * @file optimizer_one.hpp
 * @brief Declares functions for a first-pass query optimization, focusing on predicate pushdown.
 *
 * This file provides the interface for an initial optimization phase that restructures
 * the AST. The primary goal is to push down predicates (filters) as close as possible
 * to the data sources and to transform cross products into explicit joins.
 */

#ifndef PREDICATE_PUSHDOWN_H
#define PREDICATE_PUSHDOWN_H

#include "generate_AST.hpp"
#include <iostream>
#include <vector>

/**
 * @brief Extracts predicates and join conditions from the AST.
 * @param root The root of the AST.
 * @param q A vector to store the extracted filter predicates.
 * @param joinCond A vector to store the extracted join conditions.
 */
void extractPredicate(ASTNode*& root, std::vector<ASTNode*>& q, std::vector<ASTNode*>& joinCond);

/**
 * @brief Builds a left-deep join tree from a list of join conditions.
 * @param joinCond A vector of join condition nodes.
 * @return The root of the newly constructed join tree.
 */
ASTNode* buildJoin(std::vector<ASTNode*> joinCond);

/**
 * @brief Optimizes cross products by replacing them with explicit joins.
 * @param root The root of the AST to be optimized.
 * @param joinroot The root of the join tree to be inserted.
 */
void optimizeCrossProduct(ASTNode*& root, ASTNode* joinroot);

/**
 * @brief Places filter predicates into the AST at the appropriate locations.
 * @param root The root of the AST.
 * @param q A queue of filter predicate nodes to be placed.
 */
void placePredicate(ASTNode*& root, std::queue<ASTNode*>& q);

/**
 * @brief Runs the first optimization pass on the AST.
 * @param root The root of the AST.
 * @return The root of the optimized AST.
 */
ASTNode* optimizerOne(ASTNode* root);

#endif
