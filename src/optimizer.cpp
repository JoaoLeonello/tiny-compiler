#include "compiler.h"

#include <climits>
#include <unordered_map>
#include <unordered_set>

namespace {

bool is_temp(const std::string& name) { return !name.empty() && name[0] == '%'; }

long wrap(unsigned long v) { return static_cast<long>(v); }

bool fold(Op op, long a, long b, long& r) {
    unsigned long ua = a, ub = b;
    switch (op) {
    case Op::Add: r = wrap(ua + ub); return true;
    case Op::Sub: r = wrap(ua - ub); return true;
    case Op::Mul: r = wrap(ua * ub); return true;
    case Op::Div:
    case Op::Mod:
        if (b == 0 || (a == LONG_MIN && b == -1)) return false;
        r = op == Op::Div ? a / b : a % b;
        return true;
    case Op::Eq: r = a == b; return true;
    case Op::Ne: r = a != b; return true;
    case Op::Lt: r = a < b; return true;
    case Op::Le: r = a <= b; return true;
    case Op::Gt: r = a > b; return true;
    case Op::Ge: r = a >= b; return true;
    default: return false;
    }
}

void constant_propagation(IRFunc& f) {
    std::unordered_map<std::string, long> known;
    auto substitute = [&](Operand& o) {
        if (o.is_const || o.name.empty()) return;
        auto it = known.find(o.name);
        if (it != known.end()) o = Operand::constant(it->second);
    };

    for (auto& in : f.code) {
        if (in.op == Op::Label) {
            known.clear();
            continue;
        }
        substitute(in.a);
        substitute(in.b);
        for (auto& arg : in.args) substitute(arg);

        if (!in.dst.empty()) {
            known.erase(in.dst);
            long r = 0;
            bool constant = false;
            if (in.op == Op::Copy && in.a.is_const) {
                r = in.a.value;
                constant = true;
            } else if (in.op == Op::Neg && in.a.is_const) {
                r = wrap(0UL - static_cast<unsigned long>(in.a.value));
                constant = true;
            } else if (in.op == Op::Not && in.a.is_const) {
                r = !in.a.value;
                constant = true;
            } else if (in.a.is_const && in.b.is_const && fold(in.op, in.a.value, in.b.value, r)) {
                constant = true;
            }
            if (constant) {
                in.op = Op::Copy;
                in.a = Operand::constant(r);
                in.b = {};
                known[in.dst] = r;
            }
        }

        if (in.op == Op::JumpIfZero && in.a.is_const) in.op = in.a.value == 0 ? Op::Jump : Op::Nop;
    }
}

void remove_unreachable(IRFunc& f) {
    std::vector<Instr> kept;
    bool dead = false;
    for (auto& in : f.code) {
        if (in.op == Op::Label) dead = false;
        if (!dead && in.op != Op::Nop) kept.push_back(std::move(in));
        if (in.op == Op::Jump || in.op == Op::Ret) dead = true;
    }
    f.code = std::move(kept);
}

void remove_jumps_to_next(IRFunc& f) {
    std::vector<Instr> kept;
    for (size_t i = 0; i < f.code.size(); i++) {
        const Instr& in = f.code[i];
        bool to_next = in.op == Op::Jump && i + 1 < f.code.size() &&
                       f.code[i + 1].op == Op::Label && f.code[i + 1].label == in.label;
        if (!to_next) kept.push_back(in);
    }
    f.code = std::move(kept);
}

void remove_unused_labels(IRFunc& f) {
    std::unordered_set<std::string> targets;
    for (const auto& in : f.code)
        if (in.op == Op::Jump || in.op == Op::JumpIfZero) targets.insert(in.label);
    std::vector<Instr> kept;
    for (auto& in : f.code)
        if (in.op != Op::Label || targets.count(in.label)) kept.push_back(std::move(in));
    f.code = std::move(kept);
}

void remove_dead_temps(IRFunc& f) {
    std::unordered_set<std::string> used;
    auto use = [&](const Operand& o) { if (!o.is_const && !o.name.empty()) used.insert(o.name); };
    for (const auto& in : f.code) {
        use(in.a);
        use(in.b);
        for (const auto& arg : in.args) use(arg);
    }
    std::vector<Instr> kept;
    for (auto& in : f.code) {
        if (is_temp(in.dst) && !used.count(in.dst)) {
            if (in.op != Op::Call) continue;
            in.dst.clear();
        }
        kept.push_back(std::move(in));
    }
    f.code = std::move(kept);
}

}

void optimize(std::vector<IRFunc>& funcs) {
    for (auto& f : funcs) {
        for (;;) {
            size_t before = f.code.size();
            constant_propagation(f);
            remove_unreachable(f);
            remove_jumps_to_next(f);
            remove_unused_labels(f);
            remove_dead_temps(f);
            if (f.code.size() == before) break;
        }
    }
}
