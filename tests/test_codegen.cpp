#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "IRGenerator.h"
#include "CodeGen.h"
#include "CodeGenARM64.h"
#include "CodeGenX86.h"
#include "Diagnostic.h"

static bool compileSourceToAssembly(const std::string& src,
                                   const std::string& filename,
                                   TargetArch arch,
                                   TargetOS os,
                                   std::string& outAssembly) {
    DiagnosticEngine diag;
    diag.setSource(src, filename);

    Lexer lexer(src, filename, diag);
    Parser parser(lexer, diag);
    auto prog = parser.parseProgram();
    if (diag.hasErrors()) {
        std::cerr << "  [FAIL] Parsing failed for: " << filename << "\n";
        return false;
    }

    SemanticAnalyzer sema(diag);
    if (!sema.analyze(*prog)) {
        std::cerr << "  [FAIL] Semantic analysis failed for: " << filename << "\n";
        return false;
    }

    IRGenerator gen(diag);
    auto module = gen.generate(*prog);
    if (diag.hasErrors()) {
        std::cerr << "  [FAIL] IR generation failed for: " << filename << "\n";
        return false;
    }

    auto codegen = createCodeGenerator(arch, os);
    std::ostringstream ss;
    codegen->generate(*module, ss);
    outAssembly = ss.str();
    return true;
}

static bool compileAndRunNative(const std::string& srcPath,
                                int64_t expectedExitCode,
                                TargetArch arch = getHostTargetArch(),
                                TargetOS os = getHostTargetOS(),
                                std::string* outStdout = nullptr) {
    std::ifstream file(srcPath);
    if (!file.is_open()) {
        std::cerr << "  [FAIL] Cannot open sample file: " << srcPath << "\n";
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string src = ss.str();

    std::string assembly;
    if (!compileSourceToAssembly(src, srcPath, arch, os, assembly)) {
        return false;
    }

    // Write assembly to temporary file
    char asmPath[] = "/tmp/c_compiler_test_XXXXXX.s";
    int fd = mkstemps(asmPath, 2);
    if (fd == -1) {
        std::cerr << "  [FAIL] Failed to create temp asm file\n";
        return false;
    }
    write(fd, assembly.c_str(), assembly.size());
    close(fd);

    std::string binPath = std::string(asmPath) + ".bin";

    // Invoke clang to assemble and link
    std::string archFlag = (arch == TargetArch::ARM64) ? "-arch arm64" : "-arch x86_64";
    std::string cmd = "clang " + archFlag + " " + asmPath + " -o " + binPath;
    int linkRes = std::system(cmd.c_str());
    if (linkRes != 0) {
        std::cerr << "  [FAIL] Clang assemble/link failed for " << srcPath << "\n";
        std::cerr << "--- Generated Assembly ---\n" << assembly << "\n--------------------------\n";
        unlink(asmPath);
        return false;
    }
    unlink(asmPath);

    int exitCode = -1;
    if (outStdout) {
        FILE* pipe = popen(binPath.c_str(), "r");
        if (!pipe) {
            std::cerr << "  [FAIL] Cannot popen " << binPath << "\n";
            unlink(binPath.c_str());
            return false;
        }
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            *outStdout += buffer;
        }
        int status = pclose(pipe);
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
        }
    } else {
        int status = std::system(binPath.c_str());
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
        }
    }

    unlink(binPath.c_str());

    if (exitCode != expectedExitCode) {
        std::cerr << "  [FAIL] " << srcPath << " (" << targetArchToString(arch)
                  << "): expected exit " << expectedExitCode << ", got " << exitCode << "\n";
        return false;
    }

    std::cout << "  [PASS] Native " << targetArchToString(arch) << ": " << srcPath
              << " (exit code = " << exitCode << ")\n";
    return true;
}

static bool testNativeString(const std::string& src,
                             int64_t expectedExitCode,
                             TargetArch arch = getHostTargetArch(),
                             TargetOS os = getHostTargetOS(),
                             std::string* outStdout = nullptr) {
    std::string assembly;
    if (!compileSourceToAssembly(src, "inline_test.c", arch, os, assembly)) {
        return false;
    }

    char asmPath[] = "/tmp/c_compiler_inline_XXXXXX.s";
    int fd = mkstemps(asmPath, 2);
    if (fd == -1) return false;
    write(fd, assembly.c_str(), assembly.size());
    close(fd);

    std::string binPath = std::string(asmPath) + ".bin";
    std::string archFlag = (arch == TargetArch::ARM64) ? "-arch arm64" : "-arch x86_64";
    std::string cmd = "clang " + archFlag + " " + asmPath + " -o " + binPath + " 2>/dev/null";
    int linkRes = std::system(cmd.c_str());
    unlink(asmPath);

    if (linkRes != 0) {
        return false;
    }

    int exitCode = -1;
    if (outStdout) {
        FILE* pipe = popen(binPath.c_str(), "r");
        if (!pipe) {
            unlink(binPath.c_str());
            return false;
        }
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            *outStdout += buffer;
        }
        int status = pclose(pipe);
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
        }
    } else {
        int status = std::system(binPath.c_str());
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
        }
    }

    unlink(binPath.c_str());
    return (exitCode == expectedExitCode);
}

void testNativeSamplesARM64() {
    std::cout << "[RUNNING] Native ARM64 End-to-End Sample Execution Tests...\n";
    assert(compileAndRunNative("tests/c_samples/01_arithmetic.c", 88, TargetArch::ARM64));
    assert(compileAndRunNative("tests/c_samples/02_control_flow.c", 43, TargetArch::ARM64));
    assert(compileAndRunNative("tests/c_samples/03_functions.c", 140, TargetArch::ARM64));
    assert(compileAndRunNative("tests/c_samples/04_pointers.c", 145, TargetArch::ARM64));
    assert(compileAndRunNative("tests/c_samples/05_arrays.c", 155, TargetArch::ARM64));
    assert(compileAndRunNative("tests/c_samples/06_structs.c", 107, TargetArch::ARM64));
    assert(compileAndRunNative("tests/c_samples/07_sort.c", 91, TargetArch::ARM64));
}

void testNativeBuiltinsARM64() {
    std::cout << "[RUNNING] Native ARM64 Builtin Output Tests...\n";
    std::string src = "int main() {\n"
                      "    print_int(12345);\n"
                      "    print_int(-42);\n"
                      "    print_char('X');\n"
                      "    print_char(10);\n"
                      "    print_str(\"Hello Native ARM64!\\n\");\n"
                      "    puts(\"Puts test works!\");\n"
                      "    return 0;\n"
                      "}";
    std::string captured;
    bool ok = testNativeString(src, 0, TargetArch::ARM64, TargetOS::macOS, &captured);
    assert(ok);
    assert(captured.find("12345\n") != std::string::npos);
    assert(captured.find("-42\n") != std::string::npos);
    assert(captured.find("X\n") != std::string::npos);
    assert(captured.find("Hello Native ARM64!\n") != std::string::npos);
    assert(captured.find("Puts test works!\n") != std::string::npos);
    std::cout << "  [PASS] Native ARM64 Builtins Verified\n";
}

void testNativeSamplesX86_64() {
    std::cout << "[RUNNING] Native x86_64 End-to-End Sample Execution Tests...\n";
    assert(compileAndRunNative("tests/c_samples/01_arithmetic.c", 88, TargetArch::X86_64));
    assert(compileAndRunNative("tests/c_samples/02_control_flow.c", 43, TargetArch::X86_64));
    assert(compileAndRunNative("tests/c_samples/03_functions.c", 140, TargetArch::X86_64));
    assert(compileAndRunNative("tests/c_samples/04_pointers.c", 145, TargetArch::X86_64));
    assert(compileAndRunNative("tests/c_samples/05_arrays.c", 155, TargetArch::X86_64));
    assert(compileAndRunNative("tests/c_samples/06_structs.c", 107, TargetArch::X86_64));
    assert(compileAndRunNative("tests/c_samples/07_sort.c", 91, TargetArch::X86_64));
}

void testNativeBuiltinsX86_64() {
    std::cout << "[RUNNING] Native x86_64 Builtin Output Tests...\n";
    std::string src = "int main() {\n"
                      "    print_int(67890);\n"
                      "    print_int(-100);\n"
                      "    print_str(\"Hello Native x86_64!\\n\");\n"
                      "    return 0;\n"
                      "}";
    std::string captured;
    bool ok = testNativeString(src, 0, TargetArch::X86_64, TargetOS::macOS, &captured);
    assert(ok);
    assert(captured.find("67890\n") != std::string::npos);
    assert(captured.find("-100\n") != std::string::npos);
    assert(captured.find("Hello Native x86_64!\n") != std::string::npos);
    std::cout << "  [PASS] Native x86_64 Builtins Verified\n";
}

void testNativeEdgeCases() {
    std::cout << "[RUNNING] Native CodeGen Edge Case Tests...\n";

    // 1. Many arguments (> 8 parameters to exercise stack calling convention)
    std::string srcManyArgs =
        "int sum9(int a, int b, int c, int d, int e, int f, int g, int h, int i) {\n"
        "    return a + b + c + d + e + f + g + h + i;\n"
        "}\n"
        "int main() {\n"
        "    return sum9(1, 2, 3, 4, 5, 6, 7, 8, 9);\n" // 45
        "}";
    assert(testNativeString(srcManyArgs, 45, TargetArch::ARM64));
    assert(testNativeString(srcManyArgs, 45, TargetArch::X86_64));
    std::cout << "  [PASS] Stack Arguments (>8 args) Calling Convention\n";

    // 2. Division by zero handling
    std::string srcDivZero =
        "int main() {\n"
        "    int zero = 0;\n"
        "    int a = 42 / zero;\n"
        "    int b = 42 % zero;\n"
        "    return a + b;\n" // 0 + 0 = 0
        "}";
    assert(testNativeString(srcDivZero, 0, TargetArch::ARM64));
    assert(testNativeString(srcDivZero, 0, TargetArch::X86_64));
    std::cout << "  [PASS] Safe Division and Modulo by Zero\n";

    // 3. Globals with initializers
    std::string srcGlobals =
        "int gx = 100;\n"
        "int gy = 200;\n"
        "int main() {\n"
        "    gx += 50;\n"
        "    gy -= 25;\n"
        "    return (gx + gy) == 325 ? 1 : 0;\n"
        "}";
    assert(testNativeString(srcGlobals, 1, TargetArch::ARM64));
    assert(testNativeString(srcGlobals, 1, TargetArch::X86_64));
    std::cout << "  [PASS] Global Variables Mutation & Addressing\n";
}

void testNativeCastUnaryAnd64BitPointer() {
    std::cout << "[RUNNING] Native Cast & 64-Bit Pointer Tests...\n";
    // 1. Cast of address-of & dereference
    std::string srcCast = "int main() {\n"
                          "    int x = 300;\n"
                          "    char* p = (char*)&x;\n"
                          "    return *p;\n" // 300 % 256 = 44
                          "}";
    assert(testNativeString(srcCast, 44, TargetArch::ARM64));
    assert(testNativeString(srcCast, 44, TargetArch::X86_64));

    // 2. 64-bit non-null pointer truthiness in CondBranch and Not (0x100000000 = 4294967296)
    std::string srcPtrCond = "int main() {\n"
                             "    int* p = (int*)4294967296;\n"
                             "    if (p) {\n"
                             "        if (!p) return 10;\n"
                             "        return 42;\n"
                             "    }\n"
                             "    return 20;\n"
                             "}";
    assert(testNativeString(srcPtrCond, 42, TargetArch::ARM64));
    assert(testNativeString(srcPtrCond, 42, TargetArch::X86_64));
    std::cout << "  [PASS] Native Cast & 64-Bit Pointer Truthiness\n";
}

void testLinuxELFObjectEmission() {
    std::cout << "[RUNNING] Linux ELF Object Compilation Tests...\n";
    std::string src = "int add(int a, int b) { return a + b; }\n"
                      "int main() { return add(40, 2); }";
    std::string asmARM64, asmX86;
    assert(compileSourceToAssembly(src, "linux_test.c", TargetArch::ARM64, TargetOS::Linux, asmARM64));
    assert(compileSourceToAssembly(src, "linux_test.c", TargetArch::X86_64, TargetOS::Linux, asmX86));

    // Verify symbols are unprefixed in Linux ELF (not _main, not _add)
    assert(asmARM64.find(".globl main\n") != std::string::npos);
    assert(asmARM64.find(".globl add\n") != std::string::npos);
    assert(asmX86.find(".globl main\n") != std::string::npos);
    assert(asmX86.find(".globl add\n") != std::string::npos);

    // Verify local labels use .L in Linux ELF
    assert(asmARM64.find(".L") != std::string::npos);
    assert(asmX86.find(".L") != std::string::npos);

    // Assemble ELF .o object files using clang cross-assembler
    char tempAsm[] = "/tmp/c_compiler_elf_XXXXXX.s";
    int fd = mkstemps(tempAsm, 2);
    assert(fd != -1);
    write(fd, asmARM64.c_str(), asmARM64.size());
    close(fd);
    std::string objARM64 = std::string(tempAsm) + ".o";
    int res = std::system(("clang --target=aarch64-linux-gnu -c " + std::string(tempAsm) + " -o " + objARM64).c_str());
    unlink(tempAsm);
    assert(res == 0);
    unlink(objARM64.c_str());

    fd = mkstemps(tempAsm, 2);
    assert(fd != -1);
    write(fd, asmX86.c_str(), asmX86.size());
    close(fd);
    std::string objX86 = std::string(tempAsm) + ".o";
    res = std::system(("clang --target=x86_64-linux-gnu -c " + std::string(tempAsm) + " -o " + objX86).c_str());
    unlink(tempAsm);
    assert(res == 0);
    unlink(objX86.c_str());

    std::cout << "  [PASS] Linux ELF Object Compilation for ARM64 and x86_64\n";
}

void runCodeGenTests() {
    testNativeSamplesARM64();
    testNativeBuiltinsARM64();
    testNativeSamplesX86_64();
    testNativeBuiltinsX86_64();
    testNativeEdgeCases();
    testNativeCastUnaryAnd64BitPointer();
    testLinuxELFObjectEmission();
}
