# Compiler Theory & Architecture Guide

A comprehensive, in-depth guide to the theory, mathematical foundations, data structures, and computer architecture principles behind every stage of this C compiler.

---

## Table of Contents
1. [Introduction & The Compilation Model](#1-introduction--the-compilation-model)
2. [Phase 1: Lexical Analysis (Scanning / Tokenization)](#2-phase-1-lexical-analysis-scanning--tokenization)
3. [Phase 2: Syntax Analysis (Parsing & Abstract Syntax Trees)](#3-phase-2-syntax-analysis-parsing--abstract-syntax-trees)
4. [Phase 3: Semantic Analysis & Type Systems](#4-phase-3-semantic-analysis--type-systems)
5. [Phase 4: Intermediate Representation (IR) & Control Flow Graphs (CFG)](#5-phase-4-intermediate-representation-ir--control-flow-graphs-cfg)
6. [Phase 5: Execution Models — Interpretation vs Compilation](#6-phase-5-execution-models--interpretation-vs-compilation)
7. [Phase 6: Backend Code Generation & Computer Architecture](#7-phase-6-backend-code-generation--computer-architecture)
8. [End-to-End Pipeline Trace](#8-end-to-end-pipeline-trace)

---

## 1. Introduction & The Compilation Model

A compiler is a program that translates formal text written in a high-level source language (such as C) into an equivalent program in a low-level target language (such as ARM64 or x86-64 machine assembly), preserving the operational semantics of the original program.

### 1.1 The Chomsky Hierarchy of Languages
In 1956, Noam Chomsky formalized the classification of formal grammars into four nested tiers, each corresponding to a class of automata required to recognize it:

| Level | Grammar Category | Production Rules | Automaton Model | Compiler Application |
|---|---|---|---|---|
| **Type 3** | **Regular Languages** | $A \to aB$ or $A \to a$ | Finite State Automaton (DFA / NFA) | Lexical Analysis (Tokens) |
| **Type 2** | **Context-Free Languages** | $A \to \gamma$ where $\gamma \in (V \cup \Sigma)^*$ | Pushdown Automaton (PDA) | Syntax Analysis (Grammar / AST) |
| **Type 1** | **Context-Sensitive Languages** | $\alpha A \beta \to \alpha \gamma \beta$ | Linear Bounded Automaton | Semantic Analysis (Type Checking) |
| **Type 0** | **Recursively Enumerable** | $\alpha \to \beta$ (unrestricted) | Turing Machine | Full Computation (Turing Complete) |

Programming languages cannot be parsed using Regular expressions alone because constructs like nested parentheses, block scopes, and matched braces require a memory stack (Pushdown Automaton). Similarly, context-free grammars cannot enforce that a variable is declared before use or that assigned types are compatible; these rules require context-sensitive analysis.

### 1.2 Multi-Phase Compiler Architecture
A modern production compiler is partitioned into three logical sections:
```
Source Code ──▶ [ Frontend ] ──▶ Intermediate Representation (IR) ──▶ [ Middle-End ] ──▶ [ Backend ] ──▶ Machine Code
```

1. **Frontend**: Language-specific. Performs Lexical Analysis, Parsing, and Semantic Analysis to translate source text into an Abstract Syntax Tree (AST).
2. **Middle-End**: Target-independent and language-independent. Lowers the AST into Three-Address Code (TAC) and Control Flow Graphs (CFG), performing machine-independent optimizations (constant propagation, dead code elimination).
3. **Backend**: Architecture-specific. Performs instruction selection, register allocation, stack frame layout, and emits native assembly (ARM64 / x86-64).

**The $M \times N$ Engineering Advantage**: If a compiler supports $M$ source languages and $N$ target architectures, compiling directly from source to machine code requires $M \times N$ distinct compilers. By routing all source languages into a unified Intermediate Representation (IR), the problem is reduced to $M$ frontends and $N$ backends—a total of $M + N$ components.

---

## 2. Phase 1: Lexical Analysis (Scanning / Tokenization)

### 2.1 Theoretical Foundations
The lexer's purpose is to transform an unstructured stream of characters:
$$c_1, c_2, c_3, \dots, c_n \in \Sigma$$
into a structured sequence of discrete lexical tokens:
$$T_1, T_2, T_3, \dots, T_k$$

Each token is formally defined by a Regular Expression. A language $L$ is regular if and only if it is accepted by a **Deterministic Finite Automaton (DFA)**:
$$M = (Q, \Sigma, \delta, q_0, F)$$
where:
- $Q$ is a finite set of states.
- $\Sigma$ is the input alphabet (ASCII / UTF-8 characters).
- $\delta: Q \times \Sigma \to Q$ is the state transition function.
- $q_0 \in Q$ is the start state.
- $F \subseteq Q$ is the set of accepting states.

### 2.2 Conversion: NFA to DFA
1. **Thompson's Construction**: Translates regular expressions containing concatenation ($ab$), alternation ($a \mid b$), and Kleene star ($a^*$) into an $\varepsilon$-NFA (Non-deterministic Finite Automaton with empty transitions).
2. **Subset Construction (Powerset Construction)**: Converts the $\varepsilon$-NFA into an equivalent DFA where each DFA state corresponds to a set of NFA states reachable via $\varepsilon$-closures.
3. **Hopcroft's Algorithm**: Minimizes the DFA by partitioning states into equivalence classes, producing the minimal state machine with $O(n \log n)$ complexity.

### 2.3 Implementation Realities in Our Compiler
In [`src/Lexer.cpp`](src/Lexer.cpp), we implement an efficient hand-crafted scanner implementing the DFA:

```
           [0-9]           [0-9]
   ┌─────────────────┐   ┌───────┐
   │                 ▼   │       ▼
(START) ─────────▶ (INT_DECIMAL) ───▶ Accepted as IntLiteral
   │   \
   │    \── '0' ──▶ (OCTAL_CHECK) ─── 'x'/'X' ──▶ (HEX_SCAN)
   │
   ├───── [a-zA-Z_] ──▶ (IDENTIFIER) ───▶ Keyword Hash Table Lookup
   │
   ├───── '"' ────────▶ (STRING_SCAN) ───▶ Escape Code Conversion
   │
   └───── '/' ────────▶ ('/' or '*') ───▶ Comment Stripping Loop
```

#### Maximal Munch (Longest Match Rule)
When recognizing tokens, the lexer must resolve ambiguity. For example, when encountering `+++`, does this mean `+` followed by `++`, or `++` followed by `+`?
The **Maximal Munch** rule specifies that the lexer always matches the longest valid token starting at the current position. Thus:
- `++` is parsed as the increment operator `TokenKind::PlusPlus`, rather than two separate `+` tokens.
- `>>` is parsed as `TokenKind::GreaterGreater`, not two `>` comparisons.
- `int_variable` is parsed as an identifier, not the keyword `int` followed by `_variable`.

#### Multi-Radix Numeric Parsing
- **Decimal**: Scans digits `0-9`.
- **Octal**: Initiated by a leading `0`. Validates that all subsequent digits are strictly `0-7`. Rejects invalid digits like `8` or `9` with an error diagnostic.
- **Hexadecimal**: Initiated by `0x` or `0X`. Scans digits `0-9`, `a-f`, and `A-F`, computing:
  $$\text{val} = \text{val} \times 16 + \text{digit\_value}$$

#### Escape Sequence Processing
String and character literals resolve escape sequences in-place:
`\n` $\to$ `0x0A`, `\t` $\to$ `0x09`, `\r` $\to$ `0x0D`, `\0` $\to$ `0x00`, `\\` $\to$ `0x5C`, `\"` $\to$ `0x22`, and arbitrary hex escapes `\xHH`.

#### Source Coordinate Tracking
Every token records `SourceLocation(line, column)`. Whenever a newline `\n` is scanned, the line counter increments and column resets to 1. This metadata is propagated into the AST for compiler diagnostics.

---

## 3. Phase 2: Syntax Analysis (Parsing & Abstract Syntax Trees)

### 3.1 Context-Free Grammars (CFGs)
A Context-Free Grammar is formally defined as a 4-tuple:
$$G = (V, \Sigma, R, S)$$
- $V$: Non-terminal symbols (syntactic variables like `Expr`, `Stmt`).
- $\Sigma$: Terminal symbols (lexical tokens like `+`, `int`, `identifier`).
- $R$: Finite set of production rules of the form $A \to \alpha$, where $A \in V$ and $\alpha \in (V \cup \Sigma)^*$.
- $S \in V$: The start symbol (`Program`).

### 3.2 Top-Down Recursive Descent Parsing
Our parser in [`src/Parser.cpp`](src/Parser.cpp) utilizes **Top-Down Recursive Descent**. In this paradigm:
- Each non-terminal symbol in the grammar corresponds to a C++ member function (e.g., `parseStatement()`, `parseDeclaration()`).
- The execution call stack of the parser directly mirrors the derivation tree of the grammar.
- Lookahead $k=1$ (LL(1)) is used to decide which production rule to expand by inspecting `peekToken()`.

### 3.3 The Pratt Parsing & Precedence Climbing Algorithm
Standard recursive descent requires a separate function for every single operator precedence level (e.g., `parseAdd()`, `parseMul()`, `parseRelational()`), resulting in deep call stacks and cumbersome code.
To solve this, our compiler uses **Pratt Parsing** (Precedence Climbing) for all binary, unary, and ternary expressions.

Each binary operator is assigned an **infix binding power** (precedence level) and an **associativity**:

```
Precedence Table (Highest to Lowest):
12: Member Access (., ->), Subscript ([])
11: Prefix Unary (!, ~, -, +, *, &, ++, --), Cast ((type))
10: Multiplicative (*, /, %)
 9: Additive (+, -)
 8: Bitwise Shifts (<<, >>)
 7: Relational (<, <=, >, >=)
 6: Equality (==, !=)
 5: Bitwise AND (&)
 4: Bitwise XOR (^)
 3: Bitwise OR (|)
 2: Logical AND (&&)
 1: Logical OR (||)
 0: Conditional Ternary (?:) [Right-to-Left Associative]
-1: Assignment (=, +=, -=, *=, /=) [Right-to-Left Associative]
```

#### Precedence Climbing Algorithm
```cpp
ASTPtr parseExpression(int minPrecedence) {
    ASTPtr lhs = parseUnaryPrefix();

    while (true) {
        Token op = peekToken();
        int prec = getPrecedence(op.kind);
        if (prec < minPrecedence) break;

        consumeToken();
        // Right-associative operators (like = and ?:) use 'prec',
        // whereas left-associative operators use 'prec + 1'
        int nextMinPrec = isRightAssociative(op.kind) ? prec : prec + 1;
        ASTPtr rhs = parseExpression(nextMinPrec);

        lhs = std::make_shared<BinaryExpr>(op, lhs, rhs);
    }
    return lhs;
}
```

**Why this works**:
When parsing `a + b * c`:
1. `lhs = a`
2. Operator `+` has precedence 9. Because $9 \ge 0$, the parser consumes `+`.
3. It recursively calls `parseExpression(10)`.
4. Inside the recursive call, `lhs = b`. The next operator `*` has precedence 10. Because $10 \ge 10$, `*` is consumed and binds to `b` and `c`.
5. The recursive call returns `(b * c)`, which then becomes the right-hand side of `+`, producing the tree:
   $$\text{Add}(a, \text{Mul}(b, c))$$

### 3.4 Disambiguating the Dangling-Else Problem
Consider the ambiguous grammar sequence:
```c
if (cond1) if (cond2) stmt1; else stmt2;
```
Does `else stmt2` attach to `if (cond1)` or `if (cond2)`?
In our recursive descent parser, the dangling-else is deterministically resolved by binding the `else` to the innermost, immediately preceding `if`. Because `parseIfStmt()` immediately checks for `TokenKind::KwElse` following the `then` branch, it greedily absorbs the `else` before the outer `if` can resume.

### 3.5 Abstract Syntax Trees (AST)
While a **Concrete Parse Tree** retains every single semicolon, parenthesis, and keyword, an **Abstract Syntax Tree (AST)** retains only the semantic nodes required for execution:
- Expressions: `BinaryExpr`, `UnaryExpr`, `TernaryExpr`, `CallExpr`, `CastExpr`, `DeclRefExpr`, `LiteralExpr`
- Statements: `CompoundStmt`, `IfStmt`, `WhileStmt`, `DoWhileStmt`, `ForStmt`, `ReturnStmt`, `BreakStmt`, `ContinueStmt`
- Declarations: `VarDecl`, `FunctionDecl`, `StructDecl`, `Program`

---

## 4. Phase 3: Semantic Analysis & Type Systems

Semantic analysis enforces the context-sensitive rules of C that cannot be expressed in context-free grammars.

### 4.1 Scoped Symbol Tables
The symbol table in [`src/SymbolTable.cpp`](src/SymbolTable.cpp) is implemented as a **Scoped Cactus Stack** (hierarchical tree of symbol tables):

```
┌────────────────────────────────────────────────────────┐
│ Global Scope (Functions, Global Variables, Struct Tags)│
└────────────────────────────────────────────────────────┘
                           ▲
                           │ parent
┌────────────────────────────────────────────────────────┐
│ Function Scope (Parameters, Local Variables)           │
└────────────────────────────────────────────────────────┘
                           ▲
                           │ parent
┌────────────────────────────────────────────────────────┐
│ Block Scope (Loop counters, local if/else blocks)      │
└────────────────────────────────────────────────────────┘
```

- **Lookup**: Traverses from the innermost active scope up to the global scope.
- **Shadowing**: A declaration in an inner scope hides a declaration of the same name in an outer scope.
- **Duplicate Detection**: Attempting to declare two variables with the same identifier in the same scope raises a compile-time error.
- **Namespace Separation**: C maintains distinct namespaces for identifiers (variables/functions) and struct tags (`struct Vector`). Both can share the same name without collision.

### 4.2 Type System & Array Decay
The type system in [`src/Type.cpp`](src/Type.cpp) defines:
- **Primitive Types**: `int` (32-bit signed), `char` (8-bit signed), `void`.
- **Pointer Types**: `T*` (64-bit address). Pointer arithmetic scales automatically:
  $$\text{Address}(p + i) = \text{Address}(p) + i \times \text{sizeof}(T)$$
- **Array Types**: `T[N]` (contiguous sequence of $N$ elements).
  - **Array Decay**: Whenever an array expression appears in an rvalue context (such as passing to a function or indexing), the type automatically decays from `T[N]` into a pointer `T*` pointing to the first element.
- **Struct Types**: User-defined aggregates with named fields.

### 4.3 Struct Layout & Alignment
Modern 64-bit CPUs require data in memory to be naturally aligned to avoid performance penalties or hardware bus faults:
- `char` (1 byte): aligned to 1-byte boundary.
- `int` (4 bytes): aligned to 4-byte boundary ($addr \equiv 0 \pmod 4$).
- Pointer `T*` (8 bytes): aligned to 8-byte boundary ($addr \equiv 0 \pmod 8$).

Our struct layout algorithm in `Type::computeLayout()` calculates:
1. Member offsets:
   $$\text{offset}_{i} = \text{roundUp}(\text{currentOffset}, \text{align}(T_i))$$
   where $\text{roundUp}(x, a) = (x + a - 1) \ \& \ \sim(a - 1)$.
2. Struct alignment:
   $$\text{align}(\text{Struct}) = \max_{i}(\text{align}(T_i))$$
3. Total struct size:
   $$\text{size}(\text{Struct}) = \text{roundUp}(\text{currentOffset}, \text{align}(\text{Struct}))$$

This guarantees that in arrays of structs, every element maintains proper alignment.

### 4.4 LValues vs RValues
- **LValue (Locator Value)**: Refers to an identifiable object occupying a specific memory location (e.g., variables, `*ptr`, `arr[i]`, `s.field`). LValues can appear on the left-hand side of assignments and have their address taken (`&`).
- **RValue (Read Value)**: A temporary value or literal that does not have an enduring memory address (e.g., `42`, `a + b`, function return values). Assigning to an RValue (`10 = x;`) or taking its address (`&42;`) is rejected by the semantic analyzer.

---

## 5. Phase 4: Intermediate Representation (IR) & Control Flow Graphs (CFG)

### 5.1 Three-Address Code (TAC)
Three-Address Code is a linearized representation where each instruction has at most one operator and at most three operands:
$$\text{dest} \leftarrow \text{src}_1 \ \mathbf{op} \ \text{src}_2$$

For example, the complex expression `x = a + b * c` is decomposed into:
```
%t0 = mul b, c
%t1 = add a, %t0
store %t1 -> x
```

### 5.2 Control Flow Graphs (CFGs)
A program's control flow is organized into a directed graph:
$$G = (V, E)$$
where:
- **Vertices ($V$)** are **Basic Blocks**: sequences of straight-line instructions with:
  1. Exactly **one entry point**: execution enters only at the first instruction.
  2. Exactly **one exit point**: execution leaves only at the terminator instruction (`br`, `cond_br`, `ret`).
- **Edges ($E$)** represent jumps between basic blocks.

```
                  ┌──────────────┐
                  │ while.cond   │◀────┐
                  │ %t0 = i < 10 │     │
                  │ condbr %t0   │     │
                  └──────┬───────┘     │
                    true │ false       │
             ┌───────────┘ └──────┐    │
             ▼                    ▼    │
      ┌──────────────┐     ┌───────────┴──┐
      │ while.body   │     │ while.end    │
      │ sum += i     │     │ return sum   │
      │ br while.cond│─────┘              │
      └──────────────┘                    │
```

### 5.3 The Explicit Memory Model: Alloca / Load / Store / GEP
To cleanly support pointer aliasing, function parameters, and local variables without prematurely committing to hardware registers, our IR uses an explicit memory model (similar to LLVM IR):

1. **`alloca(size)`**: Reserves a slot on the stack frame and returns its pointer.
2. **`load(ptr, type)`**: Reads a value from memory address `ptr` into a virtual register.
3. **`store(val, ptr, type)`**: Writes the value `val` into memory address `ptr`.
4. **`gep(ptr, offset)`** (*GetElementPtr*): Calculates an address offset:
   $$\text{dest} = \text{ptr} + \text{offset}$$
   Used for struct member accesses and array indexing.

---

## 6. Phase 5: Execution Models — Interpretation vs Compilation

| Dimension | IR Virtual Machine Interpreter | Native Backend Compilation |
|---|---|---|
| **Mechanism** | Software emulation loop traversing CFG in memory | Lowers IR to hardware machine instructions |
| **Dependencies** | Self-contained (no external tools required) | Requires system assembler & linker (`clang`/`ld`) |
| **Memory** | Emulated byte array (`std::vector<uint8_t>`) | Physical hardware RAM & OS-managed virtual memory |
| **Call Stack** | Virtual `StackFrame` map storing register values | Physical hardware CPU stack (`sp`, `x29`, `rbp`) |
| **Execution Speed**| Interpreted overhead per instruction | Bare-metal execution at CPU clock speed |

Our compiler includes **both models**:
- `--run` executes the IR directly in the built-in virtual machine for rapid debugging, profiling, and testing.
- Default execution compiles to native ARM64 or x86-64 assembly and links an executable binary.

---

## 7. Phase 6: Backend Code Generation & Computer Architecture

### 7.1 Processor Architectures: RISC vs CISC

| Architectural Feature | ARM64 (AArch64) | x86-64 (AMD64) |
|---|---|---|
| **Design Philosophy** | **RISC** (Reduced Instruction Set Computer) | **CISC** (Complex Instruction Set Computer) |
| **Instruction Length** | Uniform 32-bit (4 bytes) | Variable length (1 to 15 bytes) |
| **Memory Access** | **Load/Store only**: arithmetic operates only on registers | Arithmetic can directly reference memory (`addl %eax, (%rdi)`) |
| **General Registers** | 31 64-bit registers (`x0` - `x30`) | 16 64-bit registers (`rax`, `rbx`, ..., `r15`) |
| **Condition Codes** | Explicit conditional set (`cset w0, eq`) | Status flags (`eflags`) and `setcc` instructions |

---

### 7.2 Application Binary Interfaces (ABIs) & Calling Conventions

An Application Binary Interface defines the low-level contract between functions, system libraries, and the operating system.

#### AAPCS64 (ARM 64-bit Procedure Call Standard)
- **Function Arguments**: First 8 integer/pointer arguments passed in registers `x0` through `x7`. Additional arguments are passed on the stack.
- **Return Values**: Returned in `x0` (64-bit / pointer) or `w0` (32-bit).
- **Special Registers**:
  - `sp`: Stack Pointer (must remain 16-byte aligned at all times).
  - `x29`: Frame Pointer (`FP`).
  - `x30`: Link Register (`LR`), holds the return address for `ret`.
  - `x16`, `x17`: Intra-procedure call scratch registers (`IP0`, `IP1`).
- **Callee-Saved Registers**: `x19` - `x28` (must be preserved across calls if modified).
- **Caller-Saved Registers**: `x0` - `x15` (can be overwritten by callees).

#### System V AMD64 ABI (x86-64 Linux & macOS)
- **Function Arguments**: First 6 integer/pointer arguments passed in `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`. Additional arguments on stack.
- **Return Values**: Returned in `%rax` (64-bit) or `%eax` (32-bit).
- **Stack Alignment**: `%rsp + 8` must be a multiple of 16 immediately before a `call` instruction executes.

---

### 7.3 Stack Frame Layout & Activation Records

Our backend calculates a deterministic, collision-free stack frame layout for each function:

```
                      ARM64 Stack Frame Layout
 High Memory
   │  ┌──────────────────────────────────────────────┐
   │  │ Caller Arguments (>8 arguments overflow)    │  [x29 + 16, ...]
   │  ├──────────────────────────────────────────────┤
   │  │ Saved Link Register (x30 / LR)               │  [x29 + 8]
   │  ├──────────────────────────────────────────────┤
   │  │ Saved Frame Pointer (x29 / FP)               │  [x29] ◀── x29 points here
   │  ├──────────────────────────────────────────────┤
   │  │ Local Alloca Memory Slots                    │  [sp + outgoingArgs + totalRegs + offset]
   │  ├──────────────────────────────────────────────┤
   │  │ Virtual Register Spill Slots (%t0, %t1, ...) │  [sp + outgoingArgs + 8*r]
   │  ├──────────────────────────────────────────────┤
   │  │ Outgoing Call Arguments (>8 overflow)        │  [sp, sp + 8, ...]
 Low Memory └──────────────────────────────────────────────┘ ◀── sp (16-byte aligned)
```

#### Prologue & Epilogue Sequences

**ARM64 Prologue**:
```assembly
stp x29, x30, [sp, #-16]!     ; Push FP and LR, decrement SP by 16
mov x29, sp                   ; Set FP to top of stack
sub sp, sp, #frameSize        ; Allocate complete frame (16-byte aligned)
```

**ARM64 Epilogue**:
```assembly
mov sp, x29                   ; Collapse frame back to saved FP
ldp x29, x30, [sp], #16       ; Restore FP and LR, pop 16 bytes
ret                           ; Return to address in x30 (LR)
```

---

### 7.4 Object File Formats & Symbol Relocations

Compilers do not emit raw machine bits directly into memory; they emit assembly that is assembled into relocatable object files (`.o`) and linked into executables.

#### Mach-O (macOS) vs ELF (Linux)

1. **Symbol Mangling**:
   - On macOS (Mach-O), all C global symbols are prefixed with an underscore `_` (e.g., `_main`, `_printf`, `_putchar`).
   - On Linux (ELF), symbols are unprefixed (e.g., `main`, `printf`).

2. **Assembler-Local Labels**:
   - On Mach-O, local branch labels must start with `L_` or `l_` (e.g., `L_main_if_then`). Labels starting with `.L` are treated as external undefined symbols.
   - On ELF, local branch labels start with `.L` (e.g., `.Lmain_if_then`).

3. **PC-Relative Addressing**:
   - 64-bit architectures avoid absolute 64-bit addresses in code sections to enable **Position Independent Executables (PIE)**.
   - In ARM64 Mach-O:
     ```assembly
     adrp x8, _globalVar@PAGE         ; Load 4KB page address into x8
     add  x8, x8, _globalVar@PAGEOFF  ; Add 12-bit page offset
     ```
   - In ARM64 Linux ELF:
     ```assembly
     adrp x8, globalVar               ; Load page
     add  x8, x8, :lo12:globalVar     ; Add 12-bit low offset
     ```

---

## 8. End-to-End Pipeline Trace

Let us trace a sample C program through all 6 phases:

```c
int square(int x) {
    return x * x;
}
```

### Phase 1: Lexical Analysis
```
[KwInt, "int"], [Identifier, "square"], [OpenParen, "("], [KwInt, "int"],
[Identifier, "x"], [CloseParen, ")"], [OpenBrace, "{" ],
[KwReturn, "return"], [Identifier, "x"], [Star, "*"], [Identifier, "x"],
[Semicolon, ";"], [CloseBrace, "}"], [Eof, ""]
```

### Phase 2: Syntax Analysis (AST)
```
FunctionDecl: square (returnType: int)
  Params:
    VarDecl: x (type: int)
  Body: CompoundStmt
    ReturnStmt
      BinaryExpr: Mul (*)
        DeclRefExpr: x
        DeclRefExpr: x
```

### Phase 3: Semantic Analysis
- Scope verifies `x` is declared as type `int`.
- Expression `x * x` evaluates `int * int -> int`.
- Return type `int` matches function declaration `int`.

### Phase 4: Intermediate Representation (IR)
```llvm
function square(%t0: i32) -> i32 {
bb_entry_0:
    %t1 = alloca(4)
    store %t0 -> %t1
    %t2 = load %t1
    %t3 = load %t1
    %t4 = mul %t2, %t3
    ret %t4
}
```

### Phase 5: IR VM Interpretation
- Virtual frame maps `%t0 = 5`.
- `alloca` allocates 4 bytes at address `0x1000`.
- `store 5 -> 0x1000`.
- `load` loads `5` into `%t2` and `%t3`.
- `mul` computes `25`.
- Returns `25`.

### Phase 6: Native ARM64 Code Generation
```assembly
.section __TEXT,__text,regular,pure_instructions
.globl _square
.p2align 2
_square:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #64
    str x0, [sp, #0]             ; Store incoming parameter x0 into %t0 slot
L_square_entry_0:
    add x8, sp, #48              ; Compute alloca address for %t1
    str x8, [sp, #8]
    ldr x8, [sp, #8]             ; Store %t0 into %t1
    ldr w9, [sp, #0]
    str w9, [x8]
    ldr x8, [sp, #8]             ; Load %t1 into %t2
    ldr w9, [x8]
    sxtw x9, w9
    str x9, [sp, #16]
    ldr x8, [sp, #8]             ; Load %t1 into %t3
    ldr w9, [x8]
    sxtw x9, w9
    str x9, [sp, #24]
    ldr w8, [sp, #16]            ; Multiply %t2 * %t3
    ldr w9, [sp, #24]
    mul w10, w8, w9
    sxtw x10, w10
    str x10, [sp, #32]
    ldr w0, [sp, #32]            ; Move result into return register w0
    b L_square_epilogue
L_square_epilogue:
    mov sp, x29
    ldp x29, x30, [sp], #16      ; Restore frame
    ret                          ; Return to caller
```

The resulting assembly is assembled and linked by the system driver:
```bash
./bin/c_compiler square.c -o square
./square
```
The CPU executes the machine instructions at native hardware speeds.
