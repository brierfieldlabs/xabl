// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include "expression.hpp"
#include "internal.hpp"
#include <xabl/runtime/xabl.hpp>

#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace xabl {
namespace {

// dBASE logical spellings are distinct tokens, not substrings: 'CANDY'
// must remain an identifier and quoted AND/OR must remain ordinary text.
enum class Kind {
    End, Number, String, Identifier, True, False,
    LParen, RParen, Comma, Plus, Minus, Multiply, Divide,
    Greater, Less, Equal, EqualExact, NotEqual, GreaterEqual, LessEqual,
    And, Or, Not
};

struct Token {
    Kind kind{Kind::End};
    std::string text{};
    std::size_t position{};
};

class Lexer {
public:
    explicit Lexer(std::string_view source) : source_(source) {}

    Token next() {
        while (offset_ < source_.size() &&
               std::isspace(static_cast<unsigned char>(source_[offset_]))) {
            ++offset_;
        }
        const std::size_t start = offset_;
        if (offset_ == source_.size()) {
            return {Kind::End, {}, start};
        }

        const char c = source_[offset_];
        if (c == '\'' || c == '"') {
            ++offset_;
            std::string value;
            while (offset_ < source_.size()) {
                const char current = source_[offset_++];
                if (current == c) {
                    // Doubled matching delimiters are conventional xBase
                    // quoted-character escapes.
                    if (offset_ < source_.size() && source_[offset_] == c) {
                        value.push_back(c);
                        ++offset_;
                        continue;
                    }
                    return {Kind::String, std::move(value), start};
                }
                value.push_back(current);
            }
            throw std::runtime_error("unterminated quoted string at column " +
                                     std::to_string(start + 1));
        }

        if (c == '.') {
            for (const auto& [word, kind] : {
                     std::pair{".AND.", Kind::And},
                     {".NOT.", Kind::Not},
                     {".OR.", Kind::Or},
                     {".T.", Kind::True},
                     {".F.", Kind::False}}) {
                const std::string_view spelling{word};
                if (offset_ + spelling.size() <= source_.size() &&
                    upper(std::string(source_.substr(offset_, spelling.size()))) == spelling) {
                    offset_ += spelling.size();
                    return {kind, std::string(spelling), start};
                }
            }
        }

        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && offset_ + 1 < source_.size() &&
             std::isdigit(static_cast<unsigned char>(source_[offset_ + 1])))) {
            // strtod handles exponent signs without confusing them with the
            // following additive operators. Reject junk suffixes in parser.
            const std::string remaining(source_.substr(offset_));
            char* end = nullptr;
            std::strtod(remaining.c_str(), &end);
            const std::size_t length = static_cast<std::size_t>(end - remaining.c_str());
            if (length == 0) {
                throw std::runtime_error("invalid numeric literal");
            }
            offset_ += length;
            return {Kind::Number, remaining.substr(0, length), start};
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            ++offset_;
            while (offset_ < source_.size()) {
                const char d = source_[offset_];
                if (std::isalnum(static_cast<unsigned char>(d)) || d == '_') {
                    ++offset_;
                } else if (d == '-' && offset_ + 2 < source_.size() &&
                           source_[offset_ + 1] == '>' &&
                           (std::isalpha(static_cast<unsigned char>(source_[offset_ + 2])) ||
                            source_[offset_ + 2] == '_')) {
                    offset_ += 2; // alias->field remains one name token
                } else {
                    break;
                }
            }
            std::string name = upper(std::string(source_.substr(start, offset_ - start)));
            if (name == "AND") return {Kind::And, std::move(name), start};
            if (name == "OR") return {Kind::Or, std::move(name), start};
            if (name == "NOT") return {Kind::Not, std::move(name), start};
            if (name == "TRUE") return {Kind::True, std::move(name), start};
            if (name == "FALSE") return {Kind::False, std::move(name), start};
            return {Kind::Identifier, std::move(name), start};
        }

        ++offset_;
        switch (c) {
        case '(': return {Kind::LParen, "(", start};
        case ')': return {Kind::RParen, ")", start};
        case ',': return {Kind::Comma, ",", start};
        case '+': return {Kind::Plus, "+", start};
        case '-': return {Kind::Minus, "-", start};
        case '*': return {Kind::Multiply, "*", start};
        case '/': return {Kind::Divide, "/", start};
        case '=':
            if (accept('=')) return {Kind::EqualExact, "==", start};
            return {Kind::Equal, "=", start};
        case '>':
            if (accept('=')) return {Kind::GreaterEqual, ">=", start};
            return {Kind::Greater, ">", start};
        case '<':
            if (accept('=')) return {Kind::LessEqual, "<=", start};
            if (accept('>')) return {Kind::NotEqual, "<>", start};
            return {Kind::Less, "<", start};
        case '!':
            if (accept('=')) return {Kind::NotEqual, "!=", start};
            break;
        case '#': return {Kind::NotEqual, "#", start};
        }
        throw std::runtime_error("unexpected character at column " +
                                 std::to_string(start + 1) + ": " + c);
    }

private:
    bool accept(char c) {
        if (offset_ < source_.size() && source_[offset_] == c) {
            ++offset_;
            return true;
        }
        return false;
    }

    std::string_view source_;
    std::size_t offset_{};
};

// Pratt precedence: OR < AND < comparison < addition < multiplication
// < unary arithmetic. NOT applies to a comparison but before AND/OR,
// matching traditional dBASE logical expressions such as NOT balance < 100.
int precedence(Kind kind) {
    switch (kind) {
    case Kind::Or: return 1;
    case Kind::And: return 2;
    case Kind::Greater:
    case Kind::Less:
    case Kind::Equal:
    case Kind::EqualExact:
    case Kind::NotEqual:
    case Kind::GreaterEqual:
    case Kind::LessEqual: return 3;
    case Kind::Plus:
    case Kind::Minus: return 4;
    case Kind::Multiply:
    case Kind::Divide: return 5;
    default: return 0;
    }
}

class Parser {
public:
    Parser(std::string_view source, Program& program)
        : lexer_(source), program_(program), token_(lexer_.next()) {}

    void compile() {
        expression(1);
        if (token_.kind != Kind::End) {
            fail("unexpected token after expression");
        }
    }

private:
    void advance() { token_ = lexer_.next(); }

    [[noreturn]] void fail(std::string_view message) const {
        throw std::runtime_error(std::string(message) +
                                 " at column " + std::to_string(token_.position + 1));
    }

    void emit(OpCode opcode) {
        program_.code.push_back({opcode});
    }

    void prefix() {
        const Token current = token_;
        advance();
        switch (current.kind) {
        case Kind::Number:
            program_.code.push_back({OpCode::PushLiteral, Value(std::stod(current.text))});
            return;
        case Kind::String:
            program_.code.push_back({OpCode::PushLiteral, Value(current.text)});
            return;
        case Kind::True:
            program_.code.push_back({OpCode::PushLiteral, Value(true)});
            return;
        case Kind::False:
            program_.code.push_back({OpCode::PushLiteral, Value(false)});
            return;
        case Kind::Identifier: {
            if (token_.kind != Kind::LParen) {
                program_.code.push_back({OpCode::LoadName, {}, current.text});
                return;
            }
            advance();
            // Status functions take no parameters. Character functions use
            // exactly one complete expression, which may itself be a call.
            const auto zero_arg = [&]() -> OpCode {
                if (current.text == "EOF") return OpCode::CallEof;
                if (current.text == "BOF") return OpCode::CallBof;
                if (current.text == "FOUND") return OpCode::CallFound;
                if (current.text == "RECNO") return OpCode::CallRecno;
                if (current.text == "RECCOUNT") return OpCode::CallReccount;
                if (current.text == "DELETED") return OpCode::CallDeleted;
                return OpCode::Halt;
            };
            if (const auto opcode = zero_arg(); opcode != OpCode::Halt) {
                if (token_.kind != Kind::RParen) {
                    fail("zero-argument function takes no parameters");
                }
                advance();
                emit(opcode);
                return;
            }
            const auto one_arg = [&]() -> OpCode {
                if (current.text == "LEN") return OpCode::CallLen;
                if (current.text == "UPPER") return OpCode::CallUpper;
                if (current.text == "LOWER") return OpCode::CallLower;
                if (current.text == "TRIM" || current.text == "RTRIM")
                    return OpCode::CallTrim;
                if (current.text == "LTRIM") return OpCode::CallLTrim;
                if (current.text == "SPACE") return OpCode::CallSpace;
                if (current.text == "VAL") return OpCode::CallVal;
                if (current.text == "ABS") return OpCode::CallAbs;
                if (current.text == "INT") return OpCode::CallInt;
                if (current.text == "ASC") return OpCode::CallAsc;
                if (current.text == "CHR") return OpCode::CallChr;
                return OpCode::Halt;
            };
            if (const auto opcode = one_arg(); opcode != OpCode::Halt) {
                if (token_.kind == Kind::RParen) {
                    fail("missing character-function argument");
                }
                expression(1);
                if (token_.kind != Kind::RParen) {
                    fail("character function expects one argument");
                }
                advance();
                emit(opcode);
                return;
            }
            // STUFF takes four full expressions, including nested calls.
            if (current.text == "STUFF") {
                for (int argument = 0; argument < 4; ++argument) {
                    if (argument != 0) {
                        if (token_.kind != Kind::Comma) {
                            fail("STUFF requires exactly four arguments");
                        }
                        advance();
                    }
                    expression(1);
                }
                if (token_.kind != Kind::RParen) {
                    fail("STUFF requires exactly four arguments");
                }
                advance();
                emit(OpCode::CallStuff);
                return;
            }
            // STR takes a numeric argument and up to two formatting
            // arguments; preserve its argument count in the bytecode.
            if (current.text == "STR") {
                if (token_.kind == Kind::RParen) fail("STR requires a numeric argument");
                expression(1);
                std::size_t count = 1;
                while (token_.kind == Kind::Comma && count < 3) {
                    advance();
                    expression(1);
                    ++count;
                }
                if (token_.kind != Kind::RParen) fail("STR takes one to three arguments");
                advance();
                Instruction instruction{OpCode::CallStr};
                instruction.target = count;
                program_.code.push_back(std::move(instruction));
                return;
            }
            // Search and substring functions each accept two full
            // expressions; only SUBSTR accepts a third argument.
            OpCode multi_arg_op = OpCode::Halt;
            if (current.text == "LEFT") multi_arg_op = OpCode::CallLeft;
            if (current.text == "RIGHT") multi_arg_op = OpCode::CallRight;
            if (current.text == "SUBSTR") multi_arg_op = OpCode::CallSubstr;
            if (current.text == "AT") multi_arg_op = OpCode::CallAt;
            if (current.text == "REPLICATE") multi_arg_op = OpCode::CallReplicate;
            if (current.text == "MIN") multi_arg_op = OpCode::CallMin;
            if (current.text == "MAX") multi_arg_op = OpCode::CallMax;
            if (multi_arg_op != OpCode::Halt) {
                if (token_.kind == Kind::RParen) fail("missing string argument");
                expression(1);
                if (token_.kind != Kind::Comma) {
                    fail("string slice function expects two arguments");
                }
                advance();
                expression(1);
                std::size_t argument_count = 2;
                if (token_.kind == Kind::Comma &&
                    multi_arg_op == OpCode::CallSubstr) {
                    advance();
                    expression(1);
                    argument_count = 3;
                }
                if (token_.kind != Kind::RParen) {
                    fail("invalid string slice function argument count");
                }
                advance();
                Instruction instruction{multi_arg_op};
                instruction.target = argument_count;
                program_.code.push_back(std::move(instruction));
                return;
            }
            throw std::runtime_error("unsupported function: " + current.text);
        }
        case Kind::LParen:
            expression(1);
            if (token_.kind != Kind::RParen) fail("missing closing parenthesis");
            advance();
            return;
        case Kind::Plus:
            expression(6);
            return;
        case Kind::Minus:
            program_.code.push_back({OpCode::PushLiteral, Value(0.0)});
            expression(6);
            emit(OpCode::Subtract);
            return;
        case Kind::Not:
            expression(3);
            emit(OpCode::UnaryNot);
            return;
        default:
            throw std::runtime_error("expected expression at column " +
                                     std::to_string(current.position + 1));
        }
    }

    void expression(int min_precedence) {
        prefix();
        while (precedence(token_.kind) >= min_precedence) {
            const int prec = precedence(token_.kind);
            const Kind op = token_.kind;
            advance();
            // Left-associative binary operators consume RHS at higher minimum.
            expression(prec + 1);
            switch (op) {
            case Kind::Or: emit(OpCode::LogicalOr); break;
            case Kind::And: emit(OpCode::LogicalAnd); break;
            case Kind::Plus: emit(OpCode::Add); break;
            case Kind::Minus: emit(OpCode::Subtract); break;
            case Kind::Multiply: emit(OpCode::Multiply); break;
            case Kind::Divide: emit(OpCode::Divide); break;
            case Kind::Greater: emit(OpCode::Greater); break;
            case Kind::Less: emit(OpCode::Less); break;
            case Kind::Equal: emit(OpCode::Equal); break;
            case Kind::EqualExact: emit(OpCode::EqualExact); break;
            case Kind::NotEqual:
                emit(OpCode::Equal);
                emit(OpCode::UnaryNot);
                break;
            case Kind::GreaterEqual:
                emit(OpCode::Less);
                emit(OpCode::UnaryNot);
                break;
            case Kind::LessEqual:
                emit(OpCode::Greater);
                emit(OpCode::UnaryNot);
                break;
            default: fail("invalid binary operator");
            }
        }
    }

    Lexer lexer_;
    Program& program_;
    Token token_;
};
} // namespace

void ExpressionCompiler::emit(std::string_view source) const {
    Parser{source, program}.compile();
}

} // namespace xabl
