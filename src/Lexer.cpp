#include "Lexer.h"
#include <cctype>
#include <unordered_map>
#include <cstdlib>

static const std::unordered_map<std::string_view, TokenKind> keywords = {
    {"int",      TokenKind::KW_int},
    {"char",     TokenKind::KW_char},
    {"void",     TokenKind::KW_void},
    {"return",   TokenKind::KW_return},
    {"if",       TokenKind::KW_if},
    {"else",     TokenKind::KW_else},
    {"while",    TokenKind::KW_while},
    {"do",       TokenKind::KW_do},
    {"for",      TokenKind::KW_for},
    {"break",    TokenKind::KW_break},
    {"continue", TokenKind::KW_continue},
    {"struct",   TokenKind::KW_struct},
    {"sizeof",   TokenKind::KW_sizeof}
};

Lexer::Lexer(std::string_view source, std::string filename, DiagnosticEngine& diag)
    : source(source), filename(std::move(filename)), diag(diag) {
    diag.setSource(std::string(source), this->filename);
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[cursor];
}

char Lexer::peekNext() const {
    if (cursor + 1 >= source.size()) return '\0';
    return source[cursor + 1];
}

char Lexer::advance() {
    if (isAtEnd()) return '\0';
    char c = source[cursor++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source[cursor] != expected) return false;
    advance();
    return true;
}

bool Lexer::isAtEnd() const {
    return cursor >= source.size();
}

SourceLocation Lexer::currentLocation() const {
    return {filename, line, column};
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            // Line comment: skip until newline
            advance(); advance();
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peekNext() == '*') {
            // Block comment
            SourceLocation startLoc = currentLocation();
            advance(); advance();
            bool closed = false;
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance(); advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                diag.error(startLoc, "unterminated /* comment");
            }
        } else {
            break;
        }
    }
}

Token Lexer::scanIdentifierOrKeyword() {
    SourceLocation loc = currentLocation();
    size_t start = cursor;
    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        advance();
    }
    std::string_view text = source.substr(start, cursor - start);
    auto it = keywords.find(text);
    TokenKind kind = (it != keywords.end()) ? it->second : TokenKind::Identifier;

    Token token;
    token.kind = kind;
    token.lexeme = std::string(text);
    token.location = loc;
    return token;
}

Token Lexer::scanNumber() {
    SourceLocation loc = currentLocation();
    size_t start = cursor;
    int base = 10;
    bool invalidOctal = false;

    if (peek() == '0') {
        if (peekNext() == 'x' || peekNext() == 'X') {
            base = 16;
            advance(); advance(); // Skip 0x
            if (!std::isxdigit(static_cast<unsigned char>(peek()))) {
                diag.error(loc, "hexadecimal literal has no digits");
            }
            while (!isAtEnd() && std::isxdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        } else {
            base = 8;
            while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                char d = advance();
                if (d == '8' || d == '9') {
                    invalidOctal = true;
                }
            }
        }
    } else {
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    std::string_view text = source.substr(start, cursor - start);
    int64_t val = 0;
    if (invalidOctal) {
        diag.error(loc, "invalid digit in octal constant: " + std::string(text));
    } else {
        try {
            if (base == 16) {
                std::string hexStr(text.substr(2));
                val = hexStr.empty() ? 0 : std::stoll(hexStr, nullptr, 16);
            } else if (base == 8) {
                val = std::stoll(std::string(text), nullptr, 8);
            } else {
                val = std::stoll(std::string(text), nullptr, 10);
            }
        } catch (...) {
            diag.error(loc, "integer literal out of range: " + std::string(text));
        }
    }

    Token token;
    token.kind = TokenKind::IntLiteral;
    token.lexeme = std::string(text);
    token.location = loc;
    token.intValue = val;
    return token;
}

char Lexer::parseEscapeSequence() {
    SourceLocation loc = currentLocation();
    if (isAtEnd()) {
        diag.error(loc, "unfinished escape sequence");
        return '\0';
    }
    char c = advance();
    switch (c) {
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'r':  return '\r';
        case '0':  return '\0';
        case '\\': return '\\';
        case '\'': return '\'';
        case '\"': return '\"';
        case 'a':  return '\a';
        case 'b':  return '\b';
        case 'f':  return '\f';
        case 'v':  return '\v';
        case 'x': {
            // Hex escape sequence \xHH
            std::string hexDigits;
            while (!isAtEnd() && std::isxdigit(static_cast<unsigned char>(peek())) && hexDigits.size() < 2) {
                hexDigits.push_back(advance());
            }
            if (hexDigits.empty()) {
                diag.error(loc, "\\x used with no following hex digits");
                return '\0';
            }
            return static_cast<char>(std::strtol(hexDigits.c_str(), nullptr, 16));
        }
        default:
            diag.warning(loc, "unknown escape sequence '\\" + std::string(1, c) + "'");
            return c;
    }
}

Token Lexer::scanCharLiteral() {
    SourceLocation loc = currentLocation();
    advance(); // Consume opening '
    char val = '\0';
    if (peek() == '\'') {
        diag.error(loc, "empty character literal");
        advance();
    } else if (peek() == '\\') {
        advance(); // consume '\'
        val = parseEscapeSequence();
        if (peek() == '\'') {
            advance();
        } else {
            diag.error(loc, "unterminated character literal");
        }
    } else if (!isAtEnd() && peek() != '\n') {
        val = advance();
        if (peek() == '\'') {
            advance();
        } else {
            diag.error(loc, "unterminated character literal");
        }
    } else {
        diag.error(loc, "unterminated character literal");
    }

    Token token;
    token.kind = TokenKind::CharLiteral;
    token.lexeme = "'" + std::string(1, val) + "'";
    token.location = loc;
    token.charValue = val;
    token.intValue = static_cast<int64_t>(val);
    return token;
}

Token Lexer::scanStringLiteral() {
    SourceLocation loc = currentLocation();
    advance(); // Consume opening "
    std::string str;
    bool closed = false;

    while (!isAtEnd()) {
        char c = peek();
        if (c == '"') {
            advance();
            closed = true;
            break;
        }
        if (c == '\n') {
            diag.error(loc, "newline in string literal");
            break;
        }
        if (c == '\\') {
            advance(); // consume '\'
            str.push_back(parseEscapeSequence());
        } else {
            str.push_back(advance());
        }
    }

    if (!closed) {
        diag.error(loc, "unterminated string literal");
    }

    Token token;
    token.kind = TokenKind::StringLiteral;
    token.lexeme = "\"" + str + "\"";
    token.location = loc;
    token.stringValue = str;
    return token;
}

Token Lexer::peekToken() {
    if (!peekedToken.has_value()) {
        peekedToken = nextToken();
    }
    return *peekedToken;
}

Token Lexer::nextToken() {
    if (peekedToken.has_value()) {
        Token t = *peekedToken;
        peekedToken.reset();
        return t;
    }

    skipWhitespaceAndComments();

    if (isAtEnd()) {
        Token token;
        token.kind = TokenKind::EndOfFile;
        token.lexeme = "";
        token.location = currentLocation();
        return token;
    }

    SourceLocation loc = currentLocation();
    char c = peek();

    // Identifiers or keywords
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return scanIdentifierOrKeyword();
    }

    // Numbers
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return scanNumber();
    }

    // Literals
    if (c == '\'') {
        return scanCharLiteral();
    }
    if (c == '"') {
        return scanStringLiteral();
    }

    // Operators and delimiters
    advance(); // consume c
    Token token;
    token.location = loc;

    switch (c) {
        case '+':
            if (match('+')) token.kind = TokenKind::PlusPlus;
            else if (match('=')) token.kind = TokenKind::PlusEqual;
            else token.kind = TokenKind::Plus;
            break;
        case '-':
            if (match('-')) token.kind = TokenKind::MinusMinus;
            else if (match('>')) token.kind = TokenKind::Arrow;
            else if (match('=')) token.kind = TokenKind::MinusEqual;
            else token.kind = TokenKind::Minus;
            break;
        case '*':
            if (match('=')) token.kind = TokenKind::StarEqual;
            else token.kind = TokenKind::Star;
            break;
        case '/':
            if (match('=')) token.kind = TokenKind::SlashEqual;
            else token.kind = TokenKind::Slash;
            break;
        case '%':
            if (match('=')) token.kind = TokenKind::PercentEqual;
            else token.kind = TokenKind::Percent;
            break;
        case '=':
            if (match('=')) token.kind = TokenKind::EqualEqual;
            else token.kind = TokenKind::Equal;
            break;
        case '!':
            if (match('=')) token.kind = TokenKind::ExclaimEqual;
            else token.kind = TokenKind::Exclaim;
            break;
        case '<':
            if (match('<')) {
                if (match('=')) token.kind = TokenKind::LessLessEqual;
                else token.kind = TokenKind::LessLess;
            } else if (match('=')) {
                token.kind = TokenKind::LessEqual;
            } else {
                token.kind = TokenKind::Less;
            }
            break;
        case '>':
            if (match('>')) {
                if (match('=')) token.kind = TokenKind::GreaterGreaterEqual;
                else token.kind = TokenKind::GreaterGreater;
            } else if (match('=')) {
                token.kind = TokenKind::GreaterEqual;
            } else {
                token.kind = TokenKind::Greater;
            }
            break;
        case '&':
            if (match('&')) token.kind = TokenKind::AmpAmp;
            else if (match('=')) token.kind = TokenKind::AmpEqual;
            else token.kind = TokenKind::Amp;
            break;
        case '|':
            if (match('|')) token.kind = TokenKind::PipePipe;
            else if (match('=')) token.kind = TokenKind::PipeEqual;
            else token.kind = TokenKind::Pipe;
            break;
        case '^':
            if (match('=')) token.kind = TokenKind::CaretEqual;
            else token.kind = TokenKind::Caret;
            break;
        case '~':
            token.kind = TokenKind::Tilde;
            break;
        case '.':
            token.kind = TokenKind::Dot;
            break;
        case '(':
            token.kind = TokenKind::LParen;
            break;
        case ')':
            token.kind = TokenKind::RParen;
            break;
        case '[':
            token.kind = TokenKind::LBracket;
            break;
        case ']':
            token.kind = TokenKind::RBracket;
            break;
        case '{':
            token.kind = TokenKind::LBrace;
            break;
        case '}':
            token.kind = TokenKind::RBrace;
            break;
        case ';':
            token.kind = TokenKind::Semicolon;
            break;
        case ',':
            token.kind = TokenKind::Comma;
            break;
        case '?':
            token.kind = TokenKind::Question;
            break;
        case ':':
            token.kind = TokenKind::Colon;
            break;
        default:
            token.kind = TokenKind::Unknown;
            diag.error(loc, std::string("unrecognized character '") + c + "'");
            break;
    }

    token.lexeme = std::string(tokenKindToString(token.kind));
    return token;
}

std::vector<Token> Lexer::tokenizeAll() {
    std::vector<Token> tokens;
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.kind == TokenKind::EndOfFile) {
            break;
        }
    }
    return tokens;
}
