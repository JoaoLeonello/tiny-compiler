#pragma once

#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

struct CompileError : std::runtime_error {
    int line;
    CompileError(int line, const std::string& msg) : std::runtime_error(msg), line(line) {}
};

enum class Tok {
    Int, Bool, Void, If, Else, While, Return, True, False,
    Ident, Number,
    LParen, RParen, LBrace, RBrace, Comma, Semi, Assign,
    Plus, Minus, Star, Slash, Percent, Not, AndAnd, OrOr,
    Eq, Ne, Lt, Le, Gt, Ge,
    End
};

struct Token {
    Tok kind;
    std::string text;
    long value;
    int line;
};

const char* tok_name(Tok kind);
std::vector<Token> lex(const std::string& source);

enum class Type { Int, Bool, Void };
const char* type_name(Type type);

struct Expr {
    enum Kind { Num, BoolLit, Var, Unary, Binary, Call } kind;
    int line = 0;
    long value = 0;
    std::string name;
    Tok op = Tok::End;
    std::vector<std::unique_ptr<Expr>> kids;
    Type type = Type::Void;
};

struct Stmt {
    enum Kind { Decl, Assign, If, While, Return, ExprStmt, Block } kind;
    int line = 0;
    Type type = Type::Void;
    std::string name;
    std::unique_ptr<Expr> expr;
    std::vector<std::unique_ptr<Stmt>> body;
};

struct Param {
    Type type;
    std::string name;
};

struct Function {
    Type ret = Type::Void;
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<Stmt> body;
    int line = 0;
};

struct Program {
    std::vector<Function> funcs;
};

Program parse(const std::vector<Token>& tokens);
void dump_ast(const Program& program, std::ostream& out);

void check(Program& program);

struct Operand {
    bool is_const = false;
    long value = 0;
    std::string name;

    static Operand constant(long v) { Operand o; o.is_const = true; o.value = v; return o; }
    static Operand var(std::string n) { Operand o; o.name = std::move(n); return o; }
    bool empty() const { return !is_const && name.empty(); }
};

enum class Op {
    Copy, Neg, Not,
    Add, Sub, Mul, Div, Mod,
    Eq, Ne, Lt, Le, Gt, Ge,
    Label, Jump, JumpIfZero, Call, Ret, Nop
};

struct Instr {
    Op op;
    std::string dst;
    Operand a, b;
    std::string label;
    std::vector<Operand> args;
};

struct IRFunc {
    std::string name;
    std::vector<std::string> params;
    std::vector<Instr> code;
};

std::vector<IRFunc> gen_ir(const Program& program);
void dump_ir(const std::vector<IRFunc>& funcs, std::ostream& out);

void optimize(std::vector<IRFunc>& funcs);

void gen_asm(const std::vector<IRFunc>& funcs, std::ostream& out);
