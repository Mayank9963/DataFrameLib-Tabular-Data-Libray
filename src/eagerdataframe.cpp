
#include "../include/dataframelib/eagerdataframe.h"
#include "../include/dataframelib/apachearrow.h"   
#include <arrow/pretty_print.h>
#include <iostream>
#include <arrow/io/file.h>
#include <arrow/csv/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <algorithm>
#include <vector>
#include <memory>
#include <cstdint>
#include "../include/dataframelib/eager_helper.h" 
namespace dataframelib {
    
    EagerDataFrame::EagerDataFrame(std::shared_ptr<arrow::Table> data) : table_(data) {}

    std::shared_ptr<arrow::Table> EagerDataFrame::getTable() const {
        return table_; 
    }
    // read csv
    EagerDataFrame EagerDataFrame::read_csv(const std::string& path) {
        auto file_result = arrow::io::ReadableFile::Open(path);
        if (!file_result.ok()) {
            throw std::runtime_error("Failed to open file: " + path);
        }
        auto curr_file = *file_result;
        auto reader_result = arrow::csv::TableReader::Make(
            arrow::io::default_io_context(),
            curr_file,
            arrow::csv::ReadOptions::Defaults(),
            arrow::csv::ParseOptions::Defaults(),
            arrow::csv::ConvertOptions::Defaults());
        if (!reader_result.ok()) {
            throw std::runtime_error("Failed to create CSV reader");
        }
        auto reader = *reader_result;
        auto table_result = reader->Read();
        if (!table_result.ok()) {
            throw std::runtime_error("Failed to read CSV data");
        }
        auto table = *table_result;
        return EagerDataFrame(table);
    }

    // read parquet
    EagerDataFrame EagerDataFrame::read_parquet(const std::string& path) {
        auto file_result = arrow::io::ReadableFile::Open(path);
        if (!file_result.ok()) {
            throw std::runtime_error("Failed to open file: " + path);
        }
        auto file = *file_result;
        auto reader_result = parquet::arrow::OpenFile(file, arrow::default_memory_pool());
        if (!reader_result.ok()) {
            throw std::runtime_error("Failed to create parquet reader");
        }
        auto reader = std::move(*reader_result);
        std::shared_ptr<arrow::Table> table;
        auto status = reader->ReadTable(&table);
        if (!status.ok()) {
            throw std::runtime_error("Failed to read parquet file: " + status.ToString());
        }
        return EagerDataFrame(table);
    }

    EagerDataFrame& EagerDataFrame::write_csv(const std::string& path) {
        auto outfile_result = arrow::io::FileOutputStream::Open(path);
        if (!outfile_result.ok()) {
            throw std::runtime_error("Failed to open file for writing CSV: " + outfile_result.status().ToString());
        }
        auto outfile = outfile_result.ValueOrDie();
        arrow::Status status = arrow::csv::WriteCSV(*table_, arrow::csv::WriteOptions::Defaults(), outfile.get());
        if (!status.ok()) {
            throw std::runtime_error("Failed to write CSV data: " + status.ToString());
        }
        return *this;
    } 

    EagerDataFrame& EagerDataFrame::write_parquet(const std::string& path) {
        auto outfile_result = arrow::io::FileOutputStream::Open(path);
        if (!outfile_result.ok()) {
            throw std::runtime_error("Failed to open file for writing Parquet: " + outfile_result.status().ToString());
        }
        auto outfile = outfile_result.ValueOrDie();
        arrow::Status status = parquet::arrow::WriteTable(*table_, arrow::default_memory_pool(), outfile, table_->num_rows());
        if (!status.ok()) {
            throw std::runtime_error("Failed to write Parquet data: " + status.ToString());
        }
        return *this;
    }

    // CREATION
    EagerDataFrame EagerDataFrame::from_columns( const std::map<std::string, std::shared_ptr<arrow::Array>>& map) {
        std::vector<std::shared_ptr<arrow::Field>> fields;
        std::vector<std::shared_ptr<arrow::Array>> arrays;
        for (const auto& p : map) {
            fields.push_back(arrow::field(p.first, p.second->type()));
            arrays.push_back(p.second);
        }
        auto schema = std::make_shared<arrow::Schema>(fields);
        auto table = arrow::Table::Make(schema, arrays);
        return EagerDataFrame(table);
    }
    //from_columns using vector of pairs
    EagerDataFrame from_columns( const std::vector<std::pair<std::string, std::shared_ptr<arrow::Array>>>& vec) {
        // Convert vector to map
        std::map<std::string, std::shared_ptr<arrow::Array>> map(vec.begin(), vec.end());
        return from_columns(map);
    }

    // SELECT
    EagerDataFrame EagerDataFrame::select(const std::vector<std::string>& colnames) {
        std::vector<std::shared_ptr<arrow::Field>> fields;
        std::vector<std::shared_ptr<arrow::ChunkedArray>> cols;
        for (const auto& name : colnames) {
            auto col = table_->GetColumnByName(name);
            if (!col) {
                throw std::invalid_argument("select: column not found: " + name);
            }
            auto field = table_->schema()->GetFieldByName(name);
            if(!field){
                throw std::invalid_argument("select: field not found " + name);
            }
            fields.push_back(field);
            cols.push_back(col);
        }
        auto schema = std::make_shared<arrow::Schema>(fields);
        auto table = arrow::Table::Make(schema, cols);
        return EagerDataFrame(table);
    }

    // filter
    EagerDataFrame EagerDataFrame::filter(const std::shared_ptr<Expression>& expr) {
        auto vals = expr->evaluate(table_);
        std::vector<int64_t> keep;
        for (int64_t i = 0; i < table_->num_rows(); i++) {
            //vals iscan be both array or chunked array type
            if(vals.is_array()){
                auto res = vals.make_array()->GetScalar(i);
                if (!res.ok()) {
                    continue; 
                }
                auto s = res.ValueOrDie();
                if (get_bool_value_safe(s)) {
                    keep.push_back(i);
                }
            }else{
                auto res = vals.chunked_array()->GetScalar(i);
                if (!res.ok()) {
                    continue;
                }
                auto s = res.ValueOrDie();
                if (get_bool_value_safe(s)) {
                    keep.push_back(i);
                }
            }
        }
        auto table = manual_take_table(table_, keep);
        return EagerDataFrame(table);
    }

    // WITH COLUMN
    EagerDataFrame EagerDataFrame::with_column(const std::string& name, const std::shared_ptr<Expression>& expr) {
        auto curr = expr->evaluate(table_);
        std::shared_ptr<arrow::ChunkedArray> col;
        
        if (curr.is_chunked_array()) {
            col = curr.chunked_array();
        } else if (curr.is_array()) {
            col = std::make_shared<arrow::ChunkedArray>(curr.make_array());
        } else if (curr.is_scalar()) {
            col = std::make_shared<arrow::ChunkedArray>(arrow::MakeArrayFromScalar(*curr.scalar(), table_->num_rows()).ValueOrDie());
        } else {
            throw std::runtime_error("Datum kind is not array, chunked_array, or scalar in with_column");
        }
        
        // Check if column already exists - REPLACE it instead of adding
        auto existing_col = table_->GetColumnByName(name);
        
        if (existing_col != nullptr) {
            // REPLACE existing column
            std::vector<std::shared_ptr<arrow::Field>> fields;
            std::vector<std::shared_ptr<arrow::ChunkedArray>> columns;
            
            for (int i = 0; i < table_->num_columns(); i++) {
                if (table_->schema()->field(i)->name() == name) {
                    // Replace this column
                    fields.push_back(arrow::field(name, col->type()));
                    columns.push_back(col);
                } else {
                    // Keep existing column
                    fields.push_back(table_->schema()->field(i));
                    columns.push_back(table_->column(i));
                }
            }
            
            auto schema = arrow::schema(fields);
            return EagerDataFrame(arrow::Table::Make(schema, columns));
        } else {
            // ADD new column (original behavior)
            auto new_table = table_->AddColumn(table_->num_columns(), arrow::field(name, col->type()), col).ValueOrDie();
            return EagerDataFrame(new_table);
        }
    }

    // GROUP BY
    GroupedDataFrame::GroupedDataFrame(const std::shared_ptr<arrow::Table>& table, const std::vector<std::string>& keys) : table_(table), keys_(keys) {}

    GroupedDataFrame EagerDataFrame::group_by(const std::vector<std::string>& keys) {
        return GroupedDataFrame(table_, keys);
    }

   
    EagerDataFrame EagerDataFrame::join(const EagerDataFrame& other, const std::vector<std::string>& on, const std::string& how) {
        try {    
            if (on.empty()) { 
                throw std::invalid_argument("join: 'on' keys must not be empty"); 
            }
            if (how != "inner" && how != "left" && how != "right" && how != "outer") {
                throw std::invalid_argument("Invalid join type");
            }
            
            auto left_table  = table_;
            auto right_table = other.getTable();
            
            for (const auto& key : on) {
                if (!left_table->GetColumnByName(key)) { 
                    throw std::invalid_argument("join: column not found in left table: " + key);
                }
                if (!right_table->GetColumnByName(key)) { 
                    throw std::invalid_argument("join: column not found in right table: " + key);
                }
            }
            
            // Number of rows 
            int64_t left_n  = left_table->num_rows();
            int64_t right_n = right_table->num_rows();
            
            // Build a hash map from right keys -> list of row indices
            std::unordered_map<std::string, std::vector<int64_t>> right_map;
            for (int64_t j = 0; j < right_n; ++j) {
                right_map[make_key(right_table, on, j)].push_back(j);
            }
            
            // Vectors to store row indices for the new table
            std::vector<int64_t> L;
            std::vector<int64_t> R;
            
            // Track which right rows got matched (needed for right/outer join)
            std::vector<bool> right_matched(right_n, false);
            
            // INNER + LEFT logic
            for (int64_t i = 0; i < left_n; ++i) {
                auto lk = make_key(left_table, on, i);
                auto it = right_map.find(lk);
                if (it != right_map.end()) {
                    // matched — emit one row per matching right row
                    for (int64_t j : it->second) {
                        L.push_back(i);
                        R.push_back(j);
                        right_matched[j] = true;
                    }
                } else if (how == "left" || how == "outer") {
                    L.push_back(i);
                    R.push_back(-1);  
                }
            }
            
            // RIGHT + OUTER logic
            if (how == "right" || how == "outer") {
                for (int64_t j = 0; j < right_n; ++j) {
                    if (!right_matched[j]) {
                        L.push_back(-1);  
                        R.push_back(j);
                    }
                }
            }
            
            // For inner join (no -1 sentinels), we can use the faster manual_take_table
            std::shared_ptr<arrow::Table> left_joined;
            std::shared_ptr<arrow::Table> right_joined;
            
            if (how == "inner") {
                left_joined  = manual_take_table(left_table, L);
                right_joined = manual_take_table(right_table, R);
            } else {
                left_joined  = take_with_nulls(left_table, L);
                right_joined = take_with_nulls(right_table, R);
            }
            
            // All of left + non-key columns from right
            std::vector<std::shared_ptr<arrow::Field>> fields;
            std::vector<std::shared_ptr<arrow::ChunkedArray>> cols;
            
            // Process left table columns and coalesce keys if necessary
            for (int i = 0; i < left_joined->num_columns(); ++i) {
                auto f = left_joined->schema()->field(i);
                auto col_name = f->name();
                
                if (std::find(on.begin(), on.end(), col_name) != on.end()) {
                    // It's a join key column: Coalesce values
                    auto left_col = left_joined->column(i);
                    auto right_col = right_joined->GetColumnByName(col_name);
                    
                    std::unique_ptr<arrow::ArrayBuilder> builder;
                    (void)arrow::MakeBuilder(arrow::default_memory_pool(), f->type(), &builder);
                    
                    for (size_t r = 0; r < L.size(); ++r) {
                        // If L[r] == -1, the row is from the right side; pull the key from right_joined
                        if (L[r] == -1) {
                            auto res = right_col->GetScalar(r);
                            if (res.ok() && res.ValueOrDie()->is_valid) {
                                (void)builder->AppendScalar(*res.ValueOrDie());
                            } else {
                                (void)builder->AppendNull();
                            }
                        } else {
                            auto res = left_col->GetScalar(r);
                            if (res.ok() && res.ValueOrDie()->is_valid) {
                                (void)builder->AppendScalar(*res.ValueOrDie());
                            } else {
                                (void)builder->AppendNull();
                            }
                        }
                    }
                    std::shared_ptr<arrow::Array> arr;
                    (void)builder->Finish(&arr);
                    
                    fields.push_back(f);
                    cols.push_back(std::make_shared<arrow::ChunkedArray>(arr));
                } else {
                    // Standard left column
                    fields.push_back(f);
                    cols.push_back(left_joined->column(i));
                }
            }
            
            // Add non-key columns from the right table
            for (int i = 0; i < right_joined->num_columns(); ++i) {
                auto f = right_joined->schema()->field(i);
                if (std::find(on.begin(), on.end(), f->name()) == on.end()) {
                    std::string col_name = f->name();
                    // Prevent duplicate columns 
                    if (left_joined->schema()->GetFieldByName(col_name) != nullptr) {
                        col_name += "_y"; 
                    }
                    fields.push_back(arrow::field(col_name, f->type()));
                    cols.push_back(right_joined->column(i));
                }
            }
            
            return EagerDataFrame(arrow::Table::Make(std::make_shared<arrow::Schema>(fields), cols));
        } catch (const std::exception& e) {
            throw std::runtime_error("Join failed (" + how + "): " + std::string(e.what()));
        }
    }

    // head
    EagerDataFrame EagerDataFrame::head(int n) {
        if (n <= 0) {
            return EagerDataFrame(table_->Slice(0, 0));
        }
        // Don't throw error if n > num_rows, just return all rows
        int64_t actual_n = std::min(static_cast<int64_t>(n), table_->num_rows());
        return EagerDataFrame(table_->Slice(0, actual_n));
    }

    //sort
    std::function<bool(int64_t, int64_t)> EagerDataFrame::make_comparator( const std::vector<std::shared_ptr<arrow::ChunkedArray>>& cols, bool asc) const {
        return [cols, asc](int64_t a, int64_t b) {
            for (auto& col : cols) {
                auto sa = col->GetScalar(a).ValueOrDie();
                auto sb = col->GetScalar(b).ValueOrDie();
                // NULL handling (nulls last)
                if (!sa->is_valid && !sb->is_valid){
                    continue;
                }
                if (!sa->is_valid || !sb->is_valid){
                    return false;
                }  
                auto type_id = col->type()->id();
                // string
                if (type_id == arrow::Type::STRING) {
                    auto A = sa->ToString();
                    auto B = sb->ToString();
                    if (A == B) {
                        continue;
                    }
                    if (asc) {
                        return A < B;
                    } else {
                        return A > B;
                    }
                }
                // numeric 
                double va = BinaryExpression::get_scalar_double(sa);
                double vb = BinaryExpression::get_scalar_double(sb);
                if (va == vb) {
                    continue;
                }
                if(asc){
                    return va < vb;
                }else{
                    return va > vb;
                }
            }
            return false;
        };
    }

    //sort
    EagerDataFrame EagerDataFrame::sort(const std::vector<std::string>& columns, bool asc) {
        //if columns are empty
        if (columns.empty()) {
            return EagerDataFrame(table_);
        }
        //get number of rows
        int64_t n = table_->num_rows();
        std::vector<std::shared_ptr<arrow::ChunkedArray>> cols;
        //get columns
        for (const auto& name : columns) {
            auto col = table_->GetColumnByName(name);
            if (!col) {
                throw std::invalid_argument("sort: column not found: " + name);
            }
            cols.push_back(col);
        }
        //initialize idx array
        std::vector<int64_t> idx(n);
        for (int64_t i = 0; i < n; ++i) idx[i] = i;
        //sort using stable sort
        std::stable_sort(idx.begin(), idx.end(), make_comparator(cols, asc));
        //make new dataframe
        auto newtable = manual_take_table(table_, idx);
        return EagerDataFrame(newtable);
    }
    
    // helpers
    EagerDataFrame read_csv(const std::string& path) {
        return EagerDataFrame::read_csv(path);
    }

    EagerDataFrame read_parquet(const std::string& path) {
        return EagerDataFrame::read_parquet(path);
    }

    EagerDataFrame from_columns(const std::map<std::string, std::shared_ptr<arrow::Array>>& map) {
        return EagerDataFrame::from_columns(map);
    }

    // Main aggregate function
    EagerDataFrame GroupedDataFrame::aggregate( const std::map<std::string, std::shared_ptr<Expression>>& agg_map) {
        if (agg_map.empty()) {
            throw std::invalid_argument("aggregate: no aggregations provided");
        }
        // Build groups
        std::unordered_map<std::string, std::vector<int64_t>> groups;
        std::vector<std::string> order;
        build_groups(table_, keys_, groups, order);
        //  Prepare aggregation structures 
        std::vector<std::vector<AggResult>> agg_values(agg_map.size());
        std::vector<std::string> agg_names;
        std::vector<std::shared_ptr<Expression>> agg_exprs;
        for (const auto& p : agg_map) {
            agg_names.push_back(p.first);
            agg_exprs.push_back(p.second);
        }
        //  Evaluate aggregations
        evaluate_aggregations(table_, groups, order, agg_exprs, agg_values);
        //  Build key table
        auto key_table = build_key_table(table_, groups, order);
        //  Build final result
        return build_final_table(key_table, keys_, agg_names, agg_values);
    }

    // Aggregate function using vector of pairs
    EagerDataFrame GroupedDataFrame::aggregate( const std::vector<std::pair<std::string, std::string>>& agg_vec) {
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
    // helper 
    EagerDataFrame GroupedDataFrame::aggregate(const std::unordered_map<std::string, std::shared_ptr<Expression>>& agg_map) {
        std::map<std::string, std::shared_ptr<Expression>> ordered_map(agg_map.begin(), agg_map.end());
        return aggregate(ordered_map);
    }

    // Basic ops
    void EagerDataFrame::print() const {
        if (table_) {
            (void) arrow::PrettyPrint(*table_, {}, &std::cout);
        }
    }
    // get number of rows
    int64_t EagerDataFrame::num_rows() const {
        if (!table_) {
            return 0;
        }
        return table_->num_rows();
    }
    // get number of columns
    int EagerDataFrame::num_columns() const {
        if (!table_) {  
            return 0;
        }
        return table_->num_columns();
    }

}



