
#ifndef LAZYDATAFRAME_H
#define LAZYDATAFRAME_H

#include "graph.h"
#include "dataframelib/eagerdataframe.h"
#include "dataframelib/expression.h"
#include <memory>
#include <cstdint>
#include <map>
namespace dataframelib {
    // class for lazy dataframe
    class LazyDataFrame {
    private:
        std::shared_ptr<Node> plan_root_;
    public:
        //constructor
        LazyDataFrame(std::shared_ptr<Node> plan)  : plan_root_(plan) {}
        //scan csv
        static LazyDataFrame scan_csv(const std::string& path);
        //scan parquet
        static LazyDataFrame scan_parquet(const std::string& path);
        //filter
        LazyDataFrame filter(const std::shared_ptr<Expression>& expr);
        //select
        LazyDataFrame select(const std::vector<std::string>& colnames);
        //with column
        LazyDataFrame with_column(const std::string& name, const std::shared_ptr<Expression>& expr);
        //sort
        LazyDataFrame sort(const std::vector<std::string>& columns, bool asc);
        //head
        LazyDataFrame head(int n);  
        //join
        LazyDataFrame join(const LazyDataFrame& other, const std::vector<std::string>& on, const std::string& join_type);
        //group by
        LazyDataFrame group_by(const std::vector<std::string>& keys);
        //aggregate using ordered map
        LazyDataFrame aggregate(const std::map<std::string, std::shared_ptr<Expression>>& agg_map);
        //aggregate using unordered map
        LazyDataFrame aggregate(const std::unordered_map<std::string, std::shared_ptr<Expression>>& agg_map);
        //aggregate using vector of pairs
        LazyDataFrame aggregate(const std::vector<std::pair<std::string, std::string>>& agg_vec);
        //aggregate using initializer list
        LazyDataFrame aggregate(std::initializer_list<std::pair<const std::string, std::shared_ptr<Expression>>> list) {
            std::map<std::string, std::shared_ptr<Expression>> m(list.begin(), list.end());
            return aggregate(m);
        }
        //collect
        EagerDataFrame collect();
        //explain to consol
        
        //explain to path
        void explain(const std::string& path);

        //sink csv
        void sink_csv(const std::string& path) {
            collect().write_csv(path);
        }
        //sink parquet
        void sink_parquet(const std::string& path) {
            collect().write_parquet(path);
        }
        //get plan root
        std::shared_ptr<Node> getPlanRoot() {
            return plan_root_;
        }
    };

    //scan csv for user input
    inline LazyDataFrame scan_csv(const std::string& path) {
        return LazyDataFrame::scan_csv(path);
    }

    //scan parquet for user input
    inline LazyDataFrame scan_parquet(const std::string& path) {
        return LazyDataFrame::scan_parquet(path);
    }
}
#endif


