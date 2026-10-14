
#include "../include/dataframelib/lazydataframe.h"
#include "../include/dataframelib/eagerdataframe.h"
#include "../include/dataframelib/graph.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <algorithm>
#include "../include/dataframelib/queryoptimiser.h"
#include "../include/dataframelib/expression.h"
#include <fstream>
#include <cstdint>
namespace dataframelib {

    //execute function execute the query plan
    EagerDataFrame execute(std::shared_ptr<Node> node) {
        //scan csv
        if (node->op == "scan_csv") {
            return EagerDataFrame::read_csv(node->file_path);
        }
        //scan parquet
        else if (node->op == "scan_parquet") {
            return EagerDataFrame::read_parquet(node->file_path);
        }
        //check for child node
        if (node->children.empty()) {
            throw std::runtime_error("Invalid plan: missing child node");
        }
        //recursive call to execute child node
        auto input = execute(node->children[0]);
        //check for child node
        if (node->children.empty() && node->op != "scan_csv" && node->op != "scan_parquet") {
            throw std::runtime_error("Invalid plan: missing child");
        }
        //filter
        if (node->op == "filter") {
            return input.filter(node->expr);
        }
        //select
        else if (node->op == "select") {
            return input.select(node->columns);
        }
        //with_column
        else if (node->op == "with_column") {
            return input.with_column(node->columns[0], node->expr);
        }
        //sort
        else if (node->op == "sort") {
            return input.sort(node->columns, node->ascending);
        }
        //head
        else if (node->op == "head") {
            return input.head(node->head_n);
        }
        //join
        else if (node->op == "join") {
            if (!node->right_child) {
                throw std::runtime_error("Join missing right child");
            }
            auto right = execute(node->right_child);
            return input.join(right, node->columns, node->join_type);
        }
        else if (node->op == "group_by") {
            return input;
        }
        else if (node->op == "aggregate") {
            if (node->children.empty()) {
                throw std::runtime_error("aggregate missing child");
            }
            auto group_node = node->children[0];
            if (group_node->op == "group_by") {
                auto data = execute(group_node->children[0]);
                return data.group_by(group_node->columns).aggregate(node->agg_map);
            }
            throw std::runtime_error("aggregate must follow group_by");
        }
        throw std::runtime_error("Unknown operation in execute()");
    }

    //scan csv
    LazyDataFrame LazyDataFrame::scan_csv(const std::string& path) {
        auto node = std::make_shared<Node>("scan_csv");
        node->file_path = path;
        return LazyDataFrame(node);
    }

    //scan parquet
    LazyDataFrame LazyDataFrame::scan_parquet(const std::string& path) {
        auto node = std::make_shared<Node>("scan_parquet");
        node->file_path = path;
        return LazyDataFrame(node);
    }

    //filter
    LazyDataFrame LazyDataFrame::filter(const std::shared_ptr<Expression>& expr) {
        auto node = std::make_shared<Node>("filter");
        node->children.push_back(plan_root_);
        node->expr = expr;
        return LazyDataFrame(node);
    }

    //select
    LazyDataFrame LazyDataFrame::select(const std::vector<std::string>& colnames) {
        auto node = std::make_shared<Node>("select");
        node->children.push_back(plan_root_);
        node->columns = colnames;
        return LazyDataFrame(node);
    }

    //with_column
    LazyDataFrame LazyDataFrame::with_column(const std::string& name, const std::shared_ptr<Expression>& expr) {
        auto node = std::make_shared<Node>("with_column");
        node->children.push_back(plan_root_);
        node->columns = {name};
        node->expr = expr;
        return LazyDataFrame(node);
    }

    //sort
    LazyDataFrame LazyDataFrame::sort(const std::vector<std::string>& columns, bool asc) {
        auto node = std::make_shared<Node>("sort");
        node->children.push_back(plan_root_);
        node->columns = columns;
        node->ascending = asc;
        return LazyDataFrame(node);
    }

    //head
    LazyDataFrame LazyDataFrame::head(int n) {
        auto node = std::make_shared<Node>("head");
        node->children.push_back(plan_root_);
        node->head_n = n;
        return LazyDataFrame(node);
    }

    //collect
    EagerDataFrame LazyDataFrame::collect() {
        plan_root_ = optimize(plan_root_);
        return execute(plan_root_);
    }
    //join
    LazyDataFrame LazyDataFrame::join( const LazyDataFrame& other, const std::vector<std::string>& on, const std::string& how) {
        auto node = std::make_shared<Node>("join");
        node->children.push_back(plan_root_);      // left
        node->right_child = other.plan_root_;      // right
        node->columns = on;
        node->join_type = how;
        return LazyDataFrame(node);
    }

    //group_by
    LazyDataFrame LazyDataFrame::group_by(const std::vector<std::string>& keys) {
        auto node = std::make_shared<Node>("group_by");
        node->children.push_back(plan_root_);
        node->columns = keys;
        return LazyDataFrame(node);
    }

    //aggregate
    LazyDataFrame LazyDataFrame::aggregate( const std::map<std::string, std::shared_ptr<Expression>>& agg_map) {
        auto node = std::make_shared<Node>("aggregate");
        node->children.push_back(plan_root_);
        node->agg_map = agg_map;
        return LazyDataFrame(node);
    }
    //aggregate
    LazyDataFrame LazyDataFrame::aggregate(const std::unordered_map<std::string, std::shared_ptr<Expression>>& agg_map) {
        std::map<std::string, std::shared_ptr<Expression>> ordered_map(agg_map.begin(), agg_map.end());
        return aggregate(ordered_map);
    }

    LazyDataFrame LazyDataFrame::aggregate(const std::vector<std::pair<std::string, std::string>>& agg_vec) {
        std::map<std::string, std::shared_ptr<Expression>> agg_map;
        for (const auto& [col_name, agg_func] : agg_vec) {
            auto col_expr = col(col_name);
            std::string output_name = col_name + "_" + agg_func;
            if (agg_func == "sum") {
                agg_map[output_name] = col_expr.sum();
            } else if (agg_func == "mean" || agg_func == "avg") {
                agg_map[output_name] = col_expr.mean();
            } else if (agg_func == "count") {
                agg_map[output_name] = col_expr.count();
            } else if (agg_func == "min") {
                agg_map[output_name] = col_expr.min();
            } else if (agg_func == "max") {
                agg_map[output_name] = col_expr.max();
            } else {
                throw std::runtime_error("Unknown aggregation function: " + agg_func);
            }
        }    
        return aggregate(agg_map);
    }
    void LazyDataFrame::explain(const std::string& path) {
        if (path.size() >= 4 && path.substr(path.size() - 4) == ".txt") {
            std::ofstream ofs(path);
            if (ofs.is_open()) {
                print_plan(plan_root_, ofs);
                return;
            }
        }
        // Fallback to DOT/PNG if not a txt file or file open failed
        std::string dot_path = path + ".dot";
        export_to_dot(plan_root_, dot_path);
        std::string cmd = "dot -Tpng " + dot_path + " -o " + path;
        int res = std::system(cmd.c_str());
        if (res != 0) {
            // If dot failed, copy the text-based DOT file to the target path so the file exists
            std::ifstream src(dot_path, std::ios::binary);
            std::ofstream dst(path, std::ios::binary);
            dst << src.rdbuf();
        }
        std::remove(dot_path.c_str());
    }
}