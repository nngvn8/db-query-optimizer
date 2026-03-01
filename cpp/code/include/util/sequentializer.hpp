//
// Created by Blum Thomas on 2020-06-24.
//

#ifndef SEQUENTIALIZER_HPP
#define SEQUENTIALIZER_HPP

#include <vector>
#include <set>

#include <memory>

/**
 * @file sequentializer.hpp
 * @brief This file contains utility functions for traversing tree-like data structures and converting them into a sequential, post-order list of nodes.
 *
 * The provided templates support different types of node relationships, such as parent-child lists and binary tree structures (left/right children).
 * These functions are designed to handle raw pointers, `std::unique_ptr`, and `std::shared_ptr` seamlessly, ensuring flexibility and safety.
 * The primary goal is to flatten a hierarchical structure into a linear sequence, which is useful for various processing and optimization tasks.
 */

// Overload 1: If it's already a raw pointer, just return it.
template <typename T>
const T* get_raw_ptr(const T* ptr) {
    return ptr;
}

// Overload 2: If it's a unique_ptr, call .get() to extract the raw pointer.
template <typename T>
const T* get_raw_ptr(const std::unique_ptr<T>& ptr) {
    return ptr.get();
}

template <typename T>
const T* get_raw_ptr(const std::shared_ptr<T>& ptr) {
    return ptr.get();
}

/**
 * @brief Recursively traverses a tree structure with a list of children and builds a post-order sequence.
 * @tparam T The type of the node.
 * @param cur_node The current node being visited.
 * @param visited A set of already visited nodes to avoid cycles.
 * @param post_order_list The list to which the nodes are added in post-order.
 * @return A reference to the post-order list of nodes.
 */
template <typename T>
std::vector<const T*>& to_sequence_children_list_sub(const T* cur_node, std::set<const T*>& visited, std::vector<const T*>& post_order_list) {
    const auto& children = cur_node->children;
    visited.insert(cur_node);

    // Recursion for all children
    for (const auto& child_wrapper : children) { // handles: T* and shared_ptr<T> and unique_ptr<T>
        const T* child_ptr = get_raw_ptr(child_wrapper);
        bool child_visited = visited.find(child_ptr) != visited.cend();
        if (!child_visited) {
            to_sequence_children_list_sub(child_ptr, visited, post_order_list);
        }
    }

    // Add parent after children to list
    post_order_list.push_back(cur_node);
    return post_order_list;
};

/**
 * @brief Converts a tree structure with a list of children into a post-order sequence of nodes.
 * @tparam T The type of the node.
 * @param cur_node The root node of the tree to be traversed.
 * @return A vector containing the nodes in post-order.
 */
template <typename T>
std::vector<const T*> to_sequence_children_list(const T* cur_node) {
    std::set<const T*> visited;
    std::vector<const T*> post_order_list;
    return to_sequence_children_list_sub(cur_node, visited, post_order_list);
}

/**
 * @brief Recursively traverses a binary tree structure and builds a post-order sequence.
 * @tparam T The type of the node.
 * @param cur_node The current node being visited.
 * @param visited A set of already visited nodes to avoid cycles.
 * @param post_order_list The list to which the nodes are added in post-order.
 * @return A reference to the post-order list of nodes.
 */
template <typename T>
std::vector<const T*>& to_sequence_two_children_sub(const T* cur_node, std::set<const T*>& visited, std::vector<const T*>& post_order_list) {
    const auto& left_child = cur_node->left;
    const auto& right_child = cur_node->right;

    visited.insert(cur_node);

    const T* child_ptr;
    bool child_visited;

    // visit left child
    child_ptr = get_raw_ptr(left_child);
    if (child_ptr) {
        child_visited = visited.find(child_ptr) != visited.cend();
        if (!child_visited) {
            to_sequence_two_children_sub(child_ptr, visited, post_order_list);
        }
    }

    // visit right child
    child_ptr = get_raw_ptr(right_child);
    if (child_ptr) {
        child_visited = visited.find(child_ptr) != visited.cend();
        if (!child_visited) {
            to_sequence_two_children_sub(child_ptr, visited, post_order_list);
        }
    }

    // Add parent after children to list
    post_order_list.push_back(cur_node);
    return post_order_list;
};

/**
 * @brief Converts a binary tree structure into a post-order sequence of nodes.
 * @tparam T The type of the node.
 * @param cur_node The root node of the tree to be traversed.
 * @return A vector containing the nodes in post-order.
 */
template <typename T>
std::vector<const T*> to_sequence_two_children(const T* cur_node) {
    std::set<const T*> visited;
    std::vector<const T*> post_order_list;
    if (cur_node) {
        to_sequence_two_children_sub(cur_node, visited, post_order_list);
    }
    return post_order_list;
}

#endif //SEQUENTIALIZER_HPP