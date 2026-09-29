#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "Lexer.h"
#include "Diagnostic.h"

void testLexerKeywords() {
    DiagnosticEngine diag;
    std::string src = "int char void return if else while for break continue struct sizeof";
    Lexer lexer(src, "keywords.c", diag);
    auto tokens = lexer.tokenizeAll();

    assert(tokens.size() == 13); // 12 keywords + EOF
    assert(tokens[0].kind == TokenKind::KW_int);
    assert(tokens[1].kind == TokenKind::KW_char);
    assert(tokens[2].kind == TokenKind::KW_void);
    assert(tokens[3].kind == TokenKind::KW_return);
    assert(tokens[4].kind == TokenKind::KW_if);
    assert(tokens[5].kind == TokenKind::KW_else);
    assert(tokens[6].kind == TokenKind::KW_while);
    assert(tokens[7].kind == TokenKind::KW_for);
    assert(tokens[8].kind == TokenKind::KW_break);
    assert(tokens[9].kind == TokenKind::KW_continue);
    assert(tokens[10].kind == TokenKind::KW_struct);
    assert(tokens[11].kind == TokenKind::KW_sizeof);
    assert(tokens[12].kind == TokenKind::EndOfFile);
    assert(!diag.hasErrors());
    std::cout << "  [PASS] testLexerKeywords\n";
}

void testLexerLiterals() {
    DiagnosticEngine diag;
    std::string src = "42 0x2A 052 'a' '\\n' '\\x41' \"hello\\nworld\"";
    Lexer lexer(src, "literals.c", diag);
    auto tokens = lexer.tokenizeAll();

    assert(tokens[0].kind == TokenKind::IntLiteral && tokens[0].intValue == 42);
    assert(tokens[1].kind == TokenKind::IntLiteral && tokens[1].intValue == 42); // 0x2A
    assert(tokens[2].kind == TokenKind::IntLiteral && tokens[2].intValue == 42); // 052 octal
    assert(tokens[3].kind == TokenKind::CharLiteral && tokens[3].charValue == 'a');
    assert(tokens[4].kind == TokenKind::CharLiteral && tokens[4].charValue == '\n');
    assert(tokens[5].kind == TokenKind::CharLiteral && tokens[5].charValue == 'A');
    assert(tokens[6].kind == TokenKind::StringLiteral && tokens[6].stringValue == "hello\nworld");
    assert(!diag.hasErrors());
    std::cout << "  [PASS] testLexerLiterals\n";
}

void testLexerOperators() {
    DiagnosticEngine diag;
    std::string src = "+ - * / % ++ -- = += -= *= /= == != < <= > >= && || ! & | ^ ~ << >> -> .";
    Lexer lexer(src, "ops.c", diag);
    auto tokens = lexer.tokenizeAll();

    assert(tokens[0].kind == TokenKind::Plus);
    assert(tokens[1].kind == TokenKind::Minus);
    assert(tokens[2].kind == TokenKind::Star);
    assert(tokens[3].kind == TokenKind::Slash);
    assert(tokens[4].kind == TokenKind::Percent);
    assert(tokens[5].kind == TokenKind::PlusPlus);
    assert(tokens[6].kind == TokenKind::MinusMinus);
    assert(tokens[7].kind == TokenKind::Equal);
    assert(tokens[8].kind == TokenKind::PlusEqual);
    assert(tokens[9].kind == TokenKind::MinusEqual);
    assert(tokens[10].kind == TokenKind::StarEqual);
    assert(tokens[11].kind == TokenKind::SlashEqual);
    assert(tokens[12].kind == TokenKind::EqualEqual);
    assert(tokens[13].kind == TokenKind::ExclaimEqual);
    assert(tokens[14].kind == TokenKind::Less);
    assert(tokens[15].kind == TokenKind::LessEqual);
    assert(tokens[16].kind == TokenKind::Greater);
    assert(tokens[17].kind == TokenKind::GreaterEqual);
    assert(tokens[18].kind == TokenKind::AmpAmp);
    assert(tokens[19].kind == TokenKind::PipePipe);
    assert(tokens[20].kind == TokenKind::Exclaim);
    assert(tokens[21].kind == TokenKind::Amp);
    assert(tokens[22].kind == TokenKind::Pipe);
    assert(tokens[23].kind == TokenKind::Caret);
    assert(tokens[24].kind == TokenKind::Tilde);
    assert(tokens[25].kind == TokenKind::LessLess);
    assert(tokens[26].kind == TokenKind::GreaterGreater);
    assert(tokens[27].kind == TokenKind::Arrow);
    assert(tokens[28].kind == TokenKind::Dot);
    assert(!diag.hasErrors());
    std::cout << "  [PASS] testLexerOperators\n";
}

void testLexerComments() {
    DiagnosticEngine diag;
    std::string src = "// Line comment\n/* Multi-line\n comment */ int x = 5;";
    Lexer lexer(src, "comments.c", diag);
    auto tokens = lexer.tokenizeAll();

    assert(tokens[0].kind == TokenKind::KW_int);
    assert(tokens[1].kind == TokenKind::Identifier && tokens[1].lexeme == "x");
    assert(tokens[2].kind == TokenKind::Equal);
    assert(tokens[3].kind == TokenKind::IntLiteral && tokens[3].intValue == 5);
    assert(tokens[4].kind == TokenKind::Semicolon);
    assert(!diag.hasErrors());
    std::cout << "  [PASS] testLexerComments\n";
}

void testLexerErrorHandling() {
    DiagnosticEngine diag;
    diag.setSuppressOutput(true);
    std::string src = "/* Unterminated comment";
    Lexer lexer(src, "err.c", diag);
    lexer.tokenizeAll();
    assert(diag.hasErrors());
    std::cout << "  [PASS] testLexerErrorHandling\n";
}

void runLexerTests() {
    std::cout << "[RUNNING] Lexer Unit Tests...\n";
    testLexerKeywords();
    testLexerLiterals();
    testLexerOperators();
    testLexerComments();
    testLexerErrorHandling();
}
