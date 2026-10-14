#include "../include/dataframelib/eager_helper.h"
#include "../include/dataframelib/apachearrow.h"
#include <stdexcept>

namespace dataframelib {
 
     //  function to make key for join
     std::string make_key( const std::shared_ptr<arrow::Table>& tbl, const std::vector<std::string>& on, int64_t row) {
        std::string k;
        for (const auto& col_name : on) {
            auto col = tbl->GetColumnByName(col_name);
            // if column is not found
            if (!col) {
                throw std::invalid_argument("join: column not found: " + col_name);
            }
            auto res = col->GetScalar(row);
            if (!res.ok()) continue; 
            auto scalar = res.ValueOrDie();
            if (!scalar || !scalar->is_valid) {
                k += "||NULL";
            } else {
                k += "||" + scalar->ToString();
            }
        }  
        return k;
    }
    
    // safe bool get helper function  for join
    bool get_bool_value_safe(const std::shared_ptr<arrow::Scalar>& s) {
        if (!s || !s->is_valid) {
            return false;
        }
        if (s->type->id() != arrow::Type::BOOL) {
            return false;
        }
        return static_cast<const arrow::BooleanScalar&>(*s).value;
    }

    // take with nulls 
    std::shared_ptr<arrow::Table> take_with_nulls( const std::shared_ptr<arrow::Table>& tbl, const std::vector<int64_t>& indices) {
        int num_cols = tbl->num_columns();
        std::vector<std::shared_ptr<arrow::Field>> fields;
        std::vector<std::shared_ptr<arrow::ChunkedArray>> columns;
        auto schema = tbl->schema();
        for (int c = 0; c < num_cols; ++c) {
            auto field = schema->field(c);
            auto src_col = tbl->column(c);
            fields.push_back(field);
            std::unique_ptr<arrow::ArrayBuilder> builder;
            auto status = arrow::MakeBuilder(arrow::default_memory_pool(), field->type(), &builder);
            if (!status.ok()) {
                throw std::runtime_error("Failed to create builder: " + status.ToString());
            }
            for (int64_t idx : indices) {
                if (idx < 0) {
                    (void) builder->AppendNull();
                } else {
                    auto res = src_col->GetScalar(idx);
                    if (!res.ok()) continue; 
                    auto scalar = res.ValueOrDie();
                    if (!scalar || !scalar->is_valid) {
                        (void) builder->AppendNull();
                    } else {
                        (void) builder->AppendScalar(*scalar);
                    }
                }
            }

            std::shared_ptr<arrow::Array> arr;
            auto finish_status = builder->Finish(&arr);
            if (!finish_status.ok()) {
                throw std::runtime_error("Failed to finish array: " + finish_status.ToString());
            }
            columns.push_back(std::make_shared<arrow::ChunkedArray>(arr));
        }
        return arrow::Table::Make(std::make_shared<arrow::Schema>(fields), columns);
    }

    // helper function for group by for appending values to builder 
    void append_scalar_to_builder( arrow::ArrayBuilder* builder, const std::shared_ptr<arrow::Scalar>& scalar) {
        if (!scalar || !scalar->is_valid) {
            (void)builder->AppendNull();
            return;
        }
        auto type_id = scalar->type->id();

        if (type_id == arrow::Type::INT32) {
            (void)static_cast<arrow::Int32Builder*>(builder)->Append(
                std::static_pointer_cast<arrow::Int32Scalar>(scalar)->value);
        }
        else if (type_id == arrow::Type::INT64) {
            (void)static_cast<arrow::Int64Builder*>(builder)->Append(
                std::static_pointer_cast<arrow::Int64Scalar>(scalar)->value);
        }
        else if (type_id == arrow::Type::FLOAT) {
            (void)static_cast<arrow::FloatBuilder*>(builder)->Append(
                std::static_pointer_cast<arrow::FloatScalar>(scalar)->value);
        }
        else if (type_id == arrow::Type::DOUBLE) {
            (void)static_cast<arrow::DoubleBuilder*>(builder)->Append(
                std::static_pointer_cast<arrow::DoubleScalar>(scalar)->value);
        }
        else if (type_id == arrow::Type::STRING) {
            (void)static_cast<arrow::StringBuilder*>(builder)->Append(
                std::static_pointer_cast<arrow::StringScalar>(scalar)->value->ToString());
        }
        else if (type_id == arrow::Type::BOOL) {
            (void)static_cast<arrow::BooleanBuilder*>(builder)->Append(
                std::static_pointer_cast<arrow::BooleanScalar>(scalar)->value);
        }
        else {
            (void)builder->AppendNull();
        }
    }
    
    // helper function for group by to build groups 
    void build_groups( const std::shared_ptr<arrow::Table>& table, const std::vector<std::string>& keys, std::unordered_map<std::string, std::vector<int64_t>>& groups, std::vector<std::string>& order) {
        int64_t n = table->num_rows();
        std::vector<std::shared_ptr<arrow::ChunkedArray>> key_cols;
        for (const auto& k : keys) {
            auto col = table->GetColumnByName(k);
            if (!col) throw std::runtime_error("Column not found: " + k);
            key_cols.push_back(col);
        }

        for (int64_t i = 0; i < n; ++i) {
            std::string key;
            for (const auto& col : key_cols) {
                std::shared_ptr<arrow::Scalar> scalar;
                int64_t idx = i;
                for (int c = 0; c < col->num_chunks(); ++c) {
                    auto chunk = col->chunk(c);
                    if (idx < chunk->length()) {
                        scalar = chunk->GetScalar(idx).ValueOrDie();
                        break;
                    }
                    idx -= chunk->length();
                }
                if (!scalar || !scalar->is_valid) {
                    key += "NULL|";
                } else {
                    key += scalar->ToString() + "|";
                }
            }
            if (groups.find(key) == groups.end()) {
                order.push_back(key);
            }
            groups[key].push_back(i);
        }
    }   
    
    // helper function for group by to build key table 
    std::shared_ptr<arrow::Table> build_key_table( const std::shared_ptr<arrow::Table>& table, const std::unordered_map<std::string, std::vector<int64_t>>& groups, const std::vector<std::string>& order ) {
        std::vector<int64_t> first_rows;
        for (const auto& k : order) {
            first_rows.push_back(groups.at(k).front());
        }
        return manual_take_table(table, first_rows);
    }

    // helper function for group by to evaluate aggregations 
    void evaluate_aggregations( const std::shared_ptr<arrow::Table>& table, const std::unordered_map<std::string, std::vector<int64_t>>& groups, const std::vector<std::string>& order, const std::vector<std::shared_ptr<Expression>>& agg_exprs, std::vector<std::vector<AggResult>>& agg_values ) {
        for (const auto& k : order) {
            auto sub_table = manual_take_table(table, groups.at(k));
            for (size_t j = 0; j < agg_exprs.size(); ++j) {
                auto d = agg_exprs[j]->evaluate(sub_table);
                if (d.is_scalar() && d.scalar()->is_valid) {
                    agg_values[j].push_back({d.scalar(), true});
                } else {
                    agg_values[j].push_back({nullptr, false});
                }
            }
        }
    }

    // helper function for group by to build final table 
    EagerDataFrame build_final_table( const std::shared_ptr<arrow::Table>& key_table, const std::vector<std::string>& keys, const std::vector<std::string>& agg_names, const std::vector<std::vector<AggResult>>& agg_values ) {
        std::vector<std::shared_ptr<arrow::Field>> fields;
        std::vector<std::shared_ptr<arrow::ChunkedArray>> columns;
        // Add key columns
        for (const auto& k : keys) {
            fields.push_back(key_table->schema()->GetFieldByName(k));
            columns.push_back(key_table->GetColumnByName(k));
        }

        // Add aggregation columns
        for (size_t j = 0; j < agg_values.size(); ++j) {
            // Infer type from first valid result
            std::shared_ptr<arrow::DataType> result_type = arrow::float64();
            for (const auto& res : agg_values[j]) {
                if (res.is_valid && res.scalar) {
                    result_type = res.scalar->type;
                    break;
                }
            }
            // Create appropriate builder
            std::unique_ptr<arrow::ArrayBuilder> builder;
            (void)arrow::MakeBuilder(arrow::default_memory_pool(), result_type, &builder);
            // Append all results
            for (const auto& res : agg_values[j]) {
                if (res.is_valid && res.scalar) {
                    append_scalar_to_builder(builder.get(), res.scalar);
                } else {
                    (void)builder->AppendNull();
                }
            }
            std::shared_ptr<arrow::Array> arr;
            (void)builder->Finish(&arr);
            fields.push_back(arrow::field(agg_names[j], result_type));
            columns.push_back(std::make_shared<arrow::ChunkedArray>(arr));
        }
         return EagerDataFrame(arrow::Table::Make(std::make_shared<arrow::Schema>(fields), columns));
    }

}