#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <memory>
#include "Token.h"
#include "Lexer.h"
#include "AST.h"
#include "Diagnostic.h"

class Parser {
public:
    Parser(Lexer& lexer, DiagnosticEngine& diag);

    std::shared_ptr<Program> parseProgram();

private:
    // Token navigation
    const Token& current() const;
    const Token& peek(size_t offset = 0) const;
    Token advance();
    bool check(TokenKind kind) const;
    bool match(TokenKind kind);
    Token consume(TokenKind kind, const std::string& errorMessage);

    void synchronize();

    // Declarations
    std::vector<DeclPtr> parseDeclaration();
    std::shared_ptr<StructDecl> parseStructDeclaration();
    std::vector<DeclPtr> parseFunctionOrVarDeclaration();
    std::shared_ptr<ParamDecl> parseParam();

    // Types
    TypePtr parseTypeSpecifier();
    TypePtr parseTypeName();
    TypePtr parseDeclarator(TypePtr baseType, std::string& outName);
    bool isTypeSpecifier(TokenKind kind) const;

    // Statements
    StmtPtr parseStatement();
    std::shared_ptr<CompoundStmt> parseCompoundStatement();
    StmtPtr parseIfStatement();
    StmtPtr parseWhileStatement();
    StmtPtr parseDoWhileStatement();
    StmtPtr parseForStatement();
    StmtPtr parseReturnStatement();
    StmtPtr parseBreakStatement();
    StmtPtr parseContinueStatement();
    StmtPtr parseExprOrDeclStatement();
    std::shared_ptr<DeclStmt> parseVarDeclStatement();

    // Expressions
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseConditional();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseBitwiseOr();
    ExprPtr parseBitwiseXor();
    ExprPtr parseBitwiseAnd();
    ExprPtr parseEquality();
    ExprPtr parseRelational();
    ExprPtr parseShift();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parseUnary();
    ExprPtr parseCast();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();

    [[maybe_unused]] Lexer& lexer;
    DiagnosticEngine& diag;
    std::vector<Token> tokens;
    size_t cursor = 0;
};

#endif // PARSER_H
