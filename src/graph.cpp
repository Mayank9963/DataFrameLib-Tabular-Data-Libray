#include "../include/dataframelib/graph.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>
namespace dataframelib {

    void print_plan(std::shared_ptr<Node> node, int depth) {
        print_plan(node, std::cout, depth);
    }

    void print_plan(std::shared_ptr<Node> node, std::ostream& os, int depth) {
        if (!node) return;

        for (int i = 0; i < depth; i++) os << "  ";
        os << node->op << "\n";

        for (auto& child : node->children) {
            print_plan(child, os, depth + 1);
        }

        if (node->right_child) {
            print_plan(node->right_child, os, depth + 1);
        }
    }

    // DFS to assign IDs helper function 
    void assign_ids_helper(std::shared_ptr<Node> node, std::unordered_map<Node*, int>& id_map, int& id) {

        if (!node || id_map.count(node.get())) {
            return;
        }

        id_map[node.get()] = id++;

        for (auto& child : node->children) {
            assign_ids_helper(child, id_map, id);
        }

        if (node->right_child) {
            assign_ids_helper(node->right_child, id_map, id);
        }
    }

    // export_to_dot function
    void export_to_dot(std::shared_ptr<Node> root, const std::string& filename) {
    std::ofstream file(filename);

    file << "digraph G {\n";

    std::unordered_map<Node*, int> id_map;
    int id = 0;
    assign_ids_helper(root, id_map, id);

    // print nodes
    for (auto& [ptr, node_id] : id_map) {
        file << "  node" << node_id << " [label=\"" << ptr->op << "\"];\n";
    }

    // print edges
    for (auto& [ptr, node_id] : id_map) {
        for (auto& child : ptr->children) {
            file << "  node" << id_map[child.get()] << " -> node" << node_id << ";\n";
        }
        if (ptr->right_child) {
            file << "  node" << id_map[ptr->right_child.get()] << " -> node" << node_id << ";\n";
        }
    }
    file << "}\n";
}
}