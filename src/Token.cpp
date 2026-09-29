#include "Token.h"

std::string_view tokenKindToString(TokenKind kind) {
    switch (kind) {
        case TokenKind::EndOfFile:            return "EndOfFile";
        case TokenKind::Unknown:              return "Unknown";
        case TokenKind::KW_int:               return "int";
        case TokenKind::KW_char:              return "char";
        case TokenKind::KW_void:              return "void";
        case TokenKind::KW_return:            return "return";
        case TokenKind::KW_if:                return "if";
        case TokenKind::KW_else:              return "else";
        case TokenKind::KW_while:             return "while";
        case TokenKind::KW_do:                return "do";
        case TokenKind::KW_for:               return "for";
        case TokenKind::KW_break:             return "break";
        case TokenKind::KW_continue:          return "continue";
        case TokenKind::KW_struct:            return "struct";
        case TokenKind::KW_sizeof:            return "sizeof";
        case TokenKind::Identifier:           return "Identifier";
        case TokenKind::IntLiteral:           return "IntLiteral";
        case TokenKind::CharLiteral:          return "CharLiteral";
        case TokenKind::StringLiteral:        return "StringLiteral";
        case TokenKind::Plus:                 return "+";
        case TokenKind::Minus:                return "-";
        case TokenKind::Star:                 return "*";
        case TokenKind::Slash:                return "/";
        case TokenKind::Percent:              return "%";
        case TokenKind::PlusPlus:             return "++";
        case TokenKind::MinusMinus:           return "--";
        case TokenKind::Equal:                return "=";
        case TokenKind::PlusEqual:            return "+=";
        case TokenKind::MinusEqual:           return "-=";
        case TokenKind::StarEqual:            return "*=";
        case TokenKind::SlashEqual:           return "/=";
        case TokenKind::PercentEqual:         return "%=";
        case TokenKind::AmpEqual:             return "&=";
        case TokenKind::PipeEqual:            return "|=";
        case TokenKind::CaretEqual:           return "^=";
        case TokenKind::LessLessEqual:        return "<<=";
        case TokenKind::GreaterGreaterEqual:  return ">>=";
        case TokenKind::EqualEqual:           return "==";
        case TokenKind::ExclaimEqual:         return "!=";
        case TokenKind::Less:                 return "<";
        case TokenKind::LessEqual:            return "<=";
        case TokenKind::Greater:              return ">";
        case TokenKind::GreaterEqual:         return ">=";
        case TokenKind::AmpAmp:               return "&&";
        case TokenKind::PipePipe:             return "||";
        case TokenKind::Exclaim:              return "!";
        case TokenKind::Amp:                  return "&";
        case TokenKind::Pipe:                 return "|";
        case TokenKind::Caret:                return "^";
        case TokenKind::Tilde:                return "~";
        case TokenKind::LessLess:             return "<<";
        case TokenKind::GreaterGreater:       return ">>";
        case TokenKind::Arrow:                return "->";
        case TokenKind::Dot:                  return ".";
        case TokenKind::LParen:               return "(";
        case TokenKind::RParen:               return ")";
        case TokenKind::LBracket:             return "[";
        case TokenKind::RBracket:             return "]";
        case TokenKind::LBrace:               return "{";
        case TokenKind::RBrace:               return "}";
        case TokenKind::Semicolon:            return ";";
        case TokenKind::Comma:                return ",";
        case TokenKind::Question:             return "?";
        case TokenKind::Colon:                return ":";
    }
    return "Unknown";
}

bool isKeyword(TokenKind kind) {
    switch (kind) {
        case TokenKind::KW_int:
        case TokenKind::KW_char:
        case TokenKind::KW_void:
        case TokenKind::KW_return:
        case TokenKind::KW_if:
        case TokenKind::KW_else:
        case TokenKind::KW_while:
        case TokenKind::KW_do:
        case TokenKind::KW_for:
        case TokenKind::KW_break:
        case TokenKind::KW_continue:
        case TokenKind::KW_struct:
        case TokenKind::KW_sizeof:
            return true;
        default:
            return false;
    }
}

bool isAssignmentOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::Equal:
        case TokenKind::PlusEqual:
        case TokenKind::MinusEqual:
        case TokenKind::StarEqual:
        case TokenKind::SlashEqual:
        case TokenKind::PercentEqual:
        case TokenKind::AmpEqual:
        case TokenKind::PipeEqual:
        case TokenKind::CaretEqual:
        case TokenKind::LessLessEqual:
        case TokenKind::GreaterGreaterEqual:
            return true;
        default:
            return false;
    }
}

bool isBinaryOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent:
        case TokenKind::EqualEqual:
        case TokenKind::ExclaimEqual:
        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:
        case TokenKind::AmpAmp:
        case TokenKind::PipePipe:
        case TokenKind::Amp:
        case TokenKind::Pipe:
        case TokenKind::Caret:
        case TokenKind::LessLess:
        case TokenKind::GreaterGreater:
            return true;
        default:
            return false;
    }
}

bool isUnaryOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Exclaim:
        case TokenKind::Tilde:
        case TokenKind::Star:
        case TokenKind::Amp:
        case TokenKind::PlusPlus:
        case TokenKind::MinusMinus:
        case TokenKind::KW_sizeof:
            return true;
        default:
            return false;
    }
}

std::string Token::toString() const {
    std::string result = "[";
    result += location.toString();
    result += "] ";
    result += tokenKindToString(kind);
    result += " '";
    result += lexeme;
    result += "'";
    if (kind == TokenKind::IntLiteral) {
        result += " (val=" + std::to_string(intValue) + ")";
    } else if (kind == TokenKind::CharLiteral) {
        result += " (char=" + std::to_string(static_cast<int>(charValue)) + ")";
    }
    return result;
}
