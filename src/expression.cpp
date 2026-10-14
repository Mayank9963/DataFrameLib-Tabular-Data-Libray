#include "../include/dataframelib/expression.h"
#include <arrow/builder.h>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <cmath>
#include <vector>
#include <cstdint>

namespace dataframelib {
    // COLUMN
    ColumnExpression::ColumnExpression(const std::string& name) : column_name_(name) {}

    std::string ColumnExpression::to_string() const {
        return "col(" + column_name_ + ")";
    }

    arrow::Datum ColumnExpression::evaluate(const std::shared_ptr<arrow::Table>& table) const {
        auto col = table->GetColumnByName(column_name_);
        if (!col) {
            throw std::runtime_error("Column not found: " + column_name_);
        }
        return arrow::Datum(col);
    }

    const std::string& ColumnExpression::getColumnName() const {
        return column_name_;
    }

    // LITERAL
    LiteralExpression::LiteralExpression( const std::variant<int64_t, double, std::string, bool>& val) : value_(val) {}

    // std::string LiteralExpression::to_string() const {
    //     return "lit(?)";
    // }

    // Helper function - add this before LiteralExpression::to_string()
    std::string literal_value_to_string(const std::variant<int64_t, double, std::string, bool>& value) {
        if (std::holds_alternative<int64_t>(value)) {
            return "lit(" + std::to_string(std::get<int64_t>(value)) + ")";
        } else if (std::holds_alternative<double>(value)) {
            return "lit(" + std::to_string(std::get<double>(value)) + ")";
        } else if (std::holds_alternative<std::string>(value)) {
            return "lit(\"" + std::get<std::string>(value) + "\")";
        } else if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? "lit(true)" : "lit(false)";
        }
        return "lit(?)";
    }

    // Then update LiteralExpression::to_string()
    std::string LiteralExpression::to_string() const {
        return literal_value_to_string(value_);
    }

    arrow::Datum LiteralExpression::evaluate(const std::shared_ptr<arrow::Table>&) const {
        if (std::holds_alternative<int64_t>(value_)) {
            return arrow::Datum(arrow::MakeScalar(std::get<int64_t>(value_)));
        }
        if (std::holds_alternative<double>(value_)) {
            return arrow::Datum(arrow::MakeScalar(std::get<double>(value_)));
        }
        if (std::holds_alternative<std::string>(value_)) {
            return arrow::Datum(arrow::MakeScalar(std::get<std::string>(value_)));
        }
        if (std::holds_alternative<bool>(value_)) {
            return arrow::Datum(arrow::MakeScalar(std::get<bool>(value_)));
        }
        throw std::runtime_error("Unknown literal type");
    }

    const std::variant<int64_t, double, std::string, bool>& LiteralExpression::getValue() const {
        return value_;
    }

    // ALIAS
    AliasExpression::AliasExpression(std::shared_ptr<Expression> child, const std::string& name) : child_(child), name_(name) {}

    std::string AliasExpression::to_string() const { 
        return child_->to_string() + ".alias(" + name_ + ")";
    }

    arrow::Datum AliasExpression::evaluate(const std::shared_ptr<arrow::Table>& table) const { 
        return child_->evaluate(table);    
    }

    const std::string& AliasExpression::getName() const { 
        return name_;
    }

    std::shared_ptr<Expression> AliasExpression::getChild() const { 
        return child_;
    }


    // UNARY
    //helper to get value at row i from a chunked array
    std::shared_ptr<arrow::Scalar> new_get_scalar( const std::shared_ptr<arrow::ChunkedArray>& arr, int64_t i) {
        if (!arr || arr->num_chunks() == 0) return nullptr;
        int64_t idx = i;
        for (int c = 0; c < arr->num_chunks(); c++) {
            auto chunk = arr->chunk(c); 
            if (idx < chunk->length()) {
                return chunk->GetScalar(idx).ValueOrDie();
            }
            idx -= chunk->length();
        }
        return nullptr; // out of bounds
    }

    // Returns the Scalar at row i for any Datum kind (scalar / chunked / array).
    static std::shared_ptr<arrow::Scalar> get_scalar_at(const arrow::Datum& d, int64_t i) {
        if (d.is_scalar()) {return d.scalar();}
        if (d.is_chunked_array()){
            auto arr = d.chunked_array();
            auto s = new_get_scalar(arr, i);
            if (!s) {
                return nullptr;
            }
            return s;
        }
        if (d.is_array()) {
            auto arr = d.make_array();
            if (i < 0 || i >= arr->length()) {
                return nullptr;
            }
            return arr->GetScalar(i).ValueOrDie();
        }
        return nullptr;
    }

    UnaryExpression::UnaryExpression(const std::string& op, std::shared_ptr<Expression> child) : op_(op), child_(child) {}

    std::string UnaryExpression::to_string() const { 
        return op_ + "(" + child_->to_string() + ")";
    }

    arrow::Datum UnaryExpression::evaluate(const std::shared_ptr<arrow::Table>& table) const {
        auto input = child_->evaluate(table);
        int64_t n = table->num_rows();
        if (op_ == "~") {
            arrow::BooleanBuilder builder;
            for (int64_t i = 0; i < n; i++) {
                auto scalar = get_scalar_at(input, i);
                if (!scalar || !scalar->is_valid) {
                    (void) builder.AppendNull();
                    continue;
                }
                bool val = static_cast<const arrow::BooleanScalar&>(*scalar).value;
                (void) builder.Append(!val);
            }
            std::shared_ptr<arrow::Array> out;
            (void) builder.Finish(&out);
            return arrow::Datum(out);
        }

        // is_null
        else if (op_ == "is_null") {
            arrow::BooleanBuilder builder;
            for (int64_t i = 0; i < n; i++) {
                auto s = get_scalar_at(input, i);
                (void) builder.Append(!s || !s->is_valid);
            }
            std::shared_ptr<arrow::Array> out;
            (void) builder.Finish(&out);
            return arrow::Datum(out);
        }

        // is_not_null
        else if (op_ == "is_not_null") {
            arrow::BooleanBuilder builder;
            for (int64_t i = 0; i < n; i++) {
                auto s = get_scalar_at(input, i);
                (void) builder.Append(s && s->is_valid);
            }
            std::shared_ptr<arrow::Array> out;
            (void) builder.Finish(&out);
            return arrow::Datum(out);
        }
        throw std::runtime_error("Unknown unary operator");
    }

    std::shared_ptr<Expression> UnaryExpression::getChild() const {
        return child_; 
    }

    const std::string& UnaryExpression::getOp() const { 
        return op_;
    }

    // BINARY
    BinaryExpression::BinaryExpression(std::shared_ptr<Expression> left, const std::string& op, std::shared_ptr<Expression> right) : leftside_(left), op_(op), rightside_(right) {}

    std::string BinaryExpression::to_string() const { 
        return leftside_->to_string() + " " + op_ + " " + rightside_->to_string();
    }

    double BinaryExpression::get_scalar_double(const std::shared_ptr<arrow::Scalar>& s) {
        if (!s || !s->is_valid) {
            return 0.0;
        }
        auto type_id = s->type->id();
        if (type_id == arrow::Type::DOUBLE) {
            return static_cast<const arrow::DoubleScalar&>(*s).value;
        }
        else if (type_id == arrow::Type::INT64) {
            return static_cast<const arrow::Int64Scalar&>(*s).value;
        }
        else if (type_id == arrow::Type::INT32) {
            return static_cast<const arrow::Int32Scalar&>(*s).value;
        }
        else if (type_id == arrow::Type::FLOAT) {
            return static_cast<const arrow::FloatScalar&>(*s).value;
        }
        else if (type_id == arrow::Type::BOOL) {
            throw std::invalid_argument("Boolean cannot be used in numeric operation");
        }
        else {
            throw std::invalid_argument("Invalid type in numeric operation");
        }
    }

    double BinaryExpression::get_value_at(const arrow::Datum& d, int64_t i) { 
        return get_scalar_double(get_scalar_at(d, i)); 
    }

    //for removing null values
    bool is_null_scalar(const std::shared_ptr<arrow::Scalar>& l, const std::shared_ptr<arrow::Scalar>& r) {
        return (!l || !r || !l->is_valid || !r->is_valid);
    }
    //evaluating boolean
    bool evaluate_boolean_logic(const std::string& op, const std::shared_ptr<arrow::Scalar>& l, const std::shared_ptr<arrow::Scalar>& r) {

        if (l->type->id() != arrow::Type::BOOL || r->type->id() != arrow::Type::BOOL) {
            throw std::invalid_argument("AND/OR requires boolean operands");
        }

        bool lb = static_cast<const arrow::BooleanScalar&>(*l).value;
        bool rb = static_cast<const arrow::BooleanScalar&>(*r).value;

        if (op == "&") {
            return lb && rb;
        }
        return lb || rb;
    }
    //evaluating string
    bool evaluate_string_cmp(const std::string& op, const std::shared_ptr<arrow::Scalar>& l, const std::shared_ptr<arrow::Scalar>& r) {

        std::string lv = l->ToString();
        std::string rv = r->ToString();
        if (op == ">") {
            return lv > rv;
        }
        if (op == "<") {
            return lv < rv;
        }
        if (op == "==") {
            return lv == rv;
        }
        if (op == "!=") {
            return lv != rv;
        }
        if (op == "<=") {
            return lv <= rv;
        }
        if (op == ">=") {
            return lv >= rv;
        }
        return false;
    }
    //evaluating boolean
    bool evaluate_bool_cmp(const std::string& op, const std::shared_ptr<arrow::Scalar>& l, const std::shared_ptr<arrow::Scalar>& r) {

        bool lv = std::static_pointer_cast<arrow::BooleanScalar>(l)->value;
        bool rv = std::static_pointer_cast<arrow::BooleanScalar>(r)->value;
        if (op == "==") {
            return lv == rv;
        }
        if (op == "!=") {
            return lv != rv;
        }
        if (op == "&") {
            return lv && rv;
        }
        if (op == "|") {
            return lv || rv;
        }
        throw std::invalid_argument("Invalid operator for boolean");
    }
    //evaluating numeric
    bool evaluate_numeric_cmp(const std::string& op, const arrow::Datum& left, const arrow::Datum& right, int64_t i) {
        // double lv = get_value_at(left, i);
        // double rv = get_value_at(right, i);
        double lv = BinaryExpression::get_value_at(left, i);
        double rv = BinaryExpression::get_value_at(right, i);
        if (op == ">") {
            return lv > rv;
        }
        if (op == "<") {
            return lv < rv;
        }
        if (op == "==") {
            return lv == rv;
        }
        if (op == "!=") {
            return lv != rv;
        }
        if (op == "<=") {
            return lv <= rv;
        }
        if (op == ">=") {
            return lv >= rv;
        }
        return false;
    }
    //evaluating arithmetic
    bool evaluate_arithmetic(const std::string& op, double lv, double rv, double& res) {

        if (op == "+") {
            res = lv + rv;
        }
        else if (op == "-") {
            res = lv - rv;
        }
        else if (op == "*") {
            res = lv * rv;
        }
        else if (op == "/") {
            if (rv == 0) {
                return false;
            }
            res = lv / rv;
        }
        else if (op == "%") {
            if (rv == 0) return false;
            res = std::fmod(lv, rv);
        }
        return true;
    }
    //evaluating binary expression
    arrow::Datum BinaryExpression::evaluate(const std::shared_ptr<arrow::Table>& table) const {
        auto left = leftside_->evaluate(table);
        auto right = rightside_->evaluate(table);
        int64_t n = table->num_rows();
        bool is_cmp = (op_ == ">" || op_ == "<" || op_ == "==" || op_ == "!=" || op_ == "<=" || op_ == ">=" || op_ == "&" || op_ == "|");
        if (is_cmp) {
            arrow::BooleanBuilder builder;
            for (int64_t i = 0; i < n; i++) {
                auto l_scalar = get_scalar_at(left, i);
                auto r_scalar = get_scalar_at(right, i);
                if (is_null_scalar(l_scalar, r_scalar)) {
                    (void)builder.AppendNull();
                    continue;
                }
                bool res = false;
                if (op_ == "&" || op_ == "|") {
                    res = evaluate_boolean_logic(op_, l_scalar, r_scalar);
                }
                else if (l_scalar->type->id() == arrow::Type::STRING || l_scalar->type->id() == arrow::Type::LARGE_STRING || r_scalar->type->id() == arrow::Type::STRING || r_scalar->type->id() == arrow::Type::LARGE_STRING) {
                    res = evaluate_string_cmp(op_, l_scalar, r_scalar);
                }
                else if (l_scalar->type->id() == arrow::Type::BOOL || r_scalar->type->id() == arrow::Type::BOOL) {
                    res = evaluate_bool_cmp(op_, l_scalar, r_scalar);
                }
                else {
                    res = evaluate_numeric_cmp(op_, left, right, i);
                }
                (void)builder.Append(res);
            }
            std::shared_ptr<arrow::Array> out;
            (void)builder.Finish(&out);
            return arrow::Datum(out);
        }
        arrow::DoubleBuilder builder;

        for (int64_t i = 0; i < n; i++) {
            auto l_scalar = get_scalar_at(left, i);
            auto r_scalar = get_scalar_at(right, i);
            if (is_null_scalar(l_scalar, r_scalar)) {
                (void)builder.AppendNull();
                continue;
            }
            if (l_scalar->type->id() == arrow::Type::STRING ||
                r_scalar->type->id() == arrow::Type::STRING) {
                throw std::invalid_argument("Cannot perform arithmetic on string");
            }
            double lv = get_value_at(left, i);
            double rv = get_value_at(right, i);
            double res = 0;
            if (!evaluate_arithmetic(op_, lv, rv, res)) {
                (void)builder.AppendNull();
                continue;
            }
            (void)builder.Append(res);
        }
        std::shared_ptr<arrow::Array> out;
        (void)builder.Finish(&out);
        return arrow::Datum(out);
    }
    static std::pair<std::shared_ptr<arrow::Array>, int64_t> locate_chunk(const std::shared_ptr<arrow::ChunkedArray>& arr, int64_t i) {
        if (!arr) return {nullptr, -1};
        int64_t idx = i;
        for (int c = 0; c < arr->num_chunks(); ++c) {
            auto chunk = arr->chunk(c);
            if (idx < chunk->length()) return {chunk, idx};
            idx -= chunk->length();
        }
        return {nullptr, -1};
    }

    static void process_string_array( const std::shared_ptr<arrow::ChunkedArray>& arr, std::function<void(const std::string_view*)> fn ) {
        auto type_id = arr->type()->id();
        for (int c = 0; c < arr->num_chunks(); c++) {
            auto chunk = arr->chunk(c);

            if (type_id == arrow::Type::STRING) {
                auto str_arr = std::static_pointer_cast<arrow::StringArray>(chunk);

                for (int64_t i = 0; i < str_arr->length(); i++) {
                    if (str_arr->IsNull(i)) {
                        fn(nullptr);
                    } else {
                        auto view = str_arr->GetView(i);
                        fn(&view);
                    }
                }
            }
            else {
                auto str_arr = std::static_pointer_cast<arrow::LargeStringArray>(chunk);

                for (int64_t i = 0; i < str_arr->length(); i++) {
                    if (str_arr->IsNull(i)) {
                        fn(nullptr);
                    } else {
                        auto view = str_arr->GetView(i);
                        fn(&view);
                    }
                }
            }
        }
    }

    FunctionExpression::FunctionExpression(std::shared_ptr<Expression> child, const std::string& func, const std::string& arg) : child_(child), func_(func), arg_(arg) {}

    std::string FunctionExpression::to_string() const {
        if (arg_.empty()) return child_->to_string() + "." + func_ + "()";
        else return child_->to_string() + "." + func_ + "(\"" + arg_ + "\")";
    }

    arrow::Datum FunctionExpression::evaluate(const std::shared_ptr<arrow::Table>& table) const {
        auto input = child_->evaluate(table);
        if (func_ == "abs") {
            std::shared_ptr<arrow::ChunkedArray> arr;
            if (input.kind() == arrow::Datum::CHUNKED_ARRAY) {
                arr = input.chunked_array();
            } else if (input.kind() == arrow::Datum::ARRAY) {
                arr = std::make_shared<arrow::ChunkedArray>(input.make_array());
            } else {
                throw std::runtime_error("abs: unexpected datum type");
            }
            auto input_type = arr->type();
            std::unique_ptr<arrow::ArrayBuilder> builder;
            (void)arrow::MakeBuilder(arrow::default_memory_pool(), input_type, &builder);
            
            for (int c = 0; c < arr->num_chunks(); c++) {
                auto chunk = arr->chunk(c);
                for (int64_t i = 0; i < chunk->length(); i++) {
                    auto scalar = chunk->GetScalar(i).ValueOrDie();
                    if (!scalar->is_valid) {
                        (void)builder->AppendNull();
                    } else {
                        switch (input_type->id()) {
                            case arrow::Type::INT32: {
                                auto val = std::static_pointer_cast<arrow::Int32Scalar>(scalar)->value;
                                (void)static_cast<arrow::Int32Builder*>(builder.get())->Append(std::abs(val));
                                break;
                            }
                            case arrow::Type::INT64: {
                                auto val = std::static_pointer_cast<arrow::Int64Scalar>(scalar)->value;
                                (void)static_cast<arrow::Int64Builder*>(builder.get())->Append(std::abs(val));
                                break;
                            }
                            case arrow::Type::FLOAT: {
                                auto val = std::static_pointer_cast<arrow::FloatScalar>(scalar)->value;
                                (void)static_cast<arrow::FloatBuilder*>(builder.get())->Append(std::fabs(val));
                                break;
                            }
                            case arrow::Type::DOUBLE: {
                                auto val = std::static_pointer_cast<arrow::DoubleScalar>(scalar)->value;
                                (void)static_cast<arrow::DoubleBuilder*>(builder.get())->Append(std::fabs(val));
                                break;
                            }
                            default:
                                (void)builder->AppendNull();
                        }
                    }
                }
            }
            
            std::shared_ptr<arrow::Array> out;
            auto finish_status = builder->Finish(&out);
            if (!finish_status.ok()) {
                throw std::runtime_error("abs: Failed to finish array: " + finish_status.ToString());
            }
            return arrow::Datum(out);
        }

        std::shared_ptr<arrow::ChunkedArray> arr;
        if (input.is_chunked_array()) {
            arr = input.chunked_array();
        } else {
            throw std::runtime_error("FunctionExpression expects column input");
        }
        
        auto type_id = arr->type()->id();
        bool is_string = (type_id == arrow::Type::STRING || type_id == arrow::Type::LARGE_STRING);

        if (!is_string) {
            throw std::invalid_argument(func_ + "() only valid on string columns");
        }
        if (func_ == "length") {
            arrow::Int32Builder builder;

            process_string_array(arr, [&](const std::string_view* val) {
                if (!val) (void) builder.AppendNull();
                else (void) builder.Append(static_cast<int32_t>(val->size()));
            });

            std::shared_ptr<arrow::Array> out;
            (void) builder.Finish(&out);
            return arrow::Datum(out);
        }
        else if (func_ == "contains") {
            arrow::BooleanBuilder builder;

            process_string_array(arr, [&](const std::string_view* val) {
                if (!val) (void) builder.AppendNull();
                else (void) builder.Append(val->find(arg_) != std::string_view::npos);
            });

            std::shared_ptr<arrow::Array> out;
            (void) builder.Finish(&out);
            return arrow::Datum(out);
        }
        else if (func_ == "starts_with") {
            arrow::BooleanBuilder builder;

            process_string_array(arr, [&](const std::string_view* val) {
                if (!val) (void) builder.AppendNull();
                else (void) builder.Append(
                    val->size() >= arg_.size() &&
                    val->substr(0, arg_.size()) == arg_
                );
            });

            std::shared_ptr<arrow::Array> out;
            (void) builder.Finish(&out);
            return arrow::Datum(out);
        }
        else if (func_ == "ends_with") {
            arrow::BooleanBuilder builder;

            process_string_array(arr, [&](const std::string_view* val) {
                if (!val) (void) builder.AppendNull();
                else (void) builder.Append(
                    val->size() >= arg_.size() &&
                    val->substr(val->size() - arg_.size()) == arg_
                );
            });

            std::shared_ptr<arrow::Array> out;
            (void) builder.Finish(&out);
            return arrow::Datum(out);
        }
        else if (func_ == "to_lower") {
            arrow::StringBuilder builder;

            process_string_array(arr, [&](const std::string_view* val) {
                if (!val) {
                    (void) builder.AppendNull();
                } else {
                    std::string s(val->begin(), val->end());
                    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                    (void) builder.Append(s);
                }
            });

            std::shared_ptr<arrow::Array> out;
            (void) builder.Finish(&out);
            return arrow::Datum(out);
        }
        else if (func_ == "to_upper") {
            arrow::StringBuilder builder;

            process_string_array(arr, [&](const std::string_view* val) {
                if (!val) {
                    (void) builder.AppendNull();
                } else {
                    std::string s(val->begin(), val->end());
                    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
                    (void) builder.Append(s);
                }
            });

            std::shared_ptr<arrow::Array> out;
            (void) builder.Finish(&out);
            return arrow::Datum(out);
        }
        throw std::runtime_error("Unknown string function: " + func_);
    }

    std::shared_ptr<Expression> FunctionExpression::getChild() const { return child_; }

    const std::string& FunctionExpression::getFunc() const { return func_; }

    const std::string& FunctionExpression::getArg() const { return arg_; }


    //Aggregation Expression
    AggregationExpression::AggregationExpression(std::shared_ptr<Expression> name, std::string op) : column_name_(name), op_(op) {}

    std::string AggregationExpression::to_string() const { return column_name_->to_string() + "." + op_ + "()"; }

    arrow::Datum AggregationExpression::evaluate(const std::shared_ptr<arrow::Table>& table) const {
        auto data = column_name_->evaluate(table);
        int64_t n = table->num_rows();
        
        // Get the input type
        std::shared_ptr<arrow::DataType> input_type;
        if (data.is_chunked_array()) {
            input_type = data.chunked_array()->type();
        } else if (data.is_array()) {
            input_type = data.make_array()->type();
        }
        
        auto type_id = input_type->id(); 
        if (op_ == "count") {
            int64_t count = 0;
            for (int64_t i = 0; i < n; i++) {
                auto s = get_scalar_at(data, i);
                if (s && s->is_valid) count++;
            }
            return arrow::Datum(arrow::MakeScalar(count));
        }
        
        // For numeric types (int32, int64, float32, float64, bool)
        if (type_id == arrow::Type::INT32 || type_id == arrow::Type::INT64 ||
            type_id == arrow::Type::FLOAT || type_id == arrow::Type::DOUBLE ||
            type_id == arrow::Type::BOOL) {
            
            double sum = 0;
            int64_t count = 0;
            double min_val = std::numeric_limits<double>::infinity();
            double max_val = -std::numeric_limits<double>::infinity();
            
            for (int64_t i = 0; i < n; i++) {
                auto s = get_scalar_at(data, i);
                if (!s || !s->is_valid) continue;
                count++;
                
                double v = BinaryExpression::get_scalar_double(s);
                sum += v;
                if (v < min_val) min_val = v;
                if (v > max_val) max_val = v;
            }
            
            if (count == 0) {
                // Return null or default based on type
                if (op_ == "sum" || op_ == "mean") return arrow::Datum(arrow::MakeScalar(0.0));
                return arrow::Datum(arrow::MakeNullScalar(input_type));
            }
            
            if (op_ == "sum") {
                if (type_id == arrow::Type::INT32) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<int32_t>(sum)));
                } else if (type_id == arrow::Type::INT64) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<int64_t>(sum)));
                } else if (type_id == arrow::Type::FLOAT) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<float>(sum)));
                } else if (type_id == arrow::Type::DOUBLE) {
                    return arrow::Datum(arrow::MakeScalar(sum));
                } else if (type_id == arrow::Type::BOOL) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<bool>(sum)));
                }
            }
            
            if (op_ == "mean") {
                return arrow::Datum(arrow::MakeScalar(sum / count));
            }
            
            if (op_ == "min") {
                // Preserve exact input type
                if (type_id == arrow::Type::INT32) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<int32_t>(min_val)));
                } else if (type_id == arrow::Type::INT64) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<int64_t>(min_val)));
                } else if (type_id == arrow::Type::FLOAT) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<float>(min_val)));
                } else if (type_id == arrow::Type::DOUBLE) {
                    return arrow::Datum(arrow::MakeScalar(min_val));
                } else if (type_id == arrow::Type::BOOL) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<bool>(min_val)));
                }
            }
            
            if (op_ == "max") {
                // Preserve exact input type
                if (type_id == arrow::Type::INT32) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<int32_t>(max_val)));
                } else if (type_id == arrow::Type::INT64) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<int64_t>(max_val)));
                } else if (type_id == arrow::Type::FLOAT) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<float>(max_val)));
                } else if (type_id == arrow::Type::DOUBLE) {
                    return arrow::Datum(arrow::MakeScalar(max_val));
                } else if (type_id == arrow::Type::BOOL) {
                    return arrow::Datum(arrow::MakeScalar(static_cast<bool>(max_val)));
                }
            }
        }
        
        // For string types
        if (type_id == arrow::Type::STRING || type_id == arrow::Type::LARGE_STRING) {
            if (op_ == "sum" || op_ == "mean") {
                throw std::invalid_argument("Cannot compute " + op_ + " on string column");
            }
            
            std::string min_str;
            std::string max_str;
            bool first = true;
            
            for (int64_t i = 0; i < n; i++) {
                auto s = get_scalar_at(data, i);
                if (!s || !s->is_valid) continue;
                
                std::string val = s->ToString();
                if (first) {
                    min_str = max_str = val;
                    first = false;
                } else {
                    if (val < min_str) min_str = val;
                    if (val > max_str) max_str = val;
                }
            }
            
            if (first) {
                // No valid strings
                return arrow::Datum(arrow::MakeNullScalar(arrow::utf8()));
            }
            
            if (op_ == "min") return arrow::Datum(arrow::MakeScalar(min_str));
            if (op_ == "max") return arrow::Datum(arrow::MakeScalar(max_str));
        }
        
        throw std::runtime_error("Unknown aggregation operation: " + op_);
    }

    std::shared_ptr<Expression> AggregationExpression::getChild() const { return column_name_; }

    const std::string& AggregationExpression::getOp() const { return op_; }
}

