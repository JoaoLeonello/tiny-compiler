#include "compiler.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#ifndef RUNTIME_OBJ
#define RUNTIME_OBJ "build/runtime.o"
#endif

namespace {

int usage() {
    std::cerr << "usage: minicc [-O] [--tokens | --ast | --ir | -S] [-o output] file.mc\n";
    return 2;
}

std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
}

}

int main(int argc, char** argv) {
    std::string input, output = "a.out", mode = "exe";
    bool opt = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) output = argv[++i];
        else if (arg == "-O") opt = true;
        else if (arg == "--tokens" || arg == "--ast" || arg == "--ir") mode = arg.substr(2);
        else if (arg == "-S") mode = "asm";
        else if (arg[0] == '-' || !input.empty()) return usage();
        else input = arg;
    }
    if (input.empty()) return usage();

    std::ifstream file(input);
    if (!file) {
        std::cerr << "minicc: cannot open '" << input << "'\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    try {
        std::vector<Token> tokens = lex(buffer.str());
        if (mode == "tokens") {
            for (const auto& t : tokens)
                std::cout << t.line << "\t" << tok_name(t.kind) << "\t" << t.text << "\n";
            return 0;
        }

        Program program = parse(tokens);
        if (mode == "ast") {
            dump_ast(program, std::cout);
            return 0;
        }

        check(program);
        std::vector<IRFunc> ir = gen_ir(program);
        if (opt) optimize(ir);
        if (mode == "ir") {
            dump_ir(ir, std::cout);
            return 0;
        }
        if (mode == "asm") {
            gen_asm(ir, std::cout);
            return 0;
        }

        std::string asm_path = output + ".s";
        {
            std::ofstream asm_file(asm_path);
            gen_asm(ir, asm_file);
        }
        std::string cmd = "cc -o " + shell_quote(output) + " " + shell_quote(asm_path) + " " +
                          shell_quote(RUNTIME_OBJ);
        int status = std::system(cmd.c_str());
        std::remove(asm_path.c_str());
        if (status != 0) {
            std::cerr << "minicc: assembler/linker failed\n";
            return 1;
        }
    } catch (const CompileError& e) {
        std::cerr << input << ":" << e.line << ": error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
