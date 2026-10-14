#include "../include/dataframelib/apachearrow.h"
#include <arrow/api.h>
#include <vector>
#include <memory>
#include <cstdint>

namespace dataframelib {

    // manual taking of rows from table
    std::shared_ptr<arrow::Table> manual_take_table(const std::shared_ptr<arrow::Table>& table, const std::vector<int64_t>& indices) {
        std::vector<std::shared_ptr<arrow::ChunkedArray>> new_cols;
        //loop over all columns 
        for (int c = 0; c < table->num_columns(); c++) {
            auto col = table->column(c); // current columns and tes datatype
            auto type = col->type();
            std::unique_ptr<arrow::ArrayBuilder> builder; // making a builder which makes new column
            (void) arrow::MakeBuilder(arrow::default_memory_pool(), type, &builder); // giving what a new column should have
            for (int64_t idx : indices) { // now loop over selected rows 

                auto scalar_res = col->GetScalar(idx); // get value of that row

                // check if value is valid
                if (!scalar_res.ok() || !scalar_res.ValueOrDie()->is_valid) {
                    (void) builder->AppendNull(); // if not valid append null
                    continue;
                }

                auto scalar = scalar_res.ValueOrDie(); // actual scalar value
                auto type_id = type->id(); // type of column

                // type checking using if-else
                if (type_id == arrow::Type::INT64) {
                    (void) static_cast<arrow::Int64Builder*>(builder.get())->Append(static_cast<arrow::Int64Scalar&>(*scalar).value);
                }

                else if (type_id == arrow::Type::INT32) {
                    (void) static_cast<arrow::Int32Builder*>(builder.get())->Append(static_cast<arrow::Int32Scalar&>(*scalar).value);
                }

                else if (type_id == arrow::Type::DOUBLE) {
                    (void) static_cast<arrow::DoubleBuilder*>(builder.get())->Append(static_cast<arrow::DoubleScalar&>(*scalar).value);
                }

                else if (type_id == arrow::Type::FLOAT) {
                    (void) static_cast<arrow::FloatBuilder*>(builder.get())->Append(static_cast<arrow::FloatScalar&>(*scalar).value);
                }

                else if (type_id == arrow::Type::STRING) {
                    (void) static_cast<arrow::StringBuilder*>(builder.get())->Append(static_cast<arrow::StringScalar&>(*scalar).value->ToString());
                }

                else if (type_id == arrow::Type::BOOL) {
                    (void) static_cast<arrow::BooleanBuilder*>(builder.get())->Append(static_cast<arrow::BooleanScalar&>(*scalar).value);
                }

                else {
                    (void)builder->AppendNull();
                }
            }
            std::shared_ptr<arrow::Array> new_arr;
            (void)  builder->Finish(&new_arr);  //converting the builder in tot final array
            new_cols.push_back(std::make_shared<arrow::ChunkedArray>(new_arr)); // adding new array to the list of new columns
        }
        
        return arrow::Table::Make(table->schema(), new_cols); // making a new table with new columns with schema/structure same as of the older table 
    }
}