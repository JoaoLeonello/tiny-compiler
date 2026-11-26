#include "compiler.h"

namespace {

struct Generator {
    IRFunc f;
    int temps = 0;

    std::string temp() { return "%t" + std::to_string(++temps); }

    std::string label() {
        static int labels = 0;
        return ".L" + std::to_string(++labels);
    }

    void emit(Op op, std::string dst = "", Operand a = {}, Operand b = {}) {
        Instr in;
        in.op = op;
        in.dst = std::move(dst);
        in.a = std::move(a);
        in.b = std::move(b);
        f.code.push_back(std::move(in));
    }

    void emit_label(const std::string& name) { f.code.push_back({Op::Label, "", {}, {}, name, {}}); }
    void emit_jump(const std::string& target) { f.code.push_back({Op::Jump, "", {}, {}, target, {}}); }
    void emit_jz(Operand cond, const std::string& target) {
        f.code.push_back({Op::JumpIfZero, "", std::move(cond), {}, target, {}});
    }

    static Op binary_op(Tok tok) {
        switch (tok) {
        case Tok::Plus: return Op::Add;
        case Tok::Minus: return Op::Sub;
        case Tok::Star: return Op::Mul;
        case Tok::Slash: return Op::Div;
        case Tok::Percent: return Op::Mod;
        case Tok::Eq: return Op::Eq;
        case Tok::Ne: return Op::Ne;
        case Tok::Lt: return Op::Lt;
        case Tok::Le: return Op::Le;
        case Tok::Gt: return Op::Gt;
        default: return Op::Ge;
        }
    }

    Operand expr(const Expr& e) {
        switch (e.kind) {
        case Expr::Num:
        case Expr::BoolLit:
            return Operand::constant(e.value);
        case Expr::Var:
            return Operand::var(e.name);
        case Expr::Unary: {
            Operand a = expr(*e.kids[0]);
            std::string dst = temp();
            emit(e.op == Tok::Minus ? Op::Neg : Op::Not, dst, a);
            return Operand::var(dst);
        }
        case Expr::Binary: {
            if (e.op == Tok::AndAnd) return logical_and(e);
            if (e.op == Tok::OrOr) return logical_or(e);
            Operand a = expr(*e.kids[0]);
            Operand b = expr(*e.kids[1]);
            std::string dst = temp();
            emit(binary_op(e.op), dst, a, b);
            return Operand::var(dst);
        }
        case Expr::Call: {
            Instr in;
            in.op = Op::Call;
            in.label = e.name;
            for (const auto& arg : e.kids) in.args.push_back(expr(*arg));
            if (e.type != Type::Void) in.dst = temp();
            f.code.push_back(in);
            return in.dst.empty() ? Operand{} : Operand::var(in.dst);
        }
        }
        return {};
    }

    Operand logical_and(const Expr& e) {
        std::string dst = temp(), is_false = label(), end = label();
        emit_jz(expr(*e.kids[0]), is_false);
        emit_jz(expr(*e.kids[1]), is_false);
        emit(Op::Copy, dst, Operand::constant(1));
        emit_jump(end);
        emit_label(is_false);
        emit(Op::Copy, dst, Operand::constant(0));
        emit_label(end);
        return Operand::var(dst);
    }

    Operand logical_or(const Expr& e) {
        std::string dst = temp(), rhs = label(), end = label();
        emit_jz(expr(*e.kids[0]), rhs);
        emit(Op::Copy, dst, Operand::constant(1));
        emit_jump(end);
        emit_label(rhs);
        emit(Op::Copy, dst, expr(*e.kids[1]));
        emit_label(end);
        return Operand::var(dst);
    }

    void stmt(const Stmt& s) {
        switch (s.kind) {
        case Stmt::Decl:
            emit(Op::Copy, s.name, s.expr ? expr(*s.expr) : Operand::constant(0));
            break;
        case Stmt::Assign:
            emit(Op::Copy, s.name, expr(*s.expr));
            break;
        case Stmt::If: {
            std::string otherwise = label(), end = label();
            emit_jz(expr(*s.expr), otherwise);
            stmt(*s.body[0]);
            emit_jump(end);
            emit_label(otherwise);
            if (s.body.size() > 1) stmt(*s.body[1]);
            emit_label(end);
            break;
        }
        case Stmt::While: {
            std::string start = label(), end = label();
            emit_label(start);
            emit_jz(expr(*s.expr), end);
            stmt(*s.body[0]);
            emit_jump(start);
            emit_label(end);
            break;
        }
        case Stmt::Return:
            emit(Op::Ret, "", s.expr ? expr(*s.expr) : Operand{});
            break;
        case Stmt::ExprStmt:
            expr(*s.expr);
            break;
        case Stmt::Block:
            for (const auto& child : s.body) stmt(*child);
            break;
        }
    }
};

std::string show(const Operand& o) { return o.is_const ? std::to_string(o.value) : o.name; }

const char* op_symbol(Op op) {
    switch (op) {
    case Op::Add: return "+";
    case Op::Sub: return "-";
    case Op::Mul: return "*";
    case Op::Div: return "/";
    case Op::Mod: return "%";
    case Op::Eq: return "==";
    case Op::Ne: return "!=";
    case Op::Lt: return "<";
    case Op::Le: return "<=";
    case Op::Gt: return ">";
    case Op::Ge: return ">=";
    default: return "?";
    }
}

}

std::vector<IRFunc> gen_ir(const Program& program) {
    std::vector<IRFunc> out;
    for (const auto& fn : program.funcs) {
        Generator gen;
        gen.f.name = fn.name;
        for (const auto& param : fn.params) gen.f.params.push_back(param.name);
        gen.stmt(*fn.body);
        gen.emit(Op::Ret, "", Operand::constant(0));
        out.push_back(std::move(gen.f));
    }
    return out;
}

void dump_ir(const std::vector<IRFunc>& funcs, std::ostream& out) {
    for (const auto& f : funcs) {
        out << "function " << f.name << "(";
        for (size_t i = 0; i < f.params.size(); i++) out << (i ? ", " : "") << f.params[i];
        out << ")\n";
        for (const auto& in : f.code) {
            switch (in.op) {
            case Op::Label: out << in.label << ":\n"; continue;
            case Op::Nop: continue;
            case Op::Copy: out << "  " << in.dst << " = " << show(in.a); break;
            case Op::Neg: out << "  " << in.dst << " = -" << show(in.a); break;
            case Op::Not: out << "  " << in.dst << " = !" << show(in.a); break;
            case Op::Jump: out << "  goto " << in.label; break;
            case Op::JumpIfZero: out << "  ifz " << show(in.a) << " goto " << in.label; break;
            case Op::Ret: out << "  return" << (in.a.empty() ? "" : " " + show(in.a)); break;
            case Op::Call:
                out << "  " << (in.dst.empty() ? "" : in.dst + " = ") << "call " << in.label << "(";
                for (size_t i = 0; i < in.args.size(); i++) out << (i ? ", " : "") << show(in.args[i]);
                out << ")";
                break;
            default:
                out << "  " << in.dst << " = " << show(in.a) << " " << op_symbol(in.op) << " " << show(in.b);
            }
            out << "\n";
        }
        out << "\n";
    }
}
