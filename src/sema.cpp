#include "compiler.h"

#include <unordered_map>

const char* type_name(Type type) {
    switch (type) {
    case Type::Int: return "int";
    case Type::Bool: return "bool";
    case Type::Void: return "void";
    }
    return "?";
}

namespace {

struct Signature {
    Type ret;
    std::vector<Type> params;
};

struct Symbol {
    Type type;
    std::string unique_name;
};

struct Checker {
    std::unordered_map<std::string, Signature> funcs;
    std::vector<std::unordered_map<std::string, Symbol>> scopes;
    int counter = 0;
    Type current_ret = Type::Void;

    [[noreturn]] void fail(int line, const std::string& msg) { throw CompileError(line, msg); }

    std::string declare(int line, const std::string& name, Type type) {
        if (type == Type::Void) fail(line, "variable '" + name + "' cannot have type void");
        if (scopes.back().count(name)) fail(line, "redeclaration of '" + name + "'");
        std::string unique = name + "." + std::to_string(++counter);
        scopes.back()[name] = {type, unique};
        return unique;
    }

    const Symbol& lookup(int line, const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        fail(line, "undeclared variable '" + name + "'");
    }

    void expect_type(int line, Type got, Type want, const std::string& what) {
        if (got != want)
            fail(line, what + ": expected " + type_name(want) + " but got " + type_name(got));
    }

    Type expr(Expr& e) {
        e.type = infer(e);
        return e.type;
    }

    Type infer(Expr& e) {
        switch (e.kind) {
        case Expr::Num: return Type::Int;
        case Expr::BoolLit: return Type::Bool;
        case Expr::Var: {
            const Symbol& sym = lookup(e.line, e.name);
            e.name = sym.unique_name;
            return sym.type;
        }
        case Expr::Unary: {
            Type operand = expr(*e.kids[0]);
            Type want = e.op == Tok::Minus ? Type::Int : Type::Bool;
            expect_type(e.line, operand, want, std::string("operand of '") + tok_name(e.op) + "'");
            return want;
        }
        case Expr::Binary: {
            Type l = expr(*e.kids[0]);
            Type r = expr(*e.kids[1]);
            std::string what = std::string("operand of '") + tok_name(e.op) + "'";
            switch (e.op) {
            case Tok::AndAnd:
            case Tok::OrOr:
                expect_type(e.line, l, Type::Bool, what);
                expect_type(e.line, r, Type::Bool, what);
                return Type::Bool;
            case Tok::Eq:
            case Tok::Ne:
                if (l == Type::Void) fail(e.line, "cannot compare void values");
                expect_type(e.line, r, l, what);
                return Type::Bool;
            case Tok::Lt: case Tok::Le: case Tok::Gt: case Tok::Ge:
                expect_type(e.line, l, Type::Int, what);
                expect_type(e.line, r, Type::Int, what);
                return Type::Bool;
            default:
                expect_type(e.line, l, Type::Int, what);
                expect_type(e.line, r, Type::Int, what);
                return Type::Int;
            }
        }
        case Expr::Call: {
            auto it = funcs.find(e.name);
            if (it == funcs.end()) fail(e.line, "call to undeclared function '" + e.name + "'");
            const Signature& sig = it->second;
            if (sig.params.size() != e.kids.size())
                fail(e.line, "function '" + e.name + "' expects " + std::to_string(sig.params.size()) +
                                 " argument(s) but got " + std::to_string(e.kids.size()));
            for (size_t i = 0; i < e.kids.size(); i++)
                expect_type(e.line, expr(*e.kids[i]), sig.params[i],
                            "argument " + std::to_string(i + 1) + " of '" + e.name + "'");
            return sig.ret;
        }
        }
        return Type::Void;
    }

    void scoped(Stmt& s) {
        scopes.emplace_back();
        stmt(s);
        scopes.pop_back();
    }

    void stmt(Stmt& s) {
        switch (s.kind) {
        case Stmt::Decl:
            if (s.expr) expect_type(s.line, expr(*s.expr), s.type, "initializer of '" + s.name + "'");
            s.name = declare(s.line, s.name, s.type);
            break;
        case Stmt::Assign: {
            const Symbol& sym = lookup(s.line, s.name);
            expect_type(s.line, expr(*s.expr), sym.type, "assignment to '" + s.name + "'");
            s.name = sym.unique_name;
            break;
        }
        case Stmt::If:
        case Stmt::While:
            expect_type(s.line, expr(*s.expr), Type::Bool, "condition");
            for (auto& child : s.body) scoped(*child);
            break;
        case Stmt::Return:
            if (!s.expr) {
                if (current_ret != Type::Void) fail(s.line, "non-void function must return a value");
            } else {
                if (current_ret == Type::Void) fail(s.line, "void function cannot return a value");
                expect_type(s.line, expr(*s.expr), current_ret, "return value");
            }
            break;
        case Stmt::ExprStmt:
            expr(*s.expr);
            break;
        case Stmt::Block:
            scopes.emplace_back();
            for (auto& child : s.body) stmt(*child);
            scopes.pop_back();
            break;
        }
    }

    void program(Program& prog) {
        funcs["print"] = {Type::Void, {Type::Int}};
        funcs["read"] = {Type::Int, {}};

        for (auto& f : prog.funcs) {
            if (funcs.count(f.name)) fail(f.line, "redefinition of function '" + f.name + "'");
            if (f.params.size() > 6) fail(f.line, "functions support at most 6 parameters");
            Signature sig{f.ret, {}};
            for (auto& param : f.params) sig.params.push_back(param.type);
            funcs[f.name] = sig;
        }

        auto main_it = funcs.find("main");
        if (main_it == funcs.end()) fail(1, "program has no 'main' function");
        if (main_it->second.ret != Type::Int || !main_it->second.params.empty())
            fail(1, "'main' must be declared as 'int main()'");

        for (auto& f : prog.funcs) {
            scopes.assign(1, {});
            current_ret = f.ret;
            for (auto& param : f.params) param.name = declare(f.line, param.name, param.type);
            stmt(*f.body);
        }
    }
};

}

void check(Program& program) {
    Checker checker;
    checker.program(program);
}
