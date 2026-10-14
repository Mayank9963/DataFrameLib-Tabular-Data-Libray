#ifndef EAGER_HELPERS_H
#define EAGER_HELPERS_H

#include <arrow/api.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <map>
#include "../include/dataframelib/expression.h"
#include "../include/dataframelib/eagerdataframe.h"
#include <string>
#include <cstdint>
#include <functional>
#include <cstdint>

namespace dataframelib {

    // helper declarations
    bool get_bool_value_safe(const std::shared_ptr<arrow::Scalar>& s);

    std::string make_key( const std::shared_ptr<arrow::Table>& tbl, const std::vector<std::string>& on, int64_t row);

    std::shared_ptr<arrow::Table> take_with_nulls( const std::shared_ptr<arrow::Table>& tbl,const std::vector<int64_t>& indices);

    void append_scalar_to_builder( arrow::ArrayBuilder* builder, const std::shared_ptr<arrow::Scalar>& scalar);

    void build_groups( const std::shared_ptr<arrow::Table>& table, const std::vector<std::string>& keys, std::unordered_map<std::string, std::vector<int64_t>>& groups, std::vector<std::string>& order);

    std::shared_ptr<arrow::Table> build_key_table( const std::shared_ptr<arrow::Table>& table, const std::unordered_map<std::string, std::vector<int64_t>>& groups, const std::vector<std::string>& order );

    struct AggResult { std::shared_ptr<arrow::Scalar> scalar; bool is_valid; };

    void evaluate_aggregations( const std::shared_ptr<arrow::Table>& table, const std::unordered_map<std::string, std::vector<int64_t>>& groups, const std::vector<std::string>& order, const std::vector<std::shared_ptr<Expression>>& agg_exprs, std::vector<std::vector<AggResult>>& agg_values );

    EagerDataFrame build_final_table( const std::shared_ptr<arrow::Table>& key_table, const std::vector<std::string>& keys, const std::vector<std::string>& agg_names, const std::vector<std::vector<AggResult>>& agg_values );

}

#endif