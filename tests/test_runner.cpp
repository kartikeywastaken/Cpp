#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>
#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "IRGenerator.h"
#include "IRInterpreter.h"
#include "Diagnostic.h"

// Forward declarations of unit test suites
void runLexerTests();
void runParserTests();
void runSemanticTests();
void runIRTests();

static bool runSampleTest(const std::string& path, int64_t expectedExitCode) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "  [FAIL] Cannot open sample file: " << path << "\n";
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string src = ss.str();

    DiagnosticEngine diag;
    diag.setSource(src, path);

    Lexer lexer(src, path, diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    if (diag.hasErrors()) {
        std::cerr << "  [FAIL] Parsing failed for: " << path << "\n";
        return false;
    }

    SemanticAnalyzer sema(diag);
    if (!sema.analyze(*prog)) {
        std::cerr << "  [FAIL] Semantic analysis failed for: " << path << "\n";
        return false;
    }

    IRGenerator gen(diag);
    auto module = gen.generate(*prog);
    if (diag.hasErrors()) {
        std::cerr << "  [FAIL] IR generation failed for: " << path << "\n";
        return false;
    }

    IRInterpreter vm(*module);
    int64_t exitCode = vm.run("main");
    if (exitCode != expectedExitCode) {
        std::cerr << "  [FAIL] " << path << ": expected " << expectedExitCode << ", got " << exitCode << "\n";
        return false;
    }

    std::cout << "  [PASS] " << path << " (exit code = " << exitCode << ")\n";
    return true;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  C COMPILER TEST SUITE\n";
    std::cout << "========================================\n\n";

    runLexerTests();
    std::cout << "\n";

    runParserTests();
    std::cout << "\n";

    runSemanticTests();
    std::cout << "\n";

    runIRTests();
    std::cout << "\n";

    std::cout << "[RUNNING] End-to-End Sample Program Tests...\n";
    bool allSamplesPassed = true;
    allSamplesPassed &= runSampleTest("tests/c_samples/01_arithmetic.c", 88);
    allSamplesPassed &= runSampleTest("tests/c_samples/02_control_flow.c", 43);
    allSamplesPassed &= runSampleTest("tests/c_samples/03_functions.c", 140);
    allSamplesPassed &= runSampleTest("tests/c_samples/04_pointers.c", 145);
    allSamplesPassed &= runSampleTest("tests/c_samples/05_arrays.c", 155);
    allSamplesPassed &= runSampleTest("tests/c_samples/06_structs.c", 107);
    allSamplesPassed &= runSampleTest("tests/c_samples/07_sort.c", 91);

    std::cout << "\n========================================\n";
    if (allSamplesPassed) {
        std::cout << "  ALL TESTS PASSED SUCCESSFULLY! (100%)\n";
        std::cout << "========================================\n";
        return 0;
    } else {
        std::cerr << "  SOME TESTS FAILED!\n";
        std::cout << "========================================\n";
        return 1;
    }
}
