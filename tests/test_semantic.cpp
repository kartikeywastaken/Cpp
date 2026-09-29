#include <cassert>
#include <iostream>
#include <string>
#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "Diagnostic.h"

void testSemanticScopingAndShadowing() {
    DiagnosticEngine diag;
    std::string src = "int main() {\n"
                      "    int x = 10;\n"
                      "    {\n"
                      "        int x = 20; // Shadowing is allowed\n"
                      "    }\n"
                      "    return x;\n"
                      "}";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    SemanticAnalyzer sema(diag);
    bool ok = sema.analyze(*prog);

    assert(ok);
    assert(!diag.hasErrors());
    std::cout << "  [PASS] testSemanticScopingAndShadowing\n";
}

void testSemanticDuplicateDeclaration() {
    DiagnosticEngine diag;
    diag.setSuppressOutput(true);
    std::string src = "int main() {\n"
                      "    int a = 1;\n"
                      "    int a = 2;\n"
                      "}";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    SemanticAnalyzer sema(diag);
    bool ok = sema.analyze(*prog);

    assert(!ok);
    assert(diag.hasErrors());
    std::cout << "  [PASS] testSemanticDuplicateDeclaration\n";
}

void testSemanticUndeclaredVariable() {
    DiagnosticEngine diag;
    diag.setSuppressOutput(true);
    std::string src = "int main() {\n"
                      "    return foo + 5;\n"
                      "}";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    SemanticAnalyzer sema(diag);
    bool ok = sema.analyze(*prog);

    assert(!ok);
    assert(diag.hasErrors());
    std::cout << "  [PASS] testSemanticUndeclaredVariable\n";
}

void testSemanticLValueEnforcement() {
    DiagnosticEngine diag;
    diag.setSuppressOutput(true);
    std::string src = "int main() {\n"
                      "    int x = 10;\n"
                      "    (x + 1) = 20;\n"
                      "}";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    SemanticAnalyzer sema(diag);
    bool ok = sema.analyze(*prog);

    assert(!ok);
    assert(diag.hasErrors());
    std::cout << "  [PASS] testSemanticLValueEnforcement\n";
}

void testSemanticStructMemberValidation() {
    DiagnosticEngine diag;
    diag.setSuppressOutput(true);
    std::string src = "struct Point { int x; int y; };\n"
                      "int main() {\n"
                      "    struct Point p;\n"
                      "    p.z = 10; // Error: Point has no member z\n"
                      "}";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    SemanticAnalyzer sema(diag);
    bool ok = sema.analyze(*prog);

    assert(!ok);
    assert(diag.hasErrors());
    std::cout << "  [PASS] testSemanticStructMemberValidation\n";
}

void testSemanticLoopControlValidation() {
    DiagnosticEngine diag;
    diag.setSuppressOutput(true);
    std::string src = "int main() {\n"
                      "    break;\n"
                      "}";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    SemanticAnalyzer sema(diag);
    bool ok = sema.analyze(*prog);

    assert(!ok);
    assert(diag.hasErrors());
    std::cout << "  [PASS] testSemanticLoopControlValidation\n";
}

void testSemanticFunctionRedefinition() {
    DiagnosticEngine diag;
    diag.setSuppressOutput(true);
    std::string src = "int foo() { return 1; }\n"
                      "int foo() { return 2; }\n"
                      "int main() { return 0; }";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    SemanticAnalyzer sema(diag);
    bool ok = sema.analyze(*prog);

    assert(!ok);
    assert(diag.hasErrors());
    std::cout << "  [PASS] testSemanticFunctionRedefinition\n";
}

void testSemanticPointerCompoundOpValidation() {
    DiagnosticEngine diag;
    diag.setSuppressOutput(true);
    std::string src = "int main() {\n"
                      "    int x = 10;\n"
                      "    int *p = &x;\n"
                      "    p *= 2;\n"
                      "}";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    SemanticAnalyzer sema(diag);
    bool ok = sema.analyze(*prog);

    assert(!ok);
    assert(diag.hasErrors());
    std::cout << "  [PASS] testSemanticPointerCompoundOpValidation\n";
}

void testSemanticNonConstantGlobalInit() {
    DiagnosticEngine diag;
    diag.setSuppressOutput(true);
    std::string src = "int foo() { return 5; }\n"
                      "int g = foo();\n"
                      "int main() { return 0; }";
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    SemanticAnalyzer sema(diag);
    bool ok = sema.analyze(*prog);

    assert(!ok);
    assert(diag.hasErrors());
    std::cout << "  [PASS] testSemanticNonConstantGlobalInit\n";
}

void runSemanticTests() {
    std::cout << "[RUNNING] Semantic Analyzer Unit Tests...\n";
    testSemanticScopingAndShadowing();
    testSemanticDuplicateDeclaration();
    testSemanticUndeclaredVariable();
    testSemanticLValueEnforcement();
    testSemanticStructMemberValidation();
    testSemanticLoopControlValidation();
    testSemanticFunctionRedefinition();
    testSemanticPointerCompoundOpValidation();
    testSemanticNonConstantGlobalInit();
}
