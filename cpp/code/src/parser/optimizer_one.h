#ifndef PREDICATE_PUSHDOWN_H
#define PREDICATE_PUSHDOWN_H

#include "generate_AST.h"
#include <iostream>
#include <vector>

void extractPredicate(ASTNode*& root, std::vector<ASTNode*>& q, std::vector<ASTNode*>& joinCond);

ASTNode* buildJoin(std::vector<ASTNode*> joinCond);

void optimizeCrossProduct(ASTNode*& root, ASTNode* joinroot);

void placePredicate(ASTNode*& root, std::queue<ASTNode*>& q);

ASTNode* optimizerOne(ASTNode* root);

#endif