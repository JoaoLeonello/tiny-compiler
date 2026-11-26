#include "compiler.h"

namespace {

struct Parser {
    const std::vector<Token>& t;
    size_t p = 0;

    const Token& peek() const { return t[p]; }
    bool at(Tok k) const { return t[p].kind == k; }

    bool accept(Tok k) {
        if (!at(k)) return false;
        p++;
        return true;
    }

    const Token& expect(Tok k) {
        if (!at(k))
            throw CompileError(peek().line, std::string("expected '") + tok_name(k) +
                                                "' but found '" + tok_name(peek().kind) + "'");
        return t[p++];
    }

    bool at_type() const { return at(Tok::Int) || at(Tok::Bool) || at(Tok::Void); }

    Type type() {
        if (!at_type())
            throw CompileError(peek().line, std::string("expected type but found '") +
                                                tok_name(peek().kind) + "'");
        Tok k = t[p++].kind;
        return k == Tok::Int ? Type::Int : k == Tok::Bool ? Type::Bool : Type::Void;
    }

    Program program() {
        Program prog;
        while (!at(Tok::End)) prog.funcs.push_back(function());
        return prog;
    }

    Function function() {
        Function f;
        f.line = peek().line;
        f.ret = type();
        f.name = expect(Tok::Ident).text;
        expect(Tok::LParen);
        if (!at(Tok::RParen)) {
            do {
                Param param;
                param.type = type();
                param.name = expect(Tok::Ident).text;
                f.params.push_back(param);
            } while (accept(Tok::Comma));
        }
        expect(Tok::RParen);
        f.body = block();
        return f;
    }

    std::unique_ptr<Stmt> make(Stmt::Kind kind) {
        auto s = std::make_unique<Stmt>();
        s->kind = kind;
        s->line = peek().line;
        return s;
    }

    std::unique_ptr<Stmt> block() {
        auto s = make(Stmt::Block);
        expect(Tok::LBrace);
        while (!at(Tok::RBrace) && !at(Tok::End)) s->body.push_back(statement());
        expect(Tok::RBrace);
        return s;
    }

    std::unique_ptr<Stmt> statement() {
        if (at(Tok::LBrace)) return block();

        if (at_type()) {
            auto s = make(Stmt::Decl);
            s->type = type();
            s->name = expect(Tok::Ident).text;
            if (accept(Tok::Assign)) s->expr = expr();
            expect(Tok::Semi);
            return s;
        }

        if (at(Tok::If)) {
            auto s = make(Stmt::If);
            p++;
            expect(Tok::LParen);
            s->expr = expr();
            expect(Tok::RParen);
            s->body.push_back(statement());
            if (accept(Tok::Else)) s->body.push_back(statement());
            return s;
        }

        if (at(Tok::While)) {
            auto s = make(Stmt::While);
            p++;
            expect(Tok::LParen);
            s->expr = expr();
            expect(Tok::RParen);
            s->body.push_back(statement());
            return s;
        }

        if (at(Tok::Return)) {
            auto s = make(Stmt::Return);
            p++;
            if (!at(Tok::Semi)) s->expr = expr();
            expect(Tok::Semi);
            return s;
        }

        if (at(Tok::Ident) && t[p + 1].kind == Tok::Assign) {
            auto s = make(Stmt::Assign);
            s->name = t[p].text;
            p += 2;
            s->expr = expr();
            expect(Tok::Semi);
            return s;
        }

        auto s = make(Stmt::ExprStmt);
        s->expr = expr();
        expect(Tok::Semi);
        return s;
    }

    std::unique_ptr<Expr> node(Expr::Kind kind, int line) {
        auto e = std::make_unique<Expr>();
        e->kind = kind;
        e->line = line;
        return e;
    }

    std::unique_ptr<Expr> expr(size_t level = 0) {
        static const std::vector<std::vector<Tok>> levels = {
            {Tok::OrOr},
            {Tok::AndAnd},
            {Tok::Eq, Tok::Ne},
            {Tok::Lt, Tok::Le, Tok::Gt, Tok::Ge},
            {Tok::Plus, Tok::Minus},
            {Tok::Star, Tok::Slash, Tok::Percent},
        };
        if (level == levels.size()) return unary();

        auto lhs = expr(level + 1);
        for (;;) {
            bool found = false;
            for (Tok op : levels[level]) found = found || at(op);
            if (!found) return lhs;
            auto e = node(Expr::Binary, peek().line);
            e->op = t[p++].kind;
            e->kids.push_back(std::move(lhs));
            e->kids.push_back(expr(level + 1));
            lhs = std::move(e);
        }
    }

    std::unique_ptr<Expr> unary() {
        if (at(Tok::Minus) || at(Tok::Not)) {
            auto e = node(Expr::Unary, peek().line);
            e->op = t[p++].kind;
            e->kids.push_back(unary());
            return e;
        }
        return primary();
    }

    std::unique_ptr<Expr> primary() {
        const Token& tok = peek();
        if (accept(Tok::Number)) {
            auto e = node(Expr::Num, tok.line);
            e->value = tok.value;
            return e;
        }
        if (at(Tok::True) || at(Tok::False)) {
            auto e = node(Expr::BoolLit, tok.line);
            e->value = at(Tok::True);
            p++;
            return e;
        }
        if (accept(Tok::Ident)) {
            if (!accept(Tok::LParen)) {
                auto e = node(Expr::Var, tok.line);
                e->name = tok.text;
                return e;
            }
            auto e = node(Expr::Call, tok.line);
            e->name = tok.text;
            if (!at(Tok::RParen)) {
                do e->kids.push_back(expr());
                while (accept(Tok::Comma));
            }
            expect(Tok::RParen);
            return e;
        }
        if (accept(Tok::LParen)) {
            auto e = expr();
            expect(Tok::RParen);
            return e;
        }
        throw CompileError(tok.line, std::string("expected expression but found '") +
                                         tok_name(tok.kind) + "'");
    }
};

void dump_expr(const Expr& e, std::ostream& out) {
    switch (e.kind) {
    case Expr::Num: out << e.value; break;
    case Expr::BoolLit: out << (e.value ? "true" : "false"); break;
    case Expr::Var: out << e.name; break;
    case Expr::Unary:
    case Expr::Binary:
        out << "(" << tok_name(e.op);
        for (const auto& k : e.kids) { out << " "; dump_expr(*k, out); }
        out << ")";
        break;
    case Expr::Call:
        out << "(call " << e.name;
        for (const auto& k : e.kids) { out << " "; dump_expr(*k, out); }
        out << ")";
        break;
    }
}

void dump_stmt(const Stmt& s, std::ostream& out, int depth) {
    std::string ind(depth * 2, ' ');
    switch (s.kind) {
    case Stmt::Decl:
        out << ind << "(decl " << type_name(s.type) << " " << s.name;
        if (s.expr) { out << " "; dump_expr(*s.expr, out); }
        out << ")\n";
        return;
    case Stmt::Assign:
        out << ind << "(set " << s.name << " ";
        dump_expr(*s.expr, out);
        out << ")\n";
        return;
    case Stmt::Return:
        out << ind << "(return";
        if (s.expr) { out << " "; dump_expr(*s.expr, out); }
        out << ")\n";
        return;
    case Stmt::ExprStmt:
        out << ind;
        dump_expr(*s.expr, out);
        out << "\n";
        return;
    case Stmt::If:
    case Stmt::While:
        out << ind << (s.kind == Stmt::If ? "(if " : "(while ");
        dump_expr(*s.expr, out);
        out << "\n";
        break;
    case Stmt::Block:
        out << ind << "(block\n";
        break;
    }
    for (const auto& child : s.body) dump_stmt(*child, out, depth + 1);
    out << ind << ")\n";
}

}

Program parse(const std::vector<Token>& tokens) {
    Parser parser{tokens};
    return parser.program();
}

void dump_ast(const Program& program, std::ostream& out) {
    for (const auto& f : program.funcs) {
        out << "(function " << type_name(f.ret) << " " << f.name << " (";
        for (size_t i = 0; i < f.params.size(); i++)
            out << (i ? " " : "") << type_name(f.params[i].type) << " " << f.params[i].name;
        out << ")\n";
        dump_stmt(*f.body, out, 1);
        out << ")\n";
    }
}
