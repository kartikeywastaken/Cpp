# C Compiler in C++ (C++20)

A modular, clean, and extensible C compiler written in modern C++ (C++20). It implements all phases of a complete compiler pipeline:
1. **Lexer / Tokenizer** (Radix scanning, escape sequences, location tracking)
2. **Parser** (Recursive Descent & Pratt-style Precedence Climbing)
3. **Semantic Analyzer & Scoped Symbol Table** (Type checking, LValue checks, struct layout, constant folding)
4. **Intermediate Representation (IR)** (3-Address Code / CFG-based IR with virtual registers)
5. **IR Virtual Machine Interpreter** (In-memory execution of IR without external tools)
6. **Backend Native Code Generator** (Native assembly emission for ARM64 and x86-64, linking to executable binaries via system assembler/linker)

---

## Architecture Overview

```
                      Source Code (.c)
                             │
                             ▼
                    ┌─────────────────┐
                    │      Lexer      │  ──▶ Tokens with source locations
                    └─────────────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │     Parser      │  ──▶ Abstract Syntax Tree (AST)
                    └─────────────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ Semantic Check  │  ──▶ Scoped Symbol Tables, Types, LValues
                    └─────────────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │  IR Generator   │  ──▶ 3-Address Code / CFG Basic Blocks
                    └─────────────────┘
                             │
        ┌────────────────────┼────────────────────┐
        ▼                    ▼                    ▼
┌───────────────┐    ┌───────────────┐    ┌───────────────┐
│  IR Printer   │    │ IR Interpreter│    │ Native CodeGen│
│ (TAC listing) │    │ (VM execution)│    │(ARM64 / x86_64│
└───────────────┘    └───────────────┘    └───────┬───────┘
                                                  │
                                                  ▼
                                          ┌───────────────┐
                                          │System Assembler
                                          │   & Linker    │
                                          │  (clang/ld)   │
                                          └───────┬───────┘
                                                  │
                                                  ▼
                                          Native Binary
                                             (a.out)
```

---

## Detailed Compiler Pipeline Stages

### Stage 1: Lexical Analysis (Lexer / Tokenizer)
- **Source Files**: [`include/Lexer.h`](include/Lexer.h), [`src/Lexer.cpp`](src/Lexer.cpp), [`include/Token.h`](include/Token.h), [`src/Token.cpp`](src/Token.cpp)
- **What It Does**: Reads raw ASCII/UTF-8 source text and converts it into a stream of discrete semantic `Token` structures, discarding irrelevant whitespace and comments while preserving coordinate locations.
- **Key Features**:
  - **Location Tracking**: Every token records `(line, column)` coordinates for compiler diagnostics.
  - **Radix Support**: Decimal, octal (e.g., `077` with digit validation), and hexadecimal (`0xFF`, `0x1a`).
  - **Literals & Escape Codes**: Characters (`'a'`, `'\n'`, `'\x2A'`) and string literals (`"hello\n"`) with escape sequences (`\0`, `\n`, `\t`, `\r`, `\\`, `\'`, `\"`, `\xHH`).
  - **Comments**: Strips both `//` single-line and `/* ... */` multi-line comments, catching unclosed comments at EOF.
  - **Keywords & Operators**: C keywords (`int`, `char`, `void`, `return`, `if`, `else`, `while`, `do`, `for`, `break`, `continue`, `struct`, `sizeof`), arithmetic, relational, bitwise, and compound assignments (`+=`, `-=`, etc.).
- **Inspection Flag**:
  ```bash
  ./bin/c_compiler --lex <source.c>
  ```

---

### Stage 2: Syntax Analysis (Parser & AST)
- **Source Files**: [`include/Parser.h`](include/Parser.h), [`src/Parser.cpp`](src/Parser.cpp), [`include/AST.h`](include/AST.h), [`src/AST.cpp`](src/AST.cpp)
- **What It Does**: Validates grammar syntax and constructs a strongly-typed Abstract Syntax Tree (AST).
- **Key Features**:
  - **Recursive Descent with Precedence Climbing**: Combines recursive descent for declarations and control structures with Pratt-style precedence climbing for binary and ternary expressions (`?:`), guaranteeing correct precedence and right-associativity.
  - **Multi-Declarator & Pointer Binding**: Correctly parses `int *a, b;` where `a` is `int*` and `b` is `int`.
  - **Control Flow**: `if`/`else` (resolving dangling-else ambiguity to nearest `if`), `while`, `do-while`, `for`, `break`, `continue`, and `return`.
  - **Expressions**: Prefix and postfix increment/decrement, array subscripting (`arr[i]`), struct member access (`.` and `->`), function calls, and explicit type casts `(type)expr`.
  - **Error Synchronization**: Discards tokens to statement boundaries (`;`) on syntax errors to avoid cascading bugs.
- **Inspection Flag**:
  ```bash
  ./bin/c_compiler --ast <source.c>
  ```

---

### Stage 3: Semantic Analysis & Symbol Resolution
- **Source Files**: [`include/SemanticAnalyzer.h`](include/SemanticAnalyzer.h), [`src/SemanticAnalyzer.cpp`](src/SemanticAnalyzer.cpp), [`include/SymbolTable.h`](include/SymbolTable.h), [`src/SymbolTable.cpp`](src/SymbolTable.cpp), [`include/Type.h`](include/Type.h), [`src/Type.cpp`](src/Type.cpp), [`include/Diagnostic.h`](include/Diagnostic.h), [`src/Diagnostic.cpp`](src/Diagnostic.cpp)
- **What It Does**: Enforces C language rules, type checking, identifier resolution across lexical scopes, struct layout, and diagnostics.
- **Key Features**:
  - **Lexical Scoping & Shadowing**: Hierarchical `Scope` tables supporting variable shadowing. Struct tags (`struct Tag`) reside in a distinct namespace.
  - **Redefinition Validation**: Rejects duplicate variable or function definitions in the same scope while permitting function prototypes.
  - **Type Checking**: Primitives (`int`, `char`, `void`), pointers (`T*`), arrays (`T[N]`), structs with member field layouts, and function signatures.
  - **Array Decay & Pointer Arithmetic**: Automatically decays arrays to pointers in expressions and scales pointer offsets by `sizeof(*ptr)`.
  - **LValue Validation**: Enforces that assignment operators, increments/decrements, and address-of (`&`) only operate on valid, modifiable lvalues.
  - **Struct Layout Calculation**: Calculates struct field byte offsets and alignment padding according to 64-bit ABI alignment rules.
  - **Constant Folding**: Evaluates compile-time constant expressions for global variable initializers and array dimensions.
  - **Diagnostics**: Rich error reporting with filename, line, column, and ASCII caret (`^`) pointing to the offending token.
- **Inspection Flag**:
  ```bash
  ./bin/c_compiler --check <source.c>
  ```

---

### Stage 4: Intermediate Representation (IR Generation)
- **Source Files**: [`include/IR.h`](include/IR.h), [`src/IR.cpp`](src/IR.cpp), [`include/IRGenerator.h`](include/IRGenerator.h), [`src/IRGenerator.cpp`](src/IRGenerator.cpp), [`include/IRPrinter.h`](include/IRPrinter.h), [`src/IRPrinter.cpp`](src/IRPrinter.cpp)
- **What It Does**: Lowers the semantically verified AST into a machine-independent Three-Address Code (TAC) representation structured into basic blocks within a Control Flow Graph (CFG).
- **Key Features**:
  - **Virtual Register Allocation**: Employs an infinite virtual register model (`%t0`, `%t1`, ...).
  - **Explicit Memory Model**: `alloca` for stack allocation, `load` for memory reads, `store` for memory writes, and `gep` (GetElementPtr) for address offsets.
  - **CFG Lowering**: Translates `if`, `while`, `do-while`, and `for` loops into basic blocks connected by unconditional jumps (`br`) and conditional jumps (`cond_br`).
  - **Short-Circuit Logic**: Lowers logical `&&` and `||` into branching control blocks with early-exit paths.
  - **Struct Operations**: Struct assignments are lowered into byte-level copy sequences.
- **Inspection Flag**:
  ```bash
  ./bin/c_compiler --ir <source.c>
  ```

---

### Stage 5: IR Execution Virtual Machine (Interpreter)
- **Source Files**: [`include/IRInterpreter.h`](include/IRInterpreter.h), [`src/IRInterpreter.cpp`](src/IRInterpreter.cpp)
- **What It Does**: Executes the generated IR module directly in an internal virtual machine without requiring an assembler or linker.
- **Key Features**:
  - **Linear Memory Space**: Emulates a byte-addressable memory space separating stack frames, heap, and global data.
  - **Stack Frames**: Handles activation records, parameter passing, and return value passing.
  - **Built-in Runtime Functions**: Built-in implementations of `print_int`, `print_char`, `print_str`, `putchar`, `puts`, and `exit`.
- **Inspection Flag**:
  ```bash
  ./bin/c_compiler --run <source.c>
  ```

---

### Stage 6: Backend Native Code Generation & Compilation
- **Source Files**: [`include/CodeGen.h`](include/CodeGen.h), [`src/CodeGen.cpp`](src/CodeGen.cpp), [`include/CodeGenARM64.h`](include/CodeGenARM64.h), [`src/CodeGenARM64.cpp`](src/CodeGenARM64.cpp), [`include/CodeGenX86.h`](include/CodeGenX86.h), [`src/CodeGenX86.cpp`](src/CodeGenX86.cpp)
- **What It Does**: Translates machine-independent IR into native assembly (`.s`), and invokes the system assembler and linker to produce object files (`.o`) or standalone native executable binaries (`a.out`).
- **Supported Targets**:
  - **ARM64 (AArch64)**: Native target on Apple Silicon macOS (`arm64-apple-darwin`) and 64-bit ARM Linux. Implements AAPCS64 calling conventions (`x0-x7` parameter registers, 16-byte stack alignment, link register `x30`, Mach-O `_` symbol mangling and `L_` local labels).
  - **x86-64 (AMD64)**: Native target on x86-64 macOS and Linux. Implements System V AMD64 ABI (`rdi, rsi, rdx, rcx, r8, r9` parameter registers, 16-byte stack alignment before `call`, return in `rax`).
- **Key Features**:
  - **Stack Frame Layout**: Computes optimal fixed-spill activation records allocating dedicated slots for `alloca` memory, virtual registers, and outgoing call arguments beyond register capacity.
  - **Built-in Native Runtime**: Emits native assembly routines for builtins (`print_int`, `print_char`, `print_str`) when not overridden by user code, linking directly with standard libc (`_putchar`, `_puts`, `_exit`).
  - **Full Driver Workflow**: Supports compiling straight to executable binaries (`-o`), emitting assembly only (`-S`), or compiling to object files (`-c`).
- **Usage**:
  ```bash
  # Compile C source directly to a native executable binary
  ./bin/c_compiler source.c -o my_binary
  ./my_binary

  # Emit native assembly (.s)
  ./bin/c_compiler -S source.c -o source.s

  # Compile to object file (.o)
  ./bin/c_compiler -c source.c -o source.o
  ```

---

## Build Instructions

Requirements:
- C++20 compiler (`clang++` or `g++`)
- GNU Make
- System assembler/linker (`clang` or `gcc`)

```bash
# Build compiler executable (bin/c_compiler) and run test suite
make all

# Build only the compiler executable
make compiler

# Run all unit tests, IR tests, and native compilation & binary execution tests
make test

# Clean build artifacts
make clean
```

---

## CLI Usage & Options Reference

```bash
Usage: ./bin/c_compiler [options] <source_file>

Compilation Options:
  -o <file>        Place output into <file> (executable, object, or assembly)
  -S               Compile only; emit native assembly (.s)
  -c               Compile and assemble; produce object file (.o)
  --target <arch>  Target architecture: 'arm64' or 'x86_64' (default: host architecture)
  --os <system>    Target operating system: 'macos' or 'linux' (default: host OS)
  --help, -h       Display this help message

Pipeline Inspection Options:
  --lex            Tokenize input and print tokens with coordinates
  --ast            Parse input and print Abstract Syntax Tree (AST)
  --check          Run semantic analysis and type checking only
  --ir             Generate and print Intermediate Representation (IR)
  --run            Generate IR and execute using built-in IR interpreter
```

### Common Command Examples

1. **Compile C code to native executable and run it**:
   ```bash
   ./bin/c_compiler tests/c_samples/01_arithmetic.c -o arithmetic
   ./arithmetic
   echo "Exit code: $?"
   ```

2. **Generate native ARM64 assembly**:
   ```bash
   ./bin/c_compiler -S tests/c_samples/07_sort.c -o sort.s
   cat sort.s
   ```

3. **Cross-target compilation to x86_64**:
   ```bash
   ./bin/c_compiler --target x86_64 tests/c_samples/03_functions.c -o func_x86
   ./func_x86
   ```

4. **Cross-compile Linux ELF object files (.o) or assembly (.s)**:
   ```bash
   # Emit Linux ELF assembly
   ./bin/c_compiler -S --os linux tests/c_samples/07_sort.c -o sort_linux.s

   # Produce ELF relocatable object file (.o)
   ./bin/c_compiler -c --os linux tests/c_samples/07_sort.c -o sort_linux.o
   file sort_linux.o
   ```

5. **Execute C code in the internal IR VM without an assembler**:
   ```bash
   ./bin/c_compiler --run tests/c_samples/06_structs.c
   ```

6. **Inspect compiler intermediate stages**:
   ```bash
   # View token stream
   ./bin/c_compiler --lex tests/c_samples/02_control_flow.c

   # View AST hierarchy
   ./bin/c_compiler --ast tests/c_samples/02_control_flow.c

   # View 3-Address Code IR
   ./bin/c_compiler --ir tests/c_samples/02_control_flow.c
   ```

7. **Compile directly from standard input**:
   ```bash
   echo "int main() { puts(\"Hello from compiled C!\"); return 42; }" | ./bin/c_compiler - -o hello
   ./hello
   echo "Exit code: $?"
   ```

---

## Built-in Runtime Functions

The compiler's runtime environment provides built-in helper functions available in both native execution and the IR interpreter without requiring standard header includes:

| Function | Signature | Description |
|---|---|---|
| `print_int` | `void print_int(int n)` | Prints an integer to standard output followed by a newline |
| `print_char` | `void print_char(char c)` | Prints a single character to standard output |
| `print_str` | `void print_str(char *s)` | Prints a null-terminated string to standard output |
| `putchar` | `int putchar(int c)` | Standard C character output |
| `puts` | `int puts(char *s)` | Prints string followed by a newline |
| `exit` | `void exit(int code)` | Immediately terminates execution with status code |

---

## Comprehensive Test Suite

Run the complete test suite with `make test`:

```bash
make test
```

The test runner [`tests/test_runner.cpp`](tests/test_runner.cpp) executes across all layers:
- **Lexer Tests (`tests/test_lexer.cpp`)**: Token classifications, numeric radix parsing, string escapes, comment stripping, lexical errors.
- **Parser Tests (`tests/test_parser.cpp`)**: Precedence climbing, right-associativity, declarator pointer binding, control structures.
- **Semantic Tests (`tests/test_semantic.cpp`)**: Scope shadowing, duplicate symbols, undeclared variables, lvalue checks, struct member checks.
- **IR & VM Tests (`tests/test_ir.cpp`)**: TAC instruction evaluation, loops, recursion, struct copying, constant expressions.
- **Native CodeGen Tests (`tests/test_codegen.cpp`)**:
  - Native compilation and binary execution of sample programs on **ARM64** with process exit code validation.
  - Native compilation and binary execution of sample programs on **x86_64** with process exit code validation.
  - Native output capture tests verifying `print_int`, `print_char`, `print_str`, and `puts`.
  - Edge case validation: stack calling conventions with $>8$ arguments, safe division/modulo by zero, global variable mutation and memory addressing.
- **End-to-End Sample Suite (`tests/c_samples/`)**:
  - `01_arithmetic.c`: Arithmetic, bitwise, compound assignments, inc/dec (exit code = 88)
  - `02_control_flow.c`: If/else, while, nested loops, break/continue, short-circuit logic (exit code = 43)
  - `03_functions.c`: Recursive factorial, fibonacci, multi-argument functions (exit code = 140)
  - `04_pointers.c`: Address-of, dereference, pointer arithmetic, double pointers (exit code = 145)
  - `05_arrays.c`: Array indexing, array-to-pointer decay, array parameters (exit code = 155)
  - `06_structs.c`: Struct layout, member access via `.` and `->`, struct pointers (exit code = 107)
  - `07_sort.c`: In-place bubble sort algorithm on arrays (exit code = 91)
  - `08_errors.c`: Diagnostic verification on semantic errors

---

## Theory and Compiler Internals

For a comprehensive theoretical explanation of every stage in the compiler pipeline—covering formal language theory, DFA/NFA scanning, Pratt precedence climbing, attribute grammars, type systems, Three-Address Code, control flow graphs, register allocation, AAPCS64/System V AMD64 calling conventions, stack layout, and linkers—refer to:

👉 **[`THEORY.md`](THEORY.md)**
