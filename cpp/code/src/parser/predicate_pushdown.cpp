#include "generate_AST.h"
#include "predicate_pushdown.h"
#include <iostream>
#include <queue>

// STEP 1: take out the predicates of the form where a.id = 12, where it can be pushed down to the level of the table
// STEP 2: we make these type of tables

void extractPredicate(ASTNode*& root, std::queue<ASTNode*>& q) {
    if (root == nullptr) return;
    if (auto e = std::get_if<ExprNode>(&root->val)) {
        if ((*e).exprType == ExprWhere && !(*e).value.empty()) {
            ASTNode* nodeCopy = new ASTNode(*e);
            q.push(nodeCopy);
            ASTNode* temp = root->left;
            delete root;
            root = temp;
            extractPredicate(root, q);
        }
        else{
            extractPredicate(root->left, q);
        }
    }
    
    extractPredicate(root->right, q);
}

void placePredicate(ASTNode*& root, ASTNode*& predicate){
    if (root == nullptr) return;
    if (auto t = std::get_if<TableNode>(&root->left->val)){
        if((*t).tableRefType == TableName){
            auto pred = std::get_if<ExprNode>(&predicate->val);
            if((*pred).table == (*t).tableName || (*pred).table == (*t).tableAlias){
                ASTNode* oldRight = root->left; 
                predicate->left = oldRight;   
                root->left = predicate; 
                return; 
            }
        }        
    }

    if (auto t = std::get_if<TableNode>(&root->right->val)){
        if((*t).tableRefType == TableName){
            auto pred = std::get_if<ExprNode>(&predicate->val);
            if((*pred).table == (*t).tableName || (*pred).table == (*t).tableAlias){
                ASTNode* oldRight = root->right; 
                predicate->right = oldRight;   
                root->right = predicate; 
                return;
            }
        }        
    }

    placePredicate(root->left, predicate);
    placePredicate(root->right, predicate);
}

ASTNode* predicatePushDown(ASTNode* root){
    std::queue<ASTNode*> q;
    extractPredicate(root,q);
    while(!q.empty()){
        placePredicate(root,q.front());
        q.pop();
    }

    return root;
}



