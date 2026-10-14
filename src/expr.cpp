#include "../include/dataframelib/expression.h"
#include <arrow/api.h>
#include <vector>
#include <string>
#include <cstdint>
namespace dataframelib {
    Expr::Expr(std::shared_ptr<Expression> p) : ptr(p) {}
    Expr::operator std::shared_ptr<Expression>() const {
        return ptr;
    }
    arrow::Datum Expr::evaluate(const std::shared_ptr<arrow::Table>& table) const {
        return ptr->evaluate(table);
    }
    std::string Expr::to_string() const {
        return ptr->to_string();
    }
    Expr Expr::alias(const std::string& name) const {
        return Expr(std::make_shared<AliasExpression>(ptr, name));
    }
    Expr Expr::sum() const {
        return Expr(std::make_shared<AggregationExpression>(ptr, "sum"));
    }
    Expr Expr::mean() const {
        return Expr(std::make_shared<AggregationExpression>(ptr, "mean"));
    }
    Expr Expr::count() const {
        return Expr(std::make_shared<AggregationExpression>(ptr, "count"));
    }
    Expr Expr::min() const {
        return Expr(std::make_shared<AggregationExpression>(ptr, "min"));
    }
    Expr Expr::max() const {
        return Expr(std::make_shared<AggregationExpression>(ptr, "max"));
    }
    Expr Expr::length() const {
        return Expr(std::make_shared<FunctionExpression>(ptr, "length"));
    }
    Expr Expr::contains(const std::string& s) const {
        return Expr(std::make_shared<FunctionExpression>(ptr, "contains", s));
    }
    Expr Expr::starts_with(const std::string& s) const {
        return Expr(std::make_shared<FunctionExpression>(ptr, "starts_with", s));
    }
    Expr Expr::ends_with(const std::string& s) const {
        return Expr(std::make_shared<FunctionExpression>(ptr, "ends_with", s));
    }
    Expr Expr::to_lower() const {
        return Expr(std::make_shared<FunctionExpression>(ptr, "to_lower"));
    }
    Expr Expr::to_upper() const {
        return Expr(std::make_shared<FunctionExpression>(ptr, "to_upper"));
    }
    Expr Expr::is_null() const {
        return Expr(std::make_shared<UnaryExpression>("is_null", ptr));
    }
    Expr Expr::is_not_null() const {
        return Expr(std::make_shared<UnaryExpression>("is_not_null", ptr));
    }
    Expr Expr::abs() const {
        return Expr(std::make_shared<FunctionExpression>(ptr, "abs"));
    }

    // column
    Expr col(const std::string& name)   {
        return Expr(std::make_shared<ColumnExpression>(name));
    }

    // literals
    Expr lit(int v) {
        return Expr(std::make_shared<LiteralExpression>(static_cast<int64_t>(v)));
    }

    Expr lit(int64_t v) {
        return Expr(std::make_shared<LiteralExpression>(v));
    }

    Expr lit(double v) {
        return Expr(std::make_shared<LiteralExpression>(v));
    }

    Expr lit(bool v) {
        return Expr(std::make_shared<LiteralExpression>(v));
    }

    Expr lit(const std::string& v) {
        return Expr(std::make_shared<LiteralExpression>(v));
    }

    Expr lit(const char* v) {
        return Expr(std::make_shared<LiteralExpression>(std::string(v)));
    }

    // null helpers
    std::shared_ptr<Expression> is_null(std::shared_ptr<Expression> e) {
        return std::make_shared<UnaryExpression>("is_null", e);
    }
    std::shared_ptr<Expression> is_not_null(std::shared_ptr<Expression> e) {
        return std::make_shared<UnaryExpression>("is_not_null", e);
    }
    std::shared_ptr<Expression> not_expr(std::shared_ptr<Expression> e) {
        return std::make_shared<UnaryExpression>("~", e);
    }
    std::shared_ptr<Expression> length(std::shared_ptr<Expression> e) {
        return std::make_shared<FunctionExpression>(e, "length");
    }

    std::shared_ptr<Expression> contains(std::shared_ptr<Expression> e, const std::string& s) {
        return std::make_shared<FunctionExpression>(e, "contains", s);
    }
    std::shared_ptr<Expression> alias(std::shared_ptr<Expression> e, const std::string& name) {
        return std::make_shared<AliasExpression>(e, name);
    }


    // _to_expr
    Expr _to_expr(Expr e) { return e; }

    Expr _to_expr(std::shared_ptr<Expression> e) {
        return Expr(e);
    }

    Expr _to_expr(bool v) { return lit(v); }
    Expr _to_expr(int v) { return lit(v); }
    Expr _to_expr(int64_t v) { return lit(v); }
    Expr _to_expr(double v) { return lit(v); }
    Expr _to_expr(const char* v) { return lit(std::string(v)); }
    Expr _to_expr(const std::string& v) { return lit(v); }
}