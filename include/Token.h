#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <string_view>
#include <cstdint>
#include "Diagnostic.h"

enum class TokenKind {
    // End of file / Unknown
    EndOfFile,
    Unknown,

    // Keywords
    KW_int,
    KW_char,
    KW_void,
    KW_return,
    KW_if,
    KW_else,
    KW_while,
    KW_do,
    KW_for,
    KW_break,
    KW_continue,
    KW_struct,
    KW_sizeof,

    // Identifiers & Literals
    Identifier,
    IntLiteral,
    CharLiteral,
    StringLiteral,

    // Operators
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    PlusPlus,       // ++
    MinusMinus,     // --

    Equal,          // =
    PlusEqual,      // +=
    MinusEqual,     // -=
    StarEqual,      // *=
    SlashEqual,     // /=
    PercentEqual,   // %=
    AmpEqual,       // &=
    PipeEqual,      // |=
    CaretEqual,     // ^=
    LessLessEqual,  // <<=
    GreaterGreaterEqual, // >>=

    EqualEqual,     // ==
    ExclaimEqual,   // !=
    Less,           // <
    LessEqual,      // <=
    Greater,        // >
    GreaterEqual,   // >=

    AmpAmp,         // &&
    PipePipe,       // ||
    Exclaim,        // !

    Amp,            // &
    Pipe,           // |
    Caret,          // ^
    Tilde,          // ~
    LessLess,       // <<
    GreaterGreater, // >>

    Arrow,          // ->
    Dot,            // .

    // Punctuation
    LParen,         // (
    RParen,         // )
    LBracket,       // [
    RBracket,       // ]
    LBrace,         // {
    RBrace,         // }
    Semicolon,      // ;
    Comma,          // ,
    Question,       // ?
    Colon           // :
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    SourceLocation location;

    // Parsed literal values
    int64_t intValue = 0;
    char charValue = '\0';
    std::string stringValue;

    bool is(TokenKind k) const { return kind == k; }
    bool isNot(TokenKind k) const { return kind != k; }
    bool isOneOf(TokenKind k1, TokenKind k2) const { return kind == k1 || kind == k2; }
    template <typename... Args>
    bool isOneOf(TokenKind k1, Args... args) const {
        return kind == k1 || isOneOf(args...);
    }

    std::string toString() const;
};

std::string_view tokenKindToString(TokenKind kind);
bool isKeyword(TokenKind kind);
bool isAssignmentOp(TokenKind kind);
bool isBinaryOp(TokenKind kind);
bool isUnaryOp(TokenKind kind);

#endif // TOKEN_H
