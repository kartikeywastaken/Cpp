#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "Diagnostic.h"
#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "IRGenerator.h"
#include "IRPrinter.h"
#include "IRInterpreter.h"

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options] <source_file>\n\n"
              << "Options:\n"
              << "  --help, -h       Display this help message\n"
              << "  --lex            Tokenize input and print tokens\n"
              << "  --ast            Parse input and print AST\n"
              << "  --check          Run semantic analysis only\n"
              << "  --ir             Generate and print Intermediate Representation (IR)\n"
              << "  --run            Generate IR and execute using built-in IR interpreter\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string filename;
    bool modeLex = false;
    bool modeAst = false;
    bool modeCheck = false;
    bool modeIR = false;
    bool modeRun = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--lex") {
            modeLex = true;
        } else if (arg == "--ast") {
            modeAst = true;
        } else if (arg == "--check") {
            modeCheck = true;
        } else if (arg == "--ir") {
            modeIR = true;
        } else if (arg == "--run") {
            modeRun = true;
        } else if (arg != "-" && !arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            filename = arg;
        }
    }

    if (filename.empty()) {
        std::cerr << "Error: no input file specified.\n";
        printUsage(argv[0]);
        return 1;
    }

    // Default to --ir if no specific mode selected
    if (!modeLex && !modeAst && !modeCheck && !modeIR && !modeRun) {
        modeIR = true;
    }

    std::string source;
    if (filename == "-") {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        source = ss.str();
        filename = "<stdin>";
    } else {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: cannot open file '" << filename << "'\n";
            return 1;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        source = ss.str();
    }

    DiagnosticEngine diag;
    diag.setSource(source, filename);

    if (modeLex) {
        Lexer lexer(source, filename, diag);
        std::vector<Token> tokens = lexer.tokenizeAll();
        for (const auto& tok : tokens) {
            std::cout << tok.toString() << "\n";
        }
        return diag.hasErrors() ? 1 : 0;
    }

    Lexer lexer(source, filename, diag);
    Parser parser(lexer, diag);
    auto program = parser.parseProgram();

    if (diag.hasErrors()) {
        std::cerr << "Compilation failed with " << diag.getErrorCount() << " error(s).\n";
        return 1;
    }

    if (modeAst) {
        program->dump(std::cout);
        return 0;
    }

    SemanticAnalyzer sema(diag);
    if (!sema.analyze(*program)) {
        std::cerr << "Semantic analysis failed with " << diag.getErrorCount() << " error(s).\n";
        return 1;
    }

    if (modeCheck) {
        std::cout << "Semantic analysis passed successfully.\n";
        return 0;
    }

    IRGenerator irGen(diag);
    auto irModule = irGen.generate(*program);

    if (diag.hasErrors()) {
        std::cerr << "IR generation failed with " << diag.getErrorCount() << " error(s).\n";
        return 1;
    }

    if (modeIR) {
        IRPrinter printer(std::cout);
        printer.print(*irModule);
        return 0;
    }

    if (modeRun) {
        IRInterpreter vm(*irModule);
        try {
            int64_t ret = vm.run("main");
            std::cout << "[Program exited with return code: " << ret << "]\n";
            return static_cast<int>(ret);
        } catch (const std::exception& e) {
            std::cerr << "Runtime error: " << e.what() << "\n";
            return 1;
        }
    }

    return 0;
}
