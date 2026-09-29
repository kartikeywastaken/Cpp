#include <cassert>
#include <iostream>
#include <string>
#include <memory>
#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "IRGenerator.h"
#include "IRPrinter.h"
#include "IRInterpreter.h"
#include "Diagnostic.h"

static int64_t compileAndRun(const std::string& src, std::string* outCaptured = nullptr) {
    DiagnosticEngine diag;
    Lexer lexer(src, "test.c", diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    assert(!diag.hasErrors());

    SemanticAnalyzer sema(diag);
    bool semaOk = sema.analyze(*prog);
    assert(semaOk);

    IRGenerator gen(diag);
    auto module = gen.generate(*prog);
    assert(!diag.hasErrors());

    IRInterpreter vm(*module);
    if (outCaptured) {
        vm.enableCapture(true);
    }
    int64_t ret = vm.run("main");
    if (outCaptured) {
        *outCaptured = vm.getCapturedOutput();
    }
    return ret;
}

void testIRArithmetic() {
    std::string src = "int main() {\n"
                      "    int a = 10, b = 3;\n"
                      "    return (a * b) + (a / b) + (a % b);\n"
                      "}";
    int64_t res = compileAndRun(src);
    assert(res == 30 + 3 + 1); // 34
    std::cout << "  [PASS] testIRArithmetic\n";
}

void testIRControlFlow() {
    std::string src = "int main() {\n"
                      "    int sum = 0;\n"
                      "    for (int i = 1; i <= 10; i++) {\n"
                      "        if (i % 2 == 0) sum += i;\n"
                      "    }\n"
                      "    return sum;\n" // 2 + 4 + 6 + 8 + 10 = 30
                      "}";
    int64_t res = compileAndRun(src);
    assert(res == 30);
    std::cout << "  [PASS] testIRControlFlow\n";
}

void testIRRecursion() {
    std::string src = "int fact(int n) {\n"
                      "    if (n <= 1) return 1;\n"
                      "    return n * fact(n - 1);\n"
                      "}\n"
                      "int main() { return fact(6); }"; // 720
    int64_t res = compileAndRun(src);
    assert(res == 720);
    std::cout << "  [PASS] testIRRecursion\n";
}

void testIRPointers() {
    std::string src = "int main() {\n"
                      "    int x = 42;\n"
                      "    int* p = &x;\n"
                      "    *p = 100;\n"
                      "    return x;\n"
                      "}";
    int64_t res = compileAndRun(src);
    assert(res == 100);
    std::cout << "  [PASS] testIRPointers\n";
}

void testIRArrays() {
    std::string src = "int main() {\n"
                      "    int a[4];\n"
                      "    a[0] = 5; a[1] = 10; a[2] = 15; a[3] = 20;\n"
                      "    return a[0] + a[1] + a[2] + a[3];\n"
                      "}";
    int64_t res = compileAndRun(src);
    assert(res == 50);
    std::cout << "  [PASS] testIRArrays\n";
}

void testIRStructs() {
    std::string src = "struct Point { int x; int y; };\n"
                      "int main() {\n"
                      "    struct Point p;\n"
                      "    p.x = 20;\n"
                      "    p.y = 30;\n"
                      "    struct Point* ptr = &p;\n"
                      "    ptr->x += 5;\n"
                      "    return ptr->x + ptr->y;\n" // 25 + 30 = 55
                      "}";
    int64_t res = compileAndRun(src);
    assert(res == 55);
    std::cout << "  [PASS] testIRStructs\n";
}

void testIRBuiltinOutput() {
    std::string src = "int main() {\n"
                      "    print_int(12345);\n"
                      "    puts(\"Hello, Compiler!\");\n"
                      "    return 0;\n"
                      "}";
    std::string captured;
    int64_t res = compileAndRun(src, &captured);
    assert(res == 0);
    assert(captured.find("12345") != std::string::npos);
    assert(captured.find("Hello, Compiler!") != std::string::npos);
    std::cout << "  [PASS] testIRBuiltinOutput\n";
}

void testIRPointerCompoundArithmetic() {
    std::string src = "int main() {\n"
                      "    int arr[5];\n"
                      "    arr[0] = 100;\n"
                      "    arr[1] = 200;\n"
                      "    int *p = arr;\n"
                      "    p += 1;\n"
                      "    return *p;\n"
                      "}";
    int64_t res = compileAndRun(src);
    assert(res == 200);
    std::cout << "  [PASS] testIRPointerCompoundArithmetic\n";
}

void testIRNestedAndLargeStructs() {
    // Nested structs
    std::string srcNested = "struct Inner { int a; int b; };\n"
                            "struct Outer { struct Inner inner; int y; };\n"
                            "int main() {\n"
                            "    struct Outer o;\n"
                            "    o.inner.a = 10;\n"
                            "    o.inner.b = 20;\n"
                            "    o.y = 30;\n"
                            "    return o.inner.a + o.inner.b + o.y;\n"
                            "}";
    int64_t resNested = compileAndRun(srcNested);
    assert(resNested == 60);

    // Large struct copy (> 8 bytes)
    std::string srcLarge = "struct Big { int a; int b; int c; int d; };\n"
                           "int main() {\n"
                           "    struct Big b1;\n"
                           "    b1.a = 1; b1.b = 2; b1.c = 3; b1.d = 4;\n"
                           "    struct Big b2 = b1;\n"
                           "    return b2.c + b2.d;\n"
                           "}";
    int64_t resLarge = compileAndRun(srcLarge);
    assert(resLarge == 7);

    std::cout << "  [PASS] testIRNestedAndLargeStructs\n";
}

void testIRGlobalConstantExpr() {
    std::string src = "int g1 = -1;\n"
                      "int g2 = 10 * 3 + 2;\n"
                      "int g3 = sizeof(int) * 2;\n"
                      "int main() {\n"
                      "    return g1 + g2 + g3;\n" // -1 + 32 + 8 = 39
                      "}";
    int64_t res = compileAndRun(src);
    assert(res == 39);
    std::cout << "  [PASS] testIRGlobalConstantExpr\n";
}

void testIRDoWhileAndTernary() {
    std::string src = "int sign(int x) {\n"
                      "    return x > 0 ? 1 : x < 0 ? -1 : 0;\n"
                      "}\n"
                      "int main() {\n"
                      "    int i = 0;\n"
                      "    int sum = 0;\n"
                      "    do {\n"
                      "        i++;\n"
                      "        if (i == 3) continue;\n"
                      "        sum += i;\n"
                      "    } while (i < 4);\n" // 1 + 2 + 4 = 7
                      "    return sum + sign(10);\n" // 7 + 1 = 8
                      "}";
    int64_t res = compileAndRun(src);
    assert(res == 8);
    std::cout << "  [PASS] testIRDoWhileAndTernary\n";
}

void runIRTests() {
    std::cout << "[RUNNING] Intermediate Representation & Interpreter Tests...\n";
    testIRArithmetic();
    testIRControlFlow();
    testIRRecursion();
    testIRPointers();
    testIRArrays();
    testIRStructs();
    testIRBuiltinOutput();
    testIRPointerCompoundArithmetic();
    testIRNestedAndLargeStructs();
    testIRGlobalConstantExpr();
    testIRDoWhileAndTernary();
}
