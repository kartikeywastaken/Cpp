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

## Features

### 1. Lexer / Tokenizer (`include/Lexer.h`, `src/Lexer.cpp`)
- Full tokenization with precise line and column location tracking.
- Keywords: `int`, `char`, `void`, `return`, `if`, `else`, `while`, `for`, `break`, `continue`, `struct`, `sizeof`.
- Identifiers and numeric literals (decimal, octal `077`, hexadecimal `0x1F`).
- Character and string literals with escape sequences (`\n`, `\t`, `\r`, `\0`, `\\`, `\'`, `\"`, `\xHH`).
- Operators: arithmetic, bitwise, logical, relational, assignment, increment/decrement, arrow `->`, dot `.`.
- Single-line (`//`) and multi-line (`/* ... */`) comments with error detection for unclosed comments.

### 2. Type System & AST (`include/Type.h`, `include/AST.h`)
- Types: primitives (`int`, `char`, `void`), pointers (`T*`), arrays (`T[N]`), structs with memory layout padding, function types.
- AST hierarchy supporting declarations, statements, and expressions.
- AST pretty-printer (`--ast`).

### 3. Parser (`include/Parser.h`, `src/Parser.cpp`)
- Recursive descent parser with operator precedence climbing.
- Declarations: global variables, local variables, function prototypes, function definitions, structs.
- Control flow: `if`/`else` (resolving dangling else), `while`, `for`, `break`, `continue`, `return`.
- Expressions: binary ops with standard C precedence, unary prefix/postfix ops, casts `(type)expr`, `sizeof(type)` and `sizeof(expr)`, struct member accesses (`.` and `->`), array subscripting `a[i]`, and function calls.

### 4. Semantic Analysis (`include/SemanticAnalyzer.h`, `src/SemanticAnalyzer.cpp`)
- Lexically scoped symbol tables supporting nested scopes and shadowing.
- Strict type checking, type compatibility, and integer promotions.
- Array-to-pointer decay in expression evaluations.
- Pointer arithmetic rules (`ptr + int`, `int + ptr`, `ptr - int`, `ptr - ptr`).
- LValue validation for assignments, address-of (`&`), and increment/decrement.
- Struct member verification with field offset calculations.
- Control flow validation (ensuring `break` and `continue` only reside within loop bodies).
- Diagnostic engine highlighting error lines with column pointers (`^`).

### 5. Intermediate Representation (`include/IR.h`, `include/IRGenerator.h`)
- Three-Address Code (TAC) organized in a Control Flow Graph (CFG) of Basic Blocks.
- Quadruples with explicit virtual registers and memory operands.
- Memory model with `alloca`, `load`, `store`, and `gep` (GetElementPtr).
- Short-circuit boolean evaluation (`&&`, `||`) generated as branch blocks.
- Loop lowering with dedicated condition, body, step, and exit blocks.

### 6. IR Interpreter (`include/IRInterpreter.h`, `src/IRInterpreter.cpp`)
- Built-in Virtual Machine executing the IR module directly.
- Handles call frames, stack memory slots, global memory, string tables, and pointer dereferencing.
- Standard built-in runtime functions: `print_int`, `print_char`, `print_str`, `putchar`, `puts`, `exit`.

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
