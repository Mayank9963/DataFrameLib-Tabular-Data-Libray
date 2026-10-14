// this is our header file of the query optimiser file 

#ifndef QUERYOPTIMISER_H
#define QUERYOPTIMISER_H
#include "dataframelib/graph.h"
#include "dataframelib/expression.h"
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
namespace dataframelib {
    //this is our optimise function 
    std::shared_ptr<Node> optimize(std::shared_ptr<Node> node, int depth = 0); //this is our core optimiser functions takes in query plan and output optimised plan 
    bool contains_all(const std::vector<std::string>& sup, const std::vector<std::string>& sub); //this function is used to check if the sup set contains all the elements of the sub set 
    std::vector<std::string> extract_columns(std::shared_ptr<Expression> expr); //this function is used tfind the column used in the expressions
    std::shared_ptr<Expression> simplify(std::shared_ptr<Expression> expr); //this function is used to simplify the expression 
}
#endif
