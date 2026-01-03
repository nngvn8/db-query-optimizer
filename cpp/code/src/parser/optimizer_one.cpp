#include "generate_AST.h"
#include "optimizer_one.h"
#include <iostream>
#include <vector>

void extractPredicate(ASTNode*& root, std::vector<ASTNode*>& predicate, std::vector<ASTNode*>& joinCond) {
    if (root == nullptr) return;
    
    if (auto e = std::get_if<WhereClauseNode>(&root->val)) {
        if ((*e).operatorType == "EQUAL" && (*e).table2 != "") {
            ASTNode* nodeCopy = new ASTNode(*e);
            joinCond.push_back(nodeCopy);
        }
        else {
            ASTNode* nodeCopy = new ASTNode(*e);
            predicate.push_back(nodeCopy);
        }
    
        ASTNode* leftChild = root->left;
        ASTNode* rightChild = root->right;

        root->left = nullptr;
        root->right = nullptr;
        delete root;

        root = leftChild;
        
        extractPredicate(root, predicate, joinCond);
        extractPredicate(rightChild, predicate, joinCond);
        return;
    }
    
    extractPredicate(root->left, predicate, joinCond);
    extractPredicate(root->right, predicate, joinCond);
}


ASTNode* buildJoin(std::vector<ASTNode*> joinCond) {
    if (joinCond.empty()) return nullptr;
    
    if (joinCond.size() == 1) {
        auto e = std::get_if<WhereClauseNode>(&joinCond[0]->val);
        ASTNode* root = new ASTNode(TableJoinNode("JOININNER", (*e).table, (*e).column, (*e).table2, (*e).column2));
        root->left = new ASTNode(TableBaseNode((*e).table));
        root->right = new ASTNode(TableBaseNode((*e).table2));
        return root;
    }
    
    auto e = std::get_if<WhereClauseNode>(&joinCond[0]->val);
    ASTNode* root = new ASTNode(TableJoinNode("JOININNER", (*e).table, (*e).column, (*e).table2, (*e).column2));
    root->right = new ASTNode(TableBaseNode((*e).table2));
    root->left = new ASTNode(TableBaseNode((*e).table));
    
    ASTNode* curr = root;
    
    for (int i = 1; i < joinCond.size(); i++) {
        auto e = std::get_if<WhereClauseNode>(&joinCond[i]->val);
        ASTNode* newJoin = new ASTNode(TableJoinNode("JOININNER", (*e).table, (*e).column, (*e).table2, (*e).column2));
        
        newJoin->left = curr;
        newJoin->right = new ASTNode(TableBaseNode((*e).table2));
        
        curr = newJoin;
    }
    
    return curr;
}

void optimizeCrossProduct(ASTNode*& root, ASTNode* joinroot) {
    if (auto e = std::get_if<TableJoinNode>(&root->val)) {
        delete root;
        root = joinroot;
        return;
    }
    
    optimizeCrossProduct(root->left, joinroot);
}

void placePredicate(ASTNode*& root, ASTNode*& predicate){

    if (root == nullptr) return;
    if (auto t = std::get_if<TableBaseNode>(&root->left->val)){
        auto pred = std::get_if<WhereClauseNode>(&predicate->val);
        if((*pred).table == (*t).tableName || (*pred).table == (*t).tableAlias){
            ASTNode* oldleft = root->left; 
            predicate->left = oldleft;   
            root->left = predicate; 
            return; 
        }       
    }

    if (auto t = std::get_if<TableBaseNode>(&root->right->val)){
        auto pred = std::get_if<WhereClauseNode>(&predicate->val);
        if((*pred).table == (*t).tableName || (*pred).table == (*t).tableAlias){
            ASTNode* oldRight = root->right; 
            predicate->right = oldRight;   
            root->right = predicate; 
            return;
        }      
    }

    placePredicate(root->left, predicate);
    placePredicate(root->right, predicate);
}



ASTNode* optimizerOne(ASTNode* root){
    std::vector<ASTNode*> predicate;
    std::vector<ASTNode*> joinCond;
    
    extractPredicate(root,predicate,joinCond);

    ASTNode* joinroot = buildJoin(joinCond);
    optimizeCrossProduct(root,joinroot);


    for(int i=0;i<predicate.size();i++){
        placePredicate(root,predicate[i]);
    }

    return root;
}