# C Compiler in C++

A modular, clean, and extensible C compiler written in modern C++ (C++20), implementing all phases of the compiler pipeline:
1. **Lexer / Tokenizer**
2. **Parser (Recursive Descent & Pratt-style Precedence Climbing)**
3. **Semantic Analyzer & Scoped Symbol Table**
4. **Intermediate Representation (3-Address Code / CFG-based IR)**
5. **IR Pretty-Printer & IR Virtual Machine Interpreter**

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
                    │ Semantic Check  │  ──▶ Scoped Symbol Tables, Types, LValue checks
                    └─────────────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │  IR Generator   │  ──▶ 3-Address Code / BasicBlock CFG
                    └─────────────────┘
                             │
               ┌─────────────┴─────────────┐
               ▼                           ▼
      ┌─────────────────┐         ┌─────────────────┐
      │   IR Printer    │         │ IR Interpreter  │
      │ (LLVM-like text)│         │ (VM Execution)  │
      └─────────────────┘         └─────────────────┘
```

---
---

## Detailed Compiler Pipeline Stages

### Stage 1: Lexical Analysis (Lexer / Tokenizer)
- **Source Files**: [`include/Lexer.h`](include/Lexer.h), [`src/Lexer.cpp`](src/Lexer.cpp), [`include/Token.h`](include/Token.h), [`src/Token.cpp`](src/Token.cpp)
- **What It Does**: Reads raw ASCII/UTF-8 source text and transforms it into a clean stream of discrete semantic `Token` structures, discarding irrelevant whitespace and comments while preserving line and column numbers.
- **Key Implementation Details**:
  - **Location Tracking**: Every token retains `(line, column)` information for precise compiler error diagnostics.
  - **Radix Support**: Scans decimal integers, octal numbers (e.g., `077`, with validation rejecting invalid digits like `8` and `9`), and hexadecimal numbers (e.g., `0xFF`, `0x1a`).
  - **Literals & Escape Sequences**: Scans characters (`'a'`, `'\n'`, `'\x2A'`) and string literals (`"hello\n"`) with full escape code processing (`\0`, `\n`, `\t`, `\r`, `\\`, `\'`, `\"`, `\xHH`).
  - **Comments**: Strips both C++ single-line comments (`// ...`) and C multi-line block comments (`/* ... */`), reporting an error if a block comment remains unclosed at EOF.
  - **Keywords & Operators**: Categorizes tokens into keywords (`int`, `char`, `void`, `return`, `if`, `else`, `while`, `do`, `for`, `break`, `continue`, `struct`, `sizeof`), arithmetic operators, bitwise operators, relational operators, and compound assignment operators (`+=`, `-=`, `*=`, `/=`).
- **Inspection Flag**:
  ```bash
  ./bin/c_compiler --lex <source_file.c>
  ```

---

### Stage 2: Syntax Analysis (Parser & AST)
- **Source Files**: [`include/Parser.h`](include/Parser.h), [`src/Parser.cpp`](src/Parser.cpp), [`include/AST.h`](include/AST.h), [`src/AST.cpp`](src/AST.cpp)
- **What It Does**: Validates that the token sequence obeys the syntactic rules of the C grammar and builds a strongly-typed Abstract Syntax Tree (AST) representing the program structure.
- **Key Implementation Details**:
  - **Recursive Descent with Precedence Climbing**: Uses recursive descent for declarations and control flow statements, combined with Pratt-style precedence climbing for binary and ternary expressions, guaranteeing correct operator precedence and right-to-left associativity for assignments and conditional ternary expressions (`?:`).
  - **Multi-Declarator & Pointer Binding**: Correctly binds pointer asterisks to individual declarators rather than the base type (e.g., in `int *a, b;`, `a` is parsed as `int*` and `b` as `int`).
  - **Control Flow**: Parses `if`/`else` branches (resolving the classic dangling-else ambiguity by attaching `else` to the nearest `if`), `while`, `do-while`, `for`, `break`, `continue`, and `return`.
  - **Expressions**: Supports unary prefix (`++`, `--`, `+`, `-`, `!`, `~`, `*`, `&`), postfix (`++`, `--`), array indexing (`arr[i]`), struct member access (`.` and `->`), function calls with argument lists, and explicit type casts `(type)expr`.
  - **Error Recovery**: Implemented synchronization loops that discard tokens until a statement boundary (such as `;`) to avoid cascading hang bugs and provide multiple diagnostics.
- **Inspection Flag**:
  ```bash
  ./bin/c_compiler --ast <source_file.c>
  ```

---

### Stage 3: Semantic Analysis & Symbol Resolution
- **Source Files**: [`include/SemanticAnalyzer.h`](include/SemanticAnalyzer.h), [`src/SemanticAnalyzer.cpp`](src/SemanticAnalyzer.cpp), [`include/SymbolTable.h`](include/SymbolTable.h), [`src/SymbolTable.cpp`](src/SymbolTable.cpp), [`include/Type.h`](include/Type.h), [`src/Type.cpp`](src/Type.cpp), [`include/Diagnostic.h`](include/Diagnostic.h), [`src/Diagnostic.cpp`](src/Diagnostic.cpp)
- **What It Does**: Traverses the AST to enforce C language semantics, perform type checking and type inference, resolve identifiers in lexical scopes, verify struct layouts, and calculate byte offsets.
- **Key Implementation Details**:
  - **Lexical Scoping & Shadowing**: Manages hierarchical `Scope` symbol tables. Inner variables can shadow outer ones. Functions and variables reside in distinct namespaces from struct tags (`struct Tag`).
  - **Redefinition Prevention**: Detects and rejects duplicate definitions of variables or functions in the same scope while permitting valid forward declarations/prototypes.
  - **Type System**: Represents primitive types (`int`, `char`, `void`), pointer types (`T*`), arrays (`T[N]`), structs with member field layouts, and function signatures.
  - **Array Decay & Pointer Arithmetic**: Automatically decays array types to pointer types in expressions. Scales pointer increments and decrements by `sizeof(*ptr)`.
  - **LValue Validation**: Enforces that assignments, increment/decrement, and the address-of operator (`&`) only operate on modifiable lvalues.
  - **Struct Layout Calculation**: Resolves recursive member definitions, computes alignment padding according to 64-bit alignment rules, and records byte offsets for field lookups.
  - **Compile-Time Constant Folding**: Evaluates constant expressions for global variable initializers and array dimension declarations.
  - **Contextual Loop Validation**: Verifies that `break` and `continue` statements only appear inside loop constructs.
  - **Diagnostic Output**: Formats compiler error messages with source filenames, line numbers, columns, and an ASCII caret (`^`) pointing directly to the offending token.
- **Inspection Flag**:
  ```bash
  ./bin/c_compiler --check <source_file.c>
  ```

---

### Stage 4: Intermediate Representation (IR Generation)
- **Source Files**: [`include/IR.h`](include/IR.h), [`src/IR.cpp`](src/IR.cpp), [`include/IRGenerator.h`](include/IRGenerator.h), [`src/IRGenerator.cpp`](src/IRGenerator.cpp), [`include/IRPrinter.h`](include/IRPrinter.h), [`src/IRPrinter.cpp`](src/IRPrinter.cpp)
- **What It Does**: Lowers the semantically verified AST into a machine-independent Three-Address Code (TAC) representation structured into basic blocks within a Control Flow Graph (CFG).
- **Key Implementation Details**:
  - **Virtual Register Allocation**: Generates infinite virtual registers (`%t0`, `%t1`, `%t2`, ...).
  - **Memory Model**: Employs an explicit memory model:
    - `alloca`: Allocates stack space for local variables.
    - `load`: Loads values from memory pointers into virtual registers.
    - `store`: Writes virtual register values into target memory addresses.
    - `gep` (GetElementPtr): Computes element addresses for array indexing and struct member accesses.
  - **Control Flow Lowering**: Translates high-level constructs (`if`, `while`, `do-while`, `for`, `&&`, `||`) into basic blocks interconnected by unconditional jumps (`br`) and conditional jumps (`cond_br`).
  - **Short-Circuit Logic**: Lowers logical AND (`&&`) and OR (`||`) expressions into branching blocks that only evaluate the right-hand operand if required.
  - **Aggregate Struct Operations**: Lowers struct-by-value assignments and initializations into byte-by-byte memory copy loops.
  - **Global Initializers**: Emits data segment definitions for global variables and string literals.
- **Inspection Flag**:
  ```bash
  ./bin/c_compiler --ir <source_file.c>
  ```

---

### Stage 5: IR Execution Virtual Machine (Interpreter)
- **Source Files**: [`include/IRInterpreter.h`](include/IRInterpreter.h), [`src/IRInterpreter.cpp`](src/IRInterpreter.cpp)
- **What It Does**: Executes the generated IR module directly in an internal virtual machine without requiring an external assembler or native linker.
- **Key Implementation Details**:
  - **Virtual Memory Space**: Emulates a byte-addressable linear address space separating stack frames, heap allocations, and global data.
  - **Stack Frames**: Manages activation records for function calls, local stack variable allocations, parameter passing, and return value handling.
  - **Recursion Support**: Fully handles deeply recursive functions and mutual recursion.
  - **Built-in Runtime Environment**: Provides native built-in functions:
    - `print_int(int n)`: Prints an integer to standard output followed by a newline.
    - `print_char(char c)`: Prints a single character.
    - `print_str(char *s)`: Prints a null-terminated string.
    - `putchar(int c)`: Standard C `putchar`.
    - `puts(char *s)`: Standard C `puts` with newline.
    - `exit(int code)`: Terminates VM execution with a specific status code.
- **Inspection Flag**:
  ```bash
  ./bin/c_compiler --run <source_file.c>
  ```

---

## Build Instructions

Requirements:
- C++20 compiler (`clang++` or `g++`)
- GNU Make

```bash
# Build compiler binary and run the test suite
make all

# Build only the compiler executable (bin/c_compiler)
make compiler

# Run all unit tests and sample integration tests
make test

# Clean build artifacts
make clean
```

---

## Quick Start & Example Commands

Run the compiler pipeline on any sample C program:

```bash
# Build compiler and run test suite
make all

# Tokenize source code
./bin/c_compiler --lex tests/c_samples/01_arithmetic.c

# Dump Abstract Syntax Tree (AST)
./bin/c_compiler --ast tests/c_samples/01_arithmetic.c

# Run semantic checks only
./bin/c_compiler --check tests/c_samples/01_arithmetic.c

# Emit Intermediate Representation (IR)
./bin/c_compiler --ir tests/c_samples/01_arithmetic.c

# Run source through the built-in IR VM
./bin/c_compiler --run tests/c_samples/01_arithmetic.c
```

---

## CLI Usage

The compiler driver `bin/c_compiler` supports various flags:

```bash
# Tokenize and display token stream
./bin/c_compiler --lex file.c

# Parse and display the Abstract Syntax Tree (AST)
./bin/c_compiler --ast file.c

# Run semantic analysis and type checking only
./bin/c_compiler --check file.c

# Generate and print Intermediate Representation (IR)
./bin/c_compiler --ir file.c

# Compile to IR and execute in the IR Virtual Machine
./bin/c_compiler --run file.c

# Read source code from standard input
echo "int main() { return 42; }" | ./bin/c_compiler --run -
```

---

## How to Add and Run Custom C Files

You can compile, inspect, and execute any custom C file with `bin/c_compiler`.

### Step 1: Create a C Source File

Create a `.c` file anywhere on your filesystem (or in `tests/c_samples/`). For example, `my_program.c`:

```c
// my_program.c
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    int n = 5;
    int fact = factorial(n);
    print_str("Factorial of ");
    print_int(n);
    print_str(" is: ");
    print_int(fact);
    return fact;
}
```

### Step 2: Built-in Runtime Functions

The compiler's built-in runtime environment supports standard helper functions out of the box (no `#include` header files needed):

| Function | Signature | Description |
|---|---|---|
| `print_int` | `void print_int(int n)` | Prints an integer to stdout followed by a newline |
| `print_char` | `void print_char(char c)` | Prints a single character to stdout |
| `print_str` | `void print_str(char *s)` | Prints a null-terminated string without a newline |
| `putchar` | `int putchar(int c)` | Standard C character output |
| `puts` | `int puts(char *s)` | Prints string followed by a newline |
| `exit` | `void exit(int code)` | Immediately terminates execution with status code |

### Step 3: Run Compiler Stages on Your File

You can inspect each individual phase or run the full program end-to-end:

```bash
# 1. View lexical tokens with source coordinates
./bin/c_compiler --lex my_program.c

# 2. View Abstract Syntax Tree (AST) structure
./bin/c_compiler --ast my_program.c

# 3. Perform type checking and semantic analysis
./bin/c_compiler --check my_program.c

# 4. View generated 3-Address Code (IR)
./bin/c_compiler --ir my_program.c

# 5. Execute your program in the IR Virtual Machine
./bin/c_compiler --run my_program.c
```

### Step 4: Quick Testing via Standard Input (`stdin`)

You can pass C source code directly from the command line without saving a file by using `-` as the filename:

```bash
echo "int main() { puts(\"Hello from C compiler!\"); return 0; }" | ./bin/c_compiler --run -
```

### Step 5: (Optional) Adding Permanent Test Cases

To register your custom file with the automated test suite:
1. Save your test file under `tests/c_samples/` (e.g. `tests/c_samples/09_mytest.c`).
2. Add a test case to `tests/test_runner.cpp`:
   ```cpp
   runSampleTest("tests/c_samples/09_mytest.c", expected_return_code);
   ```
3. Run `make test` to automatically build and verify your new sample along with all other tests.

---

## Test Suite

The test suite validates every layer of the compiler:
- **Lexer Tests (`tests/test_lexer.cpp`)**: Keywords, literals, operators, comments, escapes, error reporting.
- **Parser Tests (`tests/test_parser.cpp`)**: Precedence climbing, right-associativity, structs, control flow.
- **Semantic Tests (`tests/test_semantic.cpp`)**: Scope shadowing, duplicate declarations, undeclared variables, lvalue checks, struct member checks, loop control.
- **IR & Execution Tests (`tests/test_ir.cpp` & `tests/c_samples/`)**:
  - `01_arithmetic.c`: Arithmetic, bitwise, compound assignments, inc/dec.
  - `02_control_flow.c`: If/else, nested loops, break/continue, short-circuit logic.
  - `03_functions.c`: Prototypes, recursive factorial, fibonacci, multi-parameter calls.
  - `04_pointers.c`: Address-of, dereference, pointer arithmetic, double pointers.
  - `05_arrays.c`: Array indexing, array-to-pointer decay, passing arrays to functions.
  - `06_structs.c`: Struct layout, member access via `.` and `->`, struct pointers.
  - `07_sort.c`: Complete bubble sort algorithm on an array.
  - `08_errors.c`: Diagnostic verification on semantic errors.
