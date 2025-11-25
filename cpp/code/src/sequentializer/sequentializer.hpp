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
std::vector<const T*>& sequentialize_children_list(const T* cur_node, std::set<const T*>& visited, std::vector<const T*>& post_order_list) {
    const auto& children = cur_node->children;
    visited.insert(cur_node);
    
    // Recursion for all children
    for (const auto& child_wrapper : children) { // handles: T* and shared_ptr<T> and unique_ptr<T>
        T* child_ptr = get_raw_ptr(child_wrapper);
        bool child_visited = visited.find(child_ptr) != visited.cend();
        if (!child_visited) {
            sequentialize_children_list(child_ptr, visited, post_order_list);
        }
    }
    
    // Add parent after children to list
    post_order_list.push_back(cur_node);
    return post_order_list;
};