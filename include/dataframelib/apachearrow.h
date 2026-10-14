#ifndef APACHEARROW_H
#define APACHEARROW_H

#include <arrow/api.h> // arroy table, array, chunked array, scalar
#include <memory>
#include <vector>
#include <cstdint>
#include <functional>
#include <cstdint>

namespace dataframelib {

    std::shared_ptr<arrow::Table> manual_take_table(const std::shared_ptr<arrow::Table>& table, const std::vector<int64_t>& indices); // function declaration

}

#endif