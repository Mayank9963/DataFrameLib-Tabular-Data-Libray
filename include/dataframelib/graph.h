#ifndef GRAPH_H
#define GRAPH_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cstdint>
#include "dataframelib/expression.h"

namespace dataframelib {
    // class for making plan of operation
    class Node {
    public:
        std::string op;

        std::vector<std::shared_ptr<Node>> children;
        std::shared_ptr<Node> right_child;
        std::shared_ptr<Expression> expr;
        std::vector<std::string> columns;
        std::string file_path;
        std::map<std::string, std::shared_ptr<Expression>> agg_map;

        std::string join_type;
        bool ascending = true;
        int head_n = 0;

        Node(const std::string& op_) : op(op_) {}
    };
    // function to print plan
    void print_plan(std::shared_ptr<Node> node, int depth = 0);
    void print_plan(std::shared_ptr<Node> node, std::ostream& os, int depth = 0);
    // function to export plan to dot file 
    void export_to_dot(std::shared_ptr<Node> root, const std::string& filename);
}
#endif
