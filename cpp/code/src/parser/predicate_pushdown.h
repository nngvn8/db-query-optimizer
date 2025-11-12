#ifndef PREDICATE_PUSHDOWN_H
#define PREDICATE_PUSHDOWN_H

#include "generate_AST.h"
#include <iostream>

void extractPredicate(ASTNode*& root, std::queue<ASTNode*>& q);

void placePredicate(ASTNode*& root, std::queue<ASTNode*>& q);

ASTNode* predicatePushDown(ASTNode* root);

#endif