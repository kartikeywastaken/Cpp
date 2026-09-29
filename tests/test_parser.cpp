#include <cassert>
#include <iostream>
#include <string>
#include <memory>
#include "Lexer.h"
#include "Parser.h"
#include "AST.h"
#include "Diagnostic.h"

void testParserPrecedence() {
    DiagnosticEngine diag;
    std::string src = "int main() { int x = a + b * c; }";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto program = parser.parseProgram();

    assert(!diag.hasErrors());
    assert(program->getDeclarations().size() == 1);

    auto fn = std::static_pointer_cast<FunctionDecl>(program->getDeclarations()[0]);
    auto body = fn->getBody();
    assert(body->getStatements().size() == 1);

    auto declStmt = std::static_pointer_cast<DeclStmt>(body->getStatements()[0]);
    auto init = declStmt->getDeclarations()[0]->getInitExpr();
    assert(init->getKind() == ASTKind::BinaryExpr);

    auto addExpr = std::static_pointer_cast<BinaryExpr>(init);
    assert(addExpr->getOp() == BinaryOp::Add);
    assert(addExpr->getLeft()->getKind() == ASTKind::VarExpr);
    assert(addExpr->getRight()->getKind() == ASTKind::BinaryExpr);

    auto mulExpr = std::static_pointer_cast<BinaryExpr>(addExpr->getRight());
    assert(mulExpr->getOp() == BinaryOp::Mul);

    std::cout << "  [PASS] testParserPrecedence\n";
}

void testParserRightAssociativeAssignment() {
    DiagnosticEngine diag;
    std::string src = "int main() { a = b = c; }";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto program = parser.parseProgram();

    assert(!diag.hasErrors());
    auto fn = std::static_pointer_cast<FunctionDecl>(program->getDeclarations()[0]);
    auto exprStmt = std::static_pointer_cast<ExprStmt>(fn->getBody()->getStatements()[0]);
    auto assign1 = std::static_pointer_cast<AssignExpr>(exprStmt->getExpr());

    assert(assign1->getLhs()->getKind() == ASTKind::VarExpr);
    assert(assign1->getRhs()->getKind() == ASTKind::AssignExpr);

    std::cout << "  [PASS] testParserRightAssociativeAssignment\n";
}

void testParserStructAndPointers() {
    DiagnosticEngine diag;
    std::string src = "struct Point { int x; int y; };\n"
                      "int getX(struct Point* p) { return p->x; }";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto program = parser.parseProgram();

    assert(!diag.hasErrors());
    assert(program->getDeclarations().size() == 2);
    assert(program->getDeclarations()[0]->getKind() == ASTKind::StructDecl);
    assert(program->getDeclarations()[1]->getKind() == ASTKind::FunctionDecl);

    std::cout << "  [PASS] testParserStructAndPointers\n";
}

void testParserControlFlow() {
    DiagnosticEngine diag;
    std::string src = "int main() {\n"
                      "    if (x > 0) return 1; else return 0;\n"
                      "    while (x < 10) x++;\n"
                      "    for (int i = 0; i < 5; i++) continue;\n"
                      "}";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto program = parser.parseProgram();

    assert(!diag.hasErrors());
    auto fn = std::static_pointer_cast<FunctionDecl>(program->getDeclarations()[0]);
    assert(fn->getBody()->getStatements().size() == 3);
    assert(fn->getBody()->getStatements()[0]->getKind() == ASTKind::IfStmt);
    assert(fn->getBody()->getStatements()[1]->getKind() == ASTKind::WhileStmt);
    assert(fn->getBody()->getStatements()[2]->getKind() == ASTKind::ForStmt);

    std::cout << "  [PASS] testParserControlFlow\n";
}

void testParserMultipleDeclarators() {
    DiagnosticEngine diag;
    std::string src = "int main() { int *a, b; int *c, *d; }";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto program = parser.parseProgram();

    assert(!diag.hasErrors());
    auto fn = std::static_pointer_cast<FunctionDecl>(program->getDeclarations()[0]);
    auto declStmt1 = std::static_pointer_cast<DeclStmt>(fn->getBody()->getStatements()[0]);
    auto declStmt2 = std::static_pointer_cast<DeclStmt>(fn->getBody()->getStatements()[1]);

    assert(declStmt1->getDeclarations()[0]->getType()->isPointer());
    assert(declStmt1->getDeclarations()[1]->getType()->isInt());
    assert(declStmt2->getDeclarations()[0]->getType()->isPointer());
    assert(declStmt2->getDeclarations()[1]->getType()->isPointer());

    std::cout << "  [PASS] testParserMultipleDeclarators\n";
}

void testParserDoWhileAndTernary() {
    DiagnosticEngine diag;
    std::string src = "int main() {\n"
                      "    do { x++; } while (x < 10);\n"
                      "    int y = (x > 5) ? 1 : 2;\n"
                      "}";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto program = parser.parseProgram();

    assert(!diag.hasErrors());
    auto fn = std::static_pointer_cast<FunctionDecl>(program->getDeclarations()[0]);
    assert(fn->getBody()->getStatements()[0]->getKind() == ASTKind::DoWhileStmt);

    auto declStmt = std::static_pointer_cast<DeclStmt>(fn->getBody()->getStatements()[1]);
    assert(declStmt->getDeclarations()[0]->getInitExpr()->getKind() == ASTKind::ConditionalExpr);

    std::cout << "  [PASS] testParserDoWhileAndTernary\n";
}

void runParserTests() {
    std::cout << "[RUNNING] Parser Unit Tests...\n";
    testParserPrecedence();
    testParserRightAssociativeAssignment();
    testParserStructAndPointers();
    testParserControlFlow();
    testParserMultipleDeclarators();
    testParserDoWhileAndTernary();
}
