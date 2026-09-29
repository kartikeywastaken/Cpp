#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include "Token.h"
#include "Diagnostic.h"

class Lexer {
public:
    Lexer(std::string_view source, std::string filename, DiagnosticEngine& diag);

    Token nextToken();
    Token peekToken();
    std::vector<Token> tokenizeAll();

    const std::string& getFilename() const { return filename; }

private:
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    bool isAtEnd() const;

    void skipWhitespaceAndComments();
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanCharLiteral();
    Token scanStringLiteral();
    char parseEscapeSequence();

    SourceLocation currentLocation() const;

    std::string_view source;
    std::string filename;
    DiagnosticEngine& diag;

    size_t cursor = 0;
    int line = 1;
    int column = 1;

    std::optional<Token> peekedToken;
};

#endif // LEXER_H
