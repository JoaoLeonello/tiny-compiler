#include "compiler.h"

#include <climits>
#include <unordered_map>

namespace {

const char* const arg_regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

const char* set_instr(Op op) {
    switch (op) {
    case Op::Eq: return "sete";
    case Op::Ne: return "setne";
    case Op::Lt: return "setl";
    case Op::Le: return "setle";
    case Op::Gt: return "setg";
    default: return "setge";
    }
}

std::string symbol(const std::string& name) {
    if (name == "print") return "mc_print";
    if (name == "read") return "mc_read";
    return name;
}

struct Emitter {
    std::ostream& out;
    std::unordered_map<std::string, int> slots;

    void assign_slot(const std::string& name) {
        if (!name.empty() && !slots.count(name)) slots[name] = static_cast<int>(slots.size() + 1) * 8;
    }

    std::string mem(const std::string& name) { return "-" + std::to_string(slots.at(name)) + "(%rbp)"; }

    void load(const Operand& o, const char* reg) {
        if (!o.is_const)
            out << "  movq " << mem(o.name) << ", " << reg << "\n";
        else if (o.value >= INT_MIN && o.value <= INT_MAX)
            out << "  movq $" << o.value << ", " << reg << "\n";
        else
            out << "  movabsq $" << o.value << ", " << reg << "\n";
    }

    void store(const std::string& name) { out << "  movq %rax, " << mem(name) << "\n"; }

    void function(const IRFunc& f) {
        slots.clear();
        for (const auto& p : f.params) assign_slot(p);
        for (const auto& in : f.code) {
            assign_slot(in.dst);
            assign_slot(in.a.is_const ? "" : in.a.name);
            assign_slot(in.b.is_const ? "" : in.b.name);
            for (const auto& arg : in.args) assign_slot(arg.is_const ? "" : arg.name);
        }
        int frame = (static_cast<int>(slots.size()) * 8 + 15) & ~15;

        out << "\n  .globl " << f.name << "\n" << f.name << ":\n";
        out << "  pushq %rbp\n  movq %rsp, %rbp\n";
        if (frame) out << "  subq $" << frame << ", %rsp\n";
        for (size_t i = 0; i < f.params.size(); i++)
            out << "  movq " << arg_regs[i] << ", " << mem(f.params[i]) << "\n";

        for (const auto& in : f.code) instr(in);
    }

    void instr(const Instr& in) {
        switch (in.op) {
        case Op::Nop:
            return;
        case Op::Label:
            out << in.label << ":\n";
            return;
        case Op::Jump:
            out << "  jmp " << in.label << "\n";
            return;
        case Op::JumpIfZero:
            load(in.a, "%rax");
            out << "  testq %rax, %rax\n  jz " << in.label << "\n";
            return;
        case Op::Ret:
            if (in.a.empty()) out << "  xorl %eax, %eax\n";
            else load(in.a, "%rax");
            out << "  leave\n  ret\n";
            return;
        case Op::Call:
            for (size_t i = 0; i < in.args.size(); i++) load(in.args[i], arg_regs[i]);
            out << "  call " << symbol(in.label) << "\n";
            if (!in.dst.empty()) store(in.dst);
            return;
        case Op::Copy:
            load(in.a, "%rax");
            break;
        case Op::Neg:
            load(in.a, "%rax");
            out << "  negq %rax\n";
            break;
        case Op::Not:
            load(in.a, "%rax");
            out << "  xorq $1, %rax\n";
            break;
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
            load(in.a, "%rax");
            load(in.b, "%rcx");
            out << "  " << (in.op == Op::Add ? "addq" : in.op == Op::Sub ? "subq" : "imulq") << " %rcx, %rax\n";
            break;
        case Op::Div:
        case Op::Mod:
            load(in.a, "%rax");
            load(in.b, "%rcx");
            out << "  cqto\n  idivq %rcx\n";
            if (in.op == Op::Mod) out << "  movq %rdx, %rax\n";
            break;
        default:
            load(in.a, "%rax");
            load(in.b, "%rcx");
            out << "  cmpq %rcx, %rax\n  " << set_instr(in.op) << " %al\n  movzbq %al, %rax\n";
            break;
        }
        store(in.dst);
    }
};

}

void gen_asm(const std::vector<IRFunc>& funcs, std::ostream& out) {
    out << "  .text\n";
    Emitter emitter{out, {}};
    for (const auto& f : funcs) emitter.function(f);
    out << "\n  .section .note.GNU-stack,\"\",@progbits\n";
}
