#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <unistd.h>

#include "Diagnostic.h"
#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "IRGenerator.h"
#include "IRPrinter.h"
#include "IRInterpreter.h"
#include "CodeGen.h"

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options] <source_file>\n\n"
              << "Compilation Options:\n"
              << "  -o <file>        Place output into <file> (executable, object, or assembly)\n"
              << "  -S               Compile only; emit native assembly (.s)\n"
              << "  -c               Compile and assemble; produce object file (.o)\n"
              << "  --target <arch>  Target architecture: 'arm64' or 'x86_64' (default: host)\n"
              << "  --os <system>    Target operating system: 'macos' or 'linux' (default: host)\n"
              << "  --help, -h       Display this help message\n\n"
              << "Pipeline Inspection Options:\n"
              << "  --lex            Tokenize input and print tokens with coordinates\n"
              << "  --ast            Parse input and print Abstract Syntax Tree (AST)\n"
              << "  --check          Run semantic analysis and type checking only\n"
              << "  --ir             Generate and print Intermediate Representation (IR)\n"
              << "  --run            Generate IR and execute using built-in IR interpreter\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string filename;
    std::string outputFile;
    TargetArch targetArch = getHostTargetArch();
    TargetOS targetOS = getHostTargetOS();

    bool modeLex = false;
    bool modeAst = false;
    bool modeCheck = false;
    bool modeIR = false;
    bool modeRun = false;
    bool modeAsm = false; // -S
    bool modeObj = false; // -c

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
        } else if (arg == "-S") {
            modeAsm = true;
        } else if (arg == "-c") {
            modeObj = true;
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                outputFile = argv[++i];
            } else {
                std::cerr << "Error: -o requires an argument\n";
                return 1;
            }
        } else if (arg.rfind("-o", 0) == 0 && arg.size() > 2) {
            outputFile = arg.substr(2);
        } else if (arg == "--target") {
            if (i + 1 < argc) {
                std::string t = argv[++i];
                if (t == "arm64" || t == "aarch64") {
                    targetArch = TargetArch::ARM64;
                } else if (t == "x86_64" || t == "x86-64" || t == "amd64") {
                    targetArch = TargetArch::X86_64;
                } else {
                    std::cerr << "Error: unsupported target architecture: " << t << "\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --target requires an argument\n";
                return 1;
            }
        } else if (arg == "--os") {
            if (i + 1 < argc) {
                std::string osName = argv[++i];
                if (osName == "macos" || osName == "darwin" || osName == "apple") {
                    targetOS = TargetOS::macOS;
                } else if (osName == "linux") {
                    targetOS = TargetOS::Linux;
                } else {
                    std::cerr << "Error: unsupported target OS: " << osName << " (must be 'macos' or 'linux')\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --os requires an argument\n";
                return 1;
            }
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

    std::string baseName = filename;
    if (baseName != "-" && baseName.rfind('.') != std::string::npos) {
        baseName = baseName.substr(0, baseName.rfind('.'));
    }

    if (modeAsm) {
        if (outputFile.empty()) {
            outputFile = (filename == "-") ? "-" : (baseName + ".s");
        }
    } else if (modeObj) {
        if (outputFile.empty()) {
            outputFile = (filename == "-") ? "a.o" : (baseName + ".o");
        }
    } else if (!modeLex && !modeAst && !modeCheck && !modeIR && !modeRun) {
        if (outputFile.empty()) {
            outputFile = "a.out";
        }
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

    // Backend Code Generation
    auto codegen = createCodeGenerator(targetArch, targetOS);
    std::ostringstream asmStream;
    codegen->generate(*irModule, asmStream);
    std::string assembly = asmStream.str();

    // Mode -S: Emit assembly file
    if (modeAsm) {
        if (outputFile == "-") {
            std::cout << assembly;
        } else {
            std::ofstream out(outputFile);
            if (!out.is_open()) {
                std::cerr << "Error: cannot open output file '" << outputFile << "' for writing.\n";
                return 1;
            }
            out << assembly;
            std::cout << "Assembly written to " << outputFile << "\n";
        }
        return 0;
    }

    // Assemble / Link using system assembler and linker (clang driver)
    char tempAsmTemplate[] = "/tmp/c_compiler_XXXXXX.s";
    int fd = mkstemps(tempAsmTemplate, 2);
    if (fd == -1) {
        std::cerr << "Error: failed to create temporary assembly file.\n";
        return 1;
    }
    write(fd, assembly.c_str(), assembly.size());
    close(fd);

    std::string archFlag;
    if (targetOS == TargetOS::macOS) {
        archFlag = (targetArch == TargetArch::ARM64) ? "-arch arm64" : "-arch x86_64";
    } else {
        archFlag = (targetArch == TargetArch::ARM64) ? "--target=aarch64-linux-gnu" : "--target=x86_64-linux-gnu";
    }

    std::string cmd;
    if (modeObj) {
        cmd = "clang " + archFlag + " -c \"" + tempAsmTemplate + "\" -o \"" + outputFile + "\"";
    } else {
        cmd = "clang " + archFlag + " \"" + tempAsmTemplate + "\" -o \"" + outputFile + "\"";
    }

    int res = std::system(cmd.c_str());
    unlink(tempAsmTemplate);

    if (res != 0) {
        std::cerr << "Error: system assembler/linker failed.\n";
        return 1;
    }

    if (modeObj) {
        std::cout << "Object file written to " << outputFile << "\n";
    } else {
        std::cout << "Executable binary written to " << outputFile << "\n";
    }

    return 0;
}
