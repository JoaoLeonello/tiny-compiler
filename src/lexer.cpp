#include "compiler.h"

#include <cctype>
#include <cstring>
#include <unordered_map>

const char* tok_name(Tok kind) {
    static const char* names[] = {
        "int", "bool", "void", "if", "else", "while", "return", "true", "false",
        "identifier", "number",
        "(", ")", "{", "}", ",", ";", "=",
        "+", "-", "*", "/", "%", "!", "&&", "||",
        "==", "!=", "<", "<=", ">", ">=",
        "end of file"
    };
    return names[static_cast<int>(kind)];
}

std::vector<Token> lex(const std::string& s) {
    static const std::unordered_map<std::string, Tok> keywords = {
        {"int", Tok::Int}, {"bool", Tok::Bool}, {"void", Tok::Void},
        {"if", Tok::If}, {"else", Tok::Else}, {"while", Tok::While},
        {"return", Tok::Return}, {"true", Tok::True}, {"false", Tok::False},
    };
    static const std::pair<const char*, Tok> symbols[] = {
        {"&&", Tok::AndAnd}, {"||", Tok::OrOr}, {"==", Tok::Eq}, {"!=", Tok::Ne},
        {"<=", Tok::Le}, {">=", Tok::Ge},
        {"(", Tok::LParen}, {")", Tok::RParen}, {"{", Tok::LBrace}, {"}", Tok::RBrace},
        {",", Tok::Comma}, {";", Tok::Semi}, {"=", Tok::Assign},
        {"+", Tok::Plus}, {"-", Tok::Minus}, {"*", Tok::Star}, {"/", Tok::Slash},
        {"%", Tok::Percent}, {"!", Tok::Not}, {"<", Tok::Lt}, {">", Tok::Gt},
    };

    std::vector<Token> out;
    int line = 1;
    size_t i = 0;
    auto add = [&](Tok kind, std::string text, long value = 0) {
        out.push_back({kind, std::move(text), value, line});
    };

    while (i < s.size()) {
        char c = s[i];
        if (c == '\n') { line++; i++; continue; }
        if (std::isspace(static_cast<unsigned char>(c))) { i++; continue; }

        if (s.compare(i, 2, "//") == 0) {
            while (i < s.size() && s[i] != '\n') i++;
            continue;
        }
        if (s.compare(i, 2, "/*") == 0) {
            int start = line;
            i += 2;
            while (i < s.size() && s.compare(i, 2, "*/") != 0) {
                if (s[i] == '\n') line++;
                i++;
            }
            if (i >= s.size()) throw CompileError(start, "unterminated comment");
            i += 2;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t j = i;
            while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) j++;
            std::string text = s.substr(i, j - i);
            try {
                add(Tok::Number, text, std::stol(text));
            } catch (const std::out_of_range&) {
                throw CompileError(line, "integer literal too large: " + text);
            }
            i = j;
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t j = i;
            while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_')) j++;
            std::string word = s.substr(i, j - i);
            auto it = keywords.find(word);
            add(it == keywords.end() ? Tok::Ident : it->second, word);
            i = j;
            continue;
        }

        bool matched = false;
        for (const auto& [text, kind] : symbols) {
            size_t n = std::strlen(text);
            if (s.compare(i, n, text) == 0) {
                add(kind, text);
                i += n;
                matched = true;
                break;
            }
        }
        if (!matched) throw CompileError(line, std::string("unexpected character '") + c + "'");
    }

    add(Tok::End, "");
    return out;
}
