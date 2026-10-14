
#ifndef EAGERDATAFRAME_H
#define EAGERDATAFRAME_H

#include <arrow/api.h>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <string>
#include "dataframelib/expression.h"
namespace dataframelib {
    class GroupedDataFrame;

    class EagerDataFrame {
    private:
        std::shared_ptr<arrow::Table> table_; // everything is managed in a arrow table 

    public:
        //constructor
        EagerDataFrame(std::shared_ptr<arrow::Table> data);
        //get table
        std::shared_ptr<arrow::Table> getTable() const; 
        //make comparator
        std::function<bool(int64_t, int64_t)> make_comparator( const std::vector<std::shared_ptr<arrow::ChunkedArray>>& cols, bool asc) const;
        //read csv
        static EagerDataFrame read_csv(const std::string& path); // the address of the input file is given return a data frame no need to create a dataframe 
        //read parquet
        static EagerDataFrame read_parquet(const std::string& path);
        //write csv
        EagerDataFrame& write_csv(const std::string& path); // convert the table in to the file and write back in to the file in the csv or the parquet fromat 
        //write parquet
        EagerDataFrame& write_parquet(const std::string& path);
        //from columns
        static EagerDataFrame from_columns( const std::map<std::string, std::shared_ptr<arrow::Array>>& map);
        //from columns using vector of pairs
        //select
        EagerDataFrame select(const std::vector<std::string>& colnames);
        //filter
        EagerDataFrame filter(const std::shared_ptr<Expression>& expr);
        //with column
        EagerDataFrame with_column(const std::string& name, const std::shared_ptr<Expression>& expr);
        //sort
        EagerDataFrame sort(const std::vector<std::string>& columns, bool asc);
        //head
        EagerDataFrame head(int n);
        //join
        EagerDataFrame join(const EagerDataFrame& other, const std::vector<std::string>& on, const std::string& how);
        //group by
        GroupedDataFrame group_by(const std::vector<std::string>& keys);
        //print
        void print() const; // this is used for printing 
        //num rows
        int64_t num_rows() const;
        //num columns
        int num_columns() const;
    };

    //grouped data frame
    class GroupedDataFrame {
    private:
        std::shared_ptr<arrow::Table> table_;
        std::vector<std::string> keys_;
    public:
        GroupedDataFrame( const std::shared_ptr<arrow::Table>& table, const std::vector<std::string>& keys);
        //aggregate using ordered map
        EagerDataFrame aggregate( const std::map<std::string, std::shared_ptr<Expression>>& agg_map);
        //aggregate using unordered map
        EagerDataFrame aggregate(const std::unordered_map<std::string, std::shared_ptr<Expression>>& agg_map);
        //aggregate using vector of pairs (for simple aggregations)
        EagerDataFrame aggregate(const std::vector<std::pair<std::string, std::string>>& agg_vec);
        //aggregate using initializer list
        inline EagerDataFrame aggregate(std::initializer_list<std::pair<const std::string, std::shared_ptr<Expression>>> list) {
            std::map<std::string, std::shared_ptr<Expression>> m(list.begin(), list.end());
            return aggregate(m);
        }
    };

    //read csv for user input
    EagerDataFrame read_csv(const std::string& path);
    //read parquet for user input
    EagerDataFrame read_parquet(const std::string& path);


    // From columns using vector (for autograder compatibility)
    EagerDataFrame from_columns(
        const std::vector<std::pair<std::string, std::shared_ptr<arrow::Array>>>& vec
    );
    //from columns using ordered map
    EagerDataFrame from_columns(const std::map<std::string, std::shared_ptr<arrow::Array>>& map);
    //from columns using unordered map
    inline EagerDataFrame from_columns(const std::unordered_map<std::string, std::shared_ptr<arrow::Array>>& umap) {
        std::map<std::string, std::shared_ptr<arrow::Array>> m(umap.begin(), umap.end());
        return from_columns(m);
    }
    //from columns using initializer list
    inline EagerDataFrame from_columns(std::initializer_list<std::pair<const std::string, std::shared_ptr<arrow::Array>>> list) {
        std::map<std::string, std::shared_ptr<arrow::Array>> m(list.begin(), list.end());
        return from_columns(m);
    }
}

#endif
