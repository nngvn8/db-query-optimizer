#include <vector>
#include <set>

#include <memory>

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


template <typename T>
std::vector<const T*> to_sequence_children_list(const T* cur_node) {
    std::set<const T*> visited;
    std::vector<const T*> post_order_list;
    return to_sequence_children_list_sub(cur_node, visited, post_order_list);
}

template <typename T>
std::vector<const T*>& to_sequence_two_children_sub(const T* cur_node, std::set<const T*>& visited, std::vector<const T*>& post_order_list) {
    const auto& left_child = cur_node->left;
    const auto& right_child = cur_node->right;

    visited.insert(cur_node);

    const T* child_ptr;
    bool child_visited;

    // visit left child
    child_ptr = get_raw_ptr(left_child);
    child_visited = visited.find(child_ptr) != visited.cend();
    if (!child_visited) {
        to_sequence_two_children_sub(child_ptr, visited, post_order_list);
    }

    // visit right child
    child_ptr = get_raw_ptr(right_child);
    child_visited = visited.find(child_ptr) != visited.cend();
    if (!child_visited) {
        to_sequence_two_children_sub(child_ptr, visited, post_order_list);
    }
    
    // Add parent after children to list
    post_order_list.push_back(cur_node);
    return post_order_list;
};


template <typename T>
std::vector<const T*> to_sequence_two_children(const T* cur_node) {
    std::set<const T*> visited;
    std::vector<const T*> post_order_list;
    return to_sequence_two_children_sub(cur_node, visited, post_order_list);
}