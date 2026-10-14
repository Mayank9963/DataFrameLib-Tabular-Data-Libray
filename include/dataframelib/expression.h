
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <string>
#include <memory>
#include <variant>
#include <type_traits>
#include <arrow/api.h>
#include <vector>
#include <cstdint>
#include <type_traits>
namespace dataframelib {
    class BinaryExpression;
    class UnaryExpression;
    class FunctionExpression;
    class AggregationExpression;
    class AliasExpression;

    //base class 
    class Expression {
    public:
        virtual ~Expression() = default;
        virtual std::string to_string() const = 0;
        virtual arrow::Datum evaluate(const std::shared_ptr<arrow::Table>& table) const = 0;
    };

    //column name is given and value is in the dataframe
    class ColumnExpression : public Expression {
    private:
        std::string column_name_;
    public:
        ColumnExpression(const std::string& name);
        std::string to_string() const override;
        arrow::Datum evaluate(const std::shared_ptr<arrow::Table>& table) const override;
        const std::string& getColumnName() const;
    };

    // a constant value that we give in the expression 
    class LiteralExpression : public Expression {
    private:
        std::variant<int64_t, double, std::string, bool> value_;
    public:
        LiteralExpression(const std::variant<int64_t, double, std::string, bool>& val);
        std::string to_string() const override;
        arrow::Datum evaluate(const std::shared_ptr<arrow::Table>& table) const override;
        const std::variant<int64_t, double, std::string, bool>& getValue() const;
    };

    // give name to the expression 
    class AliasExpression : public Expression {
    private:
        std::shared_ptr<Expression> child_;
        std::string name_;
    public:
        AliasExpression(std::shared_ptr<Expression> child, const std::string& name);
        std::string to_string() const override;
        arrow::Datum evaluate(const std::shared_ptr<arrow::Table>& table) const override;
        const std::string& getName() const;
        std::shared_ptr<Expression> getChild() const;
    };

    // unary expression
    class UnaryExpression : public Expression {
    private:
        std::string op_;
        std::shared_ptr<Expression> child_;
    public:
        UnaryExpression(const std::string& op, std::shared_ptr<Expression> child);
        std::string to_string() const override;
        arrow::Datum evaluate(const std::shared_ptr<arrow::Table>& table) const override;
        std::shared_ptr<Expression> getChild() const;
        const std::string& getOp() const;
    };

    // binary expression 
    class BinaryExpression : public Expression {
    private:
        std::shared_ptr<Expression> leftside_;
        std::string op_;
        std::shared_ptr<Expression> rightside_;
    public:
        BinaryExpression(std::shared_ptr<Expression> left, const std::string& op, std::shared_ptr<Expression> right);
        std::string to_string() const override;
        arrow::Datum evaluate(const std::shared_ptr<arrow::Table>& table) const override;
        static double get_scalar_double(const std::shared_ptr<arrow::Scalar>& s);
        static double get_value_at(const arrow::Datum& d, int64_t i);
        std::shared_ptr<Expression> left() const { return leftside_; }
        std::shared_ptr<Expression> right() const { return rightside_; }
        std::string op() const { return op_; }  
    };

    // function expression
    class FunctionExpression : public Expression {
    private:
        std::shared_ptr<Expression> child_;
        std::string func_;   // "length", "contains", etc.
        std::string arg_;    // for contains, starts_with, etc.

    public:
        FunctionExpression(std::shared_ptr<Expression> child, const std::string& func, const std::string& arg = "");
        std::string to_string() const override;
        arrow::Datum evaluate(const std::shared_ptr<arrow::Table>& table) const override;
        std::shared_ptr<Expression> getChild() const;
        const std::string& getFunc() const;
        const std::string& getArg() const;
    };

    // aggregation expression 
    class AggregationExpression : public Expression {
    private:
        std::shared_ptr<Expression> column_name_;
        std::string op_;
    public:
        AggregationExpression(std::shared_ptr<Expression> name, std::string op);
        std::string to_string() const override;
        arrow::Datum evaluate(const std::shared_ptr<arrow::Table>& table) const override;
        std::shared_ptr<Expression> getChild() const;
        const std::string& getOp() const;
    };

    class Expr {
    public:
        std::shared_ptr<Expression> ptr;
        Expr(std::shared_ptr<Expression> p);
        operator std::shared_ptr<Expression>() const;
        arrow::Datum evaluate(const std::shared_ptr<arrow::Table>& table) const;
        std::string to_string() const;
        Expr alias(const std::string& name) const ; 
        Expr sum() const;
        Expr mean() const;
        Expr count() const;
        Expr min() const;
        Expr max() const;
        Expr length() const;
        Expr contains(const std::string& s) const;
        Expr starts_with(const std::string& s) const;
        Expr ends_with(const std::string& s) const;
        Expr to_lower() const;
        Expr to_upper() const;
        Expr is_null() const;
        Expr is_not_null() const;
        Expr abs() const;
    };

    // declarations only
    Expr col(const std::string& name);
    Expr lit(int v);
    Expr lit(int64_t v);
    Expr lit(double v);
    Expr lit(bool v);
    Expr lit(const std::string& v);
    Expr lit(const char* v);

    std::shared_ptr<Expression> is_null(std::shared_ptr<Expression> e);
    std::shared_ptr<Expression> is_not_null(std::shared_ptr<Expression> e);
    std::shared_ptr<Expression> not_expr(std::shared_ptr<Expression> e);
    std::shared_ptr<Expression> length(std::shared_ptr<Expression> e);
    std::shared_ptr<Expression> contains(std::shared_ptr<Expression> e, const std::string& s);
    std::shared_ptr<Expression> alias(std::shared_ptr<Expression> e, const std::string& name);

    // helper
    Expr _to_expr(Expr e);
    Expr _to_expr(std::shared_ptr<Expression> e);
    Expr _to_expr(bool v);
    Expr _to_expr(int v);
    Expr _to_expr(int64_t v);
    Expr _to_expr(double v);
    Expr _to_expr(const char* v);
    Expr _to_expr(const std::string& v);

    //this is our macro for binary expression this is need so that position of left and write ooperation does not matter 
    inline Expr make_binary(const Expr& l, const std::string& op, const Expr& r) {
        return Expr(std::make_shared<BinaryExpression>(l, op, r));
    }
    // +
    inline Expr operator+(const Expr& l, const Expr& r) { return make_binary(l, "+", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator+(const Expr& l, T r) { return make_binary(l, "+", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator+(T l, const Expr& r) { return make_binary(_to_expr(l), "+", r); }

    // -
    inline Expr operator-(const Expr& l, const Expr& r) { return make_binary(l, "-", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator-(const Expr& l, T r) { return make_binary(l, "-", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator-(T l, const Expr& r) { return make_binary(_to_expr(l), "-", r); }

    // *
    inline Expr operator*(const Expr& l, const Expr& r) { return make_binary(l, "*", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator*(const Expr& l, T r) { return make_binary(l, "*", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator*(T l, const Expr& r) { return make_binary(_to_expr(l), "*", r); }

    // /
    inline Expr operator/(const Expr& l, const Expr& r) { return make_binary(l, "/", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator/(const Expr& l, T r) { return make_binary(l, "/", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator/(T l, const Expr& r) { return make_binary(_to_expr(l), "/", r); }

    // %
    inline Expr operator%(const Expr& l, const Expr& r) { return make_binary(l, "%", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator%(const Expr& l, T r) { return make_binary(l, "%", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator%(T l, const Expr& r) { return make_binary(_to_expr(l), "%", r); }
    // >
    inline Expr operator>(const Expr& l, const Expr& r) { return make_binary(l, ">", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator>(const Expr& l, T r) { return make_binary(l, ">", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator>(T l, const Expr& r) { return make_binary(_to_expr(l), ">", r); }

    // <
    inline Expr operator<(const Expr& l, const Expr& r) { return make_binary(l, "<", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator<(const Expr& l, T r) { return make_binary(l, "<", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator<(T l, const Expr& r) { return make_binary(_to_expr(l), "<", r); }

    // >=
    inline Expr operator>=(const Expr& l, const Expr& r) { return make_binary(l, ">=", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator>=(const Expr& l, T r) { return make_binary(l, ">=", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator>=(T l, const Expr& r) { return make_binary(_to_expr(l), ">=", r); }

    // <=
    inline Expr operator<=(const Expr& l, const Expr& r) { return make_binary(l, "<=", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator<=(const Expr& l, T r) { return make_binary(l, "<=", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator<=(T l, const Expr& r) { return make_binary(_to_expr(l), "<=", r); }

    // ==
    inline Expr operator==(const Expr& l, const Expr& r) { return make_binary(l, "==", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator==(const Expr& l, T r) { return make_binary(l, "==", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator==(T l, const Expr& r) { return make_binary(_to_expr(l), "==", r); }

    // !=
    inline Expr operator!=(const Expr& l, const Expr& r) { return make_binary(l, "!=", r); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator!=(const Expr& l, T r) { return make_binary(l, "!=", _to_expr(r)); }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator!=(T l, const Expr& r) { return make_binary(_to_expr(l), "!=", r); }

    // &
    inline Expr operator&(const Expr& l, const Expr& r) {
        return make_binary(l, "&", r);
    }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator&(const Expr& l, T r) {
        return make_binary(l, "&", _to_expr(r));
    }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator&(T l, const Expr& r) {
        return make_binary(_to_expr(l), "&", r);
    }

    // |
    inline Expr operator|(const Expr& l, const Expr& r) {
        return make_binary(l, "|", r);
    }
    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator|(const Expr& l, T r) {
        return make_binary(l, "|", _to_expr(r));
    }

    template<typename T, typename = std::enable_if_t<!std::is_same_v<T, Expr>>>
    inline Expr operator|(T l, const Expr& r) {
        return make_binary(_to_expr(l), "|", r);
    }

    // ~
    inline Expr operator~(const Expr& e) {
        return Expr(std::make_shared<UnaryExpression>("~", e.ptr));
    }
}
#endif



