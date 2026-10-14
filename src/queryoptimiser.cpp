
#include "../include/dataframelib/graph.h"
#include "../include/dataframelib/expression.h"
#include <unordered_set>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <parquet/arrow/reader.h>
#include <arrow/io/file.h>
#include <iostream>
#include <cstdint>
namespace dataframelib {
    // helper functions to which colos are there in the original csv
    std::vector<std::string> check_parent_columns(std::shared_ptr<Node> node) {
        if (!node) {
            return {};
        }
        if (node->op == "scan_csv") {
            std::ifstream file(node->file_path);
            std::string line;
            if (std::getline(file, line)) {
                std::vector<std::string> cols;
                std::stringstream ss(line);
                std::string col;
                while (std::getline(ss, col, ',')) cols.push_back(col);
                return cols;
            } 
        } 

        if (node->op == "scan_parquet") {
            std::shared_ptr<arrow::io::ReadableFile> infile;
            auto result = arrow::io::ReadableFile::Open(node->file_path);
            if (!result.ok()) {
                std::cerr << "Error opening file: " << node->file_path << std::endl;
                return {};
            }

            infile = *result;

            parquet::arrow::FileReaderBuilder builder;
            (void)builder.Open(infile);

            std::unique_ptr<parquet::arrow::FileReader> reader;
            (void)builder.Build(&reader);

            std::shared_ptr<arrow::Schema> schema;
            (void)reader->GetSchema(&schema);

            std::vector<std::string> cols;
            for (const auto& field : schema->fields()) {
                cols.push_back(field->name());
            }
            return cols;
        }

        if (!node->children.empty()) {
            return check_parent_columns(node->children[0]);
        }
        return {};
    }

    // check if parent_cols contains all elements of child_cols
    bool contains_all(const std::vector<std::string>& parent_cols, const std::vector<std::string>& child_cols) {
        std::unordered_map<std::string, bool> parent_map;
        for (const auto& col : parent_cols) {
            parent_map[col] = true;
        }
        for (const auto& col : child_cols) {
            if (parent_map.find(col) == parent_map.end()) {
                return false;
            }
        }
        return true;
    }

    // extract columns which are inside the given expression 
    std::vector<std::string> extract_columns(std::shared_ptr<Expression> expr) {
        std::vector<std::string> result;
        if (!expr) {
            return result;
        }

        if (auto c = std::dynamic_pointer_cast<ColumnExpression>(expr)) {
            result.push_back(c->getColumnName());
        }

        else if (auto c = std::dynamic_pointer_cast<BinaryExpression>(expr)) {
            auto l = extract_columns(c->left());
            auto r = extract_columns(c->right());
            result.insert(result.end(), l.begin(), l.end());
            result.insert(result.end(), r.begin(), r.end());
        }

        else if (auto c = std::dynamic_pointer_cast<AliasExpression>(expr)) {
            auto child_cols = extract_columns(c->getChild());
            result.insert(result.end(), child_cols.begin(), child_cols.end());
        }

        if (auto c = std::dynamic_pointer_cast<UnaryExpression>(expr)) {
            auto child_cols = extract_columns(c->getChild());
            result.insert(result.end(), child_cols.begin(), child_cols.end());
        }

        else if (auto c = std::dynamic_pointer_cast<FunctionExpression>(expr)) {
            auto child_cols = extract_columns(c->getChild());
            result.insert(result.end(), child_cols.begin(), child_cols.end());
        }
        
        else if (auto c = std::dynamic_pointer_cast<AggregationExpression>(expr)) {
            auto child_cols = extract_columns(c->getChild());
            result.insert(result.end(), child_cols.begin(), child_cols.end());
        }
        // literal expression does not have any column in them 
        return result;
    }

    // get value of literal expression
    static double get_lit_value(const std::shared_ptr<LiteralExpression>& l) {
        const auto& val = l->getValue();

        if (std::holds_alternative<int64_t>(val)){
            return static_cast<double>(std::get<int64_t>(val));
        }

        if (std::holds_alternative<double>(val)){
            return std::get<double>(val);
        }
        // if value is not int64_t or double then throw error 
        throw std::runtime_error("Literal is not numeric");
    }

    // simplify expression 
    std::shared_ptr<Expression> simplify(std::shared_ptr<Expression> expr) {
        if (!expr) {
            return expr;
        }

        auto binexpr = std::dynamic_pointer_cast<BinaryExpression>(expr);
        if (!binexpr) {
            return expr;
        }

        auto left = simplify(binexpr->left());
        auto right = simplify(binexpr->right());
        std::string op = binexpr->op();

        auto l_lit = std::dynamic_pointer_cast<LiteralExpression>(left);
        auto r_lit = std::dynamic_pointer_cast<LiteralExpression>(right);

        if (l_lit && r_lit) {
            auto l_val = l_lit->getValue();
            auto r_val = r_lit->getValue();

            if ((std::holds_alternative<int64_t>(l_val) || std::holds_alternative<double>(l_val)) &&
                (std::holds_alternative<int64_t>(r_val) || std::holds_alternative<double>(r_val))) {

                double lv = std::holds_alternative<int64_t>(l_val) ? std::get<int64_t>(l_val) : std::get<double>(l_val);
                double rv = std::holds_alternative<int64_t>(r_val) ? std::get<int64_t>(r_val) : std::get<double>(r_val);

                if (op == "+") {
                    return lit(lv + rv);
                }
                if (op == "-") {
                    return lit(lv - rv);
                }
                if (op == "*") {
                    return lit(lv * rv);
                }
                if (op == "/" && rv != 0) {
                    return lit(lv / rv);
                }

                if (op == ">"){
                    return lit(lv > rv);
                }
                if (op == "<") {
                    return lit(lv < rv);
                }
                if (op == "==") {
                    return lit(lv == rv);
                }
                if (op == "!=") {
                    return lit(lv != rv);
                }
                if (op == ">=") {
                    return lit(lv >= rv);
                }
                if (op == "<=") {
                    return lit(lv <= rv);
                }
            }

            if (std::holds_alternative<bool>(l_val) && std::holds_alternative<bool>(r_val)) {
                bool lv = std::get<bool>(l_val);
                bool rv = std::get<bool>(r_val);

                if (op == "&") {
                    return lit(lv && rv);
                }
                if (op == "|") {
                    return lit(lv || rv);
                }
                if (op == "==") {
                    return lit(lv == rv);
                }
                if (op == "!=") {
                    return lit(lv != rv);
                }
            }

            if (std::holds_alternative<std::string>(l_val) && std::holds_alternative<std::string>(r_val)) {
                std::string lv = std::get<std::string>(l_val);
                std::string rv = std::get<std::string>(r_val);

                if (op == "==") {
                    return lit(lv == rv);
                }
                if (op == "!=") {
                    return lit(lv != rv);
                }
            }
        }

        if (r_lit) {
            auto val = r_lit->getValue();
            
            if (std::holds_alternative<int64_t>(val) || std::holds_alternative<double>(val)) {
                double rv = std::holds_alternative<int64_t>(val) ? std::get<int64_t>(val) : std::get<double>(val);
                if (op == "+" && rv == 0){
                    return left;
                }
                if (op == "-" && rv == 0){
                    return left;
                }
                if (op == "*" && rv == 1){
                    return left;
                }
                if (op == "*" && rv == 0){
                    return lit(0.0);
                }
                if (op == "/" && rv == 1){
                    return left;
                }
            }
            
            if (std::holds_alternative<bool>(val)) {
                bool rv = std::get<bool>(val);
                if (op == "&"){
                    if(rv){
                        return left;
                    }
                    else{
                        return lit(false).ptr;
                    }
                }
                if (op == "|"){
                    if(rv){
                        return lit(true).ptr;
                    }
                    else{
                        return left;
                    }
                }
            }
        }

        if (l_lit) {
            auto val = l_lit->getValue();
            
            if (std::holds_alternative<int64_t>(val) || std::holds_alternative<double>(val)) {
                double lv = std::holds_alternative<int64_t>(val) ? std::get<int64_t>(val) : std::get<double>(val);
                if (op == "+" && lv == 0) {
                    return right;
                }
                if (op == "*" && lv == 1) {
                    return right;
                }
                if (op == "*" && lv == 0) {
                    return lit(0.0);
                }
            }

            if (std::holds_alternative<bool>(val)) {
                bool lv = std::get<bool>(val);
                if (op == "&") {
                    if(lv){
                        return right;
                    }
                    else{
                        return lit(false).ptr;
                    }
                }
                if (op == "|") {
                    if(lv){
                        return lit(true).ptr;
                    }
                    else{
                        return right;
                    }
                }
            }
        }
        return std::make_shared<BinaryExpression>(left, op, right);
    }

    // optimize query plan 
    std::shared_ptr<Node> optimize(std::shared_ptr<Node> node, int depth = 0) {
        if (!node) return node;
        const int MAX_STEPS = 200;
        if (depth > MAX_STEPS) {
            std::cerr << "WARNING: Optimizer exceeded max steps\n";
            return node;
        }

        for (auto& child : node->children) {
            child = optimize(child, depth + 1);   
        }

        if (node->right_child) {
            node->right_child = optimize(node->right_child, depth + 1);  
        }

        if (node->expr) {
            node->expr = simplify(node->expr);
        }

        if (node->children.empty()) return node;
        auto child = node->children[0];
        if (!child) return node;

        if (node->op == "head" && child->op == "head") {
            node->head_n = std::min(node->head_n, child->head_n);
            node->children = child->children;
            return node;
        }

        if (node->op == "select" && child->op == "select") {
            std::vector<std::string> new_cols;
            for (const auto& c : node->columns) {
                if (std::find(child->columns.begin(), child->columns.end(), c)
                    != child->columns.end()) {
                    new_cols.push_back(c);
                }
            }
            node->columns = new_cols;
            node->children = child->children;
            return node;
        }

        if (child->children.empty()) {
            return node;
        }
        
        auto curr = child->children[0];

        if (node->op == "select" && child->op == "head") {
            node->children[0] = curr;
            child->children[0] = node;
            return optimize(child, depth + 1);   
        }

        if (node->op == "filter" && child->op == "sort") {
            node->children[0] = curr;
            child->children[0] = node;
            return optimize(child, depth + 1);   
        }

        if (node->op == "filter" && child->op == "join") {

            auto filter_cols = extract_columns(node->expr);
            if (child->children.empty()) {
                return node;
            }

            auto left_child  = child->children[0];
            auto right_child = child->right_child;
            if (!left_child || !right_child) {
                return node;
            }

            auto left_cols = check_parent_columns(left_child);
            auto right_cols = check_parent_columns(right_child);

            bool in_left = !left_cols.empty() && contains_all(left_cols, filter_cols);
            bool in_right = !right_cols.empty() && contains_all(right_cols, filter_cols);

            if (in_left && !in_right) {
                auto filter_node = std::make_shared<Node>("filter");
                filter_node->expr = node->expr;
                filter_node->children = {left_child};
                child->children[0] = filter_node;
                return optimize(child, depth + 1);  
            }

            if (!in_left && in_right) {
                auto filter_node = std::make_shared<Node>("filter");
                filter_node->expr = node->expr;
                filter_node->children = {right_child};
                child->right_child = filter_node;
                return optimize(child, depth + 1);   
            }
        }

        if (node->op == "head" && child->op == "with_column") {
            node->children[0] = curr;
            child->children[0] = node;
            return optimize(child, depth + 1);   
        }

        if (node->op == "select" && child->op == "sort") {
            if (contains_all(node->columns, child->columns)) {
                node->children[0] = curr;
                child->children[0] = node;
                return optimize(child, depth + 1);   
            }
        }

        if (node->op == "filter" && child->op == "select") {
            auto filter_cols = extract_columns(node->expr);
            if (contains_all(child->columns, filter_cols)) {
                node->children[0] = curr;
                child->children[0] = node;
                return optimize(child, depth + 1);  
            }
        }

        if (node->op == "filter" && child->op == "with_column") {
            auto filter_cols = extract_columns(node->expr);
            std::string new_col_name = child->columns.empty() ? "" : child->columns[0];

            bool uses_new_col = false;
            for (const auto& fc : filter_cols) {
                if (fc == new_col_name) {
                    uses_new_col = true;
                    break;
                }
            }

            if (!uses_new_col) {
                node->children[0] = curr;
                child->children[0] = node;
                return optimize(child, depth + 1);  
            }
        }

        if (node->op == "select" && child->op == "with_column") {
            std::string new_col_name = child->columns.empty() ? "" : child->columns[0];

            bool needs_new_col = false;
            for (const auto& c : node->columns) {
                if (c == new_col_name) {
                    needs_new_col = true;
                    break;
                }
            }

            if (!needs_new_col) {
                node->children[0] = curr;
                return optimize(node, depth + 1);   
            }
        }

        return node;
    }

}
