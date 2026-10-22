#include "CodeGenX86.h"
#include <iomanip>
#include <sstream>
#include <algorithm>

X86_64CodeGenerator::X86_64CodeGenerator(TargetOS os)
    : targetOS(os) {}

std::string X86_64CodeGenerator::mangledName(const std::string& name) const {
    if (targetOS == TargetOS::macOS) {
        return "_" + name;
    }
    return name;
}

std::string X86_64CodeGenerator::stringLabel(const std::string& label) const {
    std::string clean = label;
    std::replace(clean.begin(), clean.end(), '.', '_');
    if (targetOS == TargetOS::macOS) {
        return "l_" + clean;
    }
    return ".L" + clean;
}

std::string X86_64CodeGenerator::localLabel(const std::string& name) const {
    if (targetOS == TargetOS::macOS) {
        return "L_" + name;
    }
    return ".L" + name;
}

std::string X86_64CodeGenerator::escapeString(const std::string& str) const {
    std::string res;
    for (unsigned char c : str) {
        if (c == '\n') res += "\\n";
        else if (c == '\t') res += "\\t";
        else if (c == '\r') res += "\\r";
        else if (c == '\\') res += "\\\\";
        else if (c == '"') res += "\\\"";
        else if (c >= 32 && c <= 126) res += static_cast<char>(c);
        else {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\%03o", c);
            res += buf;
        }
    }
    return res;
}

X86_64CodeGenerator::FunctionFrame X86_64CodeGenerator::computeStackLayout(const IRFunction& fn) {
    FunctionFrame frame;
    size_t maxCallArgs = 0;

    for (const auto& param : fn.getParamNames()) {
        (void)param;
        frame.maxRegNum++;
    }

    for (const auto& block : fn.getBlocks()) {
        for (const auto& inst : block->getInstructions()) {
            if (inst.dest.isRegister() && inst.dest.regNum > frame.maxRegNum) {
                frame.maxRegNum = inst.dest.regNum;
            }
            if (inst.src1.isRegister() && inst.src1.regNum > frame.maxRegNum) {
                frame.maxRegNum = inst.src1.regNum;
            }
            if (inst.src2.isRegister() && inst.src2.regNum > frame.maxRegNum) {
                frame.maxRegNum = inst.src2.regNum;
            }
            for (const auto& arg : inst.args) {
                if (arg.isRegister() && arg.regNum > frame.maxRegNum) {
                    frame.maxRegNum = arg.regNum;
                }
            }

            if (inst.opcode == IROpcode::Alloca) {
                frame.allocaOffsets[inst.dest.regNum] = frame.totalAllocaBytes;
                size_t sz = inst.allocSize == 0 ? 8 : inst.allocSize;
                frame.totalAllocaBytes += (sz + 7) & ~7ULL; // 8-byte align each alloca
            }

            if (inst.opcode == IROpcode::Call) {
                if (inst.args.size() > maxCallArgs) {
                    maxCallArgs = inst.args.size();
                }
            }
        }
    }

    frame.totalRegBytes = 8 * (frame.maxRegNum + 1);
    frame.outgoingArgBytes = (maxCallArgs > 6) ? (((maxCallArgs - 6) * 8 + 15) & ~15ULL) : 0;
    frame.frameSize = ((frame.totalAllocaBytes + frame.totalRegBytes + frame.outgoingArgBytes) + 15) & ~15ULL;

    return frame;
}

void X86_64CodeGenerator::emitLoadReg(std::ostream& out, const std::string& reg, int regNum, const FunctionFrame& frame) {
    size_t offset = frame.getRegOffset(regNum);
    out << "    movq " << offset << "(%rsp), " << reg << "\n";
}

void X86_64CodeGenerator::emitStoreReg(std::ostream& out, const std::string& reg, int regNum, const FunctionFrame& frame) {
    size_t offset = frame.getRegOffset(regNum);
    out << "    movq " << reg << ", " << offset << "(%rsp)\n";
}

void X86_64CodeGenerator::loadOperand(std::ostream& out, const IROperand& op, const std::string& destReg, const FunctionFrame& frame) {
    switch (op.kind) {
        case IROperand::Kind::ConstantInt: {
            out << "    movq $" << op.intValue << ", " << destReg << "\n";
            break;
        }
        case IROperand::Kind::Register: {
            emitLoadReg(out, destReg, op.regNum, frame);
            break;
        }
        case IROperand::Kind::GlobalVar: {
            std::string sym = mangledName(op.name);
            out << "    leaq " << sym << "(%rip), " << destReg << "\n";
            break;
        }
        case IROperand::Kind::ConstantString: {
            std::string sym = stringLabel(op.name);
            out << "    leaq " << sym << "(%rip), " << destReg << "\n";
            break;
        }
        case IROperand::Kind::None: {
            out << "    xorq " << destReg << ", " << destReg << "\n";
            break;
        }
    }
}

void X86_64CodeGenerator::emitGlobals(const IRModule& module, std::ostream& out) {
    if (module.getGlobals().empty()) return;

    for (const auto& g : module.getGlobals()) {
        std::string sym = mangledName(g.name);
        int align = (g.type == IRType::Ptr) ? 3 : (g.type == IRType::I32 ? 2 : 0);

        if (g.hasInit) {
            if (targetOS == TargetOS::macOS) {
                out << ".section __DATA,__data\n";
            } else {
                out << ".section .data\n";
            }
            out << ".globl " << sym << "\n";
            out << ".p2align " << align << "\n";
            out << sym << ":\n";
            if (g.type == IRType::I8) {
                out << "    .byte " << (g.initValue & 0xFF) << "\n";
            } else if (g.type == IRType::I32) {
                out << "    .long " << static_cast<int32_t>(g.initValue) << "\n";
            } else {
                out << "    .quad " << g.initValue << "\n";
            }
        } else {
            if (targetOS == TargetOS::macOS) {
                out << ".section __DATA,__bss\n";
            } else {
                out << ".section .bss\n";
            }
            out << ".globl " << sym << "\n";
            out << ".p2align " << align << "\n";
            out << sym << ":\n";
            out << "    .zero " << g.sizeBytes << "\n";
        }
    }
    out << "\n";
}

void X86_64CodeGenerator::emitStrings(const IRModule& module, std::ostream& out) {
    if (module.getStrings().empty()) return;

    if (targetOS == TargetOS::macOS) {
        out << ".section __TEXT,__cstring,cstring_literals\n";
    } else {
        out << ".section .rodata\n";
    }

    for (const auto& str : module.getStrings()) {
        std::string label = stringLabel(str.label);
        out << label << ":\n";
        out << "    .asciz \"" << escapeString(str.value) << "\"\n";
    }
    out << "\n";
}

static const char* SYSV_ARG_REGS_64[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

void X86_64CodeGenerator::emitFunction(const IRFunction& fn, const IRModule& module, std::ostream& out) {
    (void)module;
    std::string cleanFn = fn.getName();
    std::replace(cleanFn.begin(), cleanFn.end(), '.', '_');
    std::string sym = mangledName(fn.getName());

    FunctionFrame frame = computeStackLayout(fn);

    if (targetOS == TargetOS::macOS) {
        out << ".section __TEXT,__text,regular,pure_instructions\n";
    } else {
        out << ".section .text\n";
    }
    out << ".globl " << sym << "\n";
    out << sym << ":\n";

    // Prologue
    out << "    pushq %rbp\n";
    out << "    movq %rsp, %rbp\n";
    if (frame.frameSize > 0) {
        out << "    subq $" << frame.frameSize << ", %rsp\n";
    }

    // Save incoming arguments into parameter virtual register slots (%t0, %t1, ...)
    size_t paramCount = fn.getParamNames().size();
    for (size_t i = 0; i < paramCount; ++i) {
        if (i < 6) {
            emitStoreReg(out, SYSV_ARG_REGS_64[i], static_cast<int>(i), frame);
        } else {
            out << "    movq " << (16 + (i - 6) * 8) << "(%rbp), %r10\n";
            emitStoreReg(out, "%r10", static_cast<int>(i), frame);
        }
    }

    int divCounter = 0;

    // Instructions
    for (const auto& block : fn.getBlocks()) {
        std::string cleanBlock = block->getName();
        std::replace(cleanBlock.begin(), cleanBlock.end(), '.', '_');
        out << localLabel(cleanFn + "_" + cleanBlock) << ":\n";

        for (const auto& inst : block->getInstructions()) {
            switch (inst.opcode) {
                case IROpcode::Alloca: {
                    size_t offset = frame.getAllocaOffset(inst.dest.regNum);
                    out << "    leaq " << offset << "(%rsp), %r10\n";
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Load: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    if (inst.type == IRType::I8) {
                        out << "    movsbl (%r10), %eax\n";
                        out << "    movslq %eax, %rax\n";
                    } else if (inst.type == IRType::I32) {
                        out << "    movslq (%r10), %rax\n";
                    } else { // Ptr
                        out << "    movq (%r10), %rax\n";
                    }
                    emitStoreReg(out, "%rax", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Store: {
                    loadOperand(out, inst.dest, "%r10", frame);
                    loadOperand(out, inst.src1, "%r11", frame);
                    if (inst.type == IRType::I8) {
                        out << "    movb %r11b, (%r10)\n";
                    } else if (inst.type == IRType::I32) {
                        out << "    movl %r11d, (%r10)\n";
                    } else { // Ptr
                        out << "    movq %r11, (%r10)\n";
                    }
                    break;
                }
                case IROpcode::GetElementPtr: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    if (inst.offset != 0) {
                        out << "    addq $" << inst.offset << ", %r10\n";
                    }
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Add:
                case IROpcode::Sub:
                case IROpcode::Mul: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    loadOperand(out, inst.src2, "%r11", frame);

                    std::string opStr;
                    if (inst.opcode == IROpcode::Add) opStr = "add";
                    else if (inst.opcode == IROpcode::Sub) opStr = "sub";
                    else opStr = "imul";

                    if (inst.type == IRType::Ptr) {
                        out << "    " << opStr << "q %r11, %r10\n";
                    } else {
                        out << "    " << opStr << "l %r11d, %r10d\n";
                        out << "    movslq %r10d, %r10\n";
                    }
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Div: {
                    int curDiv = divCounter++;
                    std::string zeroLbl = localLabel("div_zero_" + cleanFn + "_" + std::to_string(curDiv));
                    std::string endLbl = localLabel("div_end_" + cleanFn + "_" + std::to_string(curDiv));

                    loadOperand(out, inst.src1, "%r10", frame);
                    loadOperand(out, inst.src2, "%r11", frame);
                    if (inst.type == IRType::Ptr) {
                        out << "    testq %r11, %r11\n";
                        out << "    jz " << zeroLbl << "\n";
                        out << "    movq %r10, %rax\n";
                        out << "    cqto\n";
                        out << "    idivq %r11\n";
                        out << "    movq %rax, %r10\n";
                        out << "    jmp " << endLbl << "\n";
                        out << zeroLbl << ":\n";
                        out << "    xorq %r10, %r10\n";
                        out << endLbl << ":\n";
                    } else {
                        out << "    testl %r11d, %r11d\n";
                        out << "    jz " << zeroLbl << "\n";
                        out << "    movl %r10d, %eax\n";
                        out << "    cltd\n";
                        out << "    idivl %r11d\n";
                        out << "    movslq %eax, %r10\n";
                        out << "    jmp " << endLbl << "\n";
                        out << zeroLbl << ":\n";
                        out << "    xorq %r10, %r10\n";
                        out << endLbl << ":\n";
                    }
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Mod: {
                    int curDiv = divCounter++;
                    std::string zeroLbl = localLabel("mod_zero_" + cleanFn + "_" + std::to_string(curDiv));
                    std::string endLbl = localLabel("mod_end_" + cleanFn + "_" + std::to_string(curDiv));

                    loadOperand(out, inst.src1, "%r10", frame);
                    loadOperand(out, inst.src2, "%r11", frame);
                    if (inst.type == IRType::Ptr) {
                        out << "    testq %r11, %r11\n";
                        out << "    jz " << zeroLbl << "\n";
                        out << "    movq %r10, %rax\n";
                        out << "    cqto\n";
                        out << "    idivq %r11\n";
                        out << "    movq %rdx, %r10\n";
                        out << "    jmp " << endLbl << "\n";
                        out << zeroLbl << ":\n";
                        out << "    xorq %r10, %r10\n";
                        out << endLbl << ":\n";
                    } else {
                        out << "    testl %r11d, %r11d\n";
                        out << "    jz " << zeroLbl << "\n";
                        out << "    movl %r10d, %eax\n";
                        out << "    cltd\n";
                        out << "    idivl %r11d\n";
                        out << "    movslq %edx, %r10\n";
                        out << "    jmp " << endLbl << "\n";
                        out << zeroLbl << ":\n";
                        out << "    xorq %r10, %r10\n";
                        out << endLbl << ":\n";
                    }
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::And:
                case IROpcode::Or:
                case IROpcode::Xor: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    loadOperand(out, inst.src2, "%r11", frame);
                    std::string opStr = (inst.opcode == IROpcode::And) ? "and" :
                                        (inst.opcode == IROpcode::Or ? "or" : "xor");
                    if (inst.type == IRType::Ptr) {
                        out << "    " << opStr << "q %r11, %r10\n";
                    } else {
                        out << "    " << opStr << "l %r11d, %r10d\n";
                        out << "    movslq %r10d, %r10\n";
                    }
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Shl:
                case IROpcode::Shr: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    loadOperand(out, inst.src2, "%rcx", frame);
                    std::string opStr = (inst.opcode == IROpcode::Shl) ? "shl" : "sar";
                    if (inst.type == IRType::Ptr) {
                        out << "    " << opStr << "q %cl, %r10\n";
                    } else {
                        out << "    " << opStr << "l %cl, %r10d\n";
                        out << "    movslq %r10d, %r10\n";
                    }
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::CmpEQ:
                case IROpcode::CmpNE:
                case IROpcode::CmpLT:
                case IROpcode::CmpLE:
                case IROpcode::CmpGT:
                case IROpcode::CmpGE: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    loadOperand(out, inst.src2, "%r11", frame);

                    std::string setcc;
                    if (inst.opcode == IROpcode::CmpEQ) setcc = "sete";
                    else if (inst.opcode == IROpcode::CmpNE) setcc = "setne";
                    else if (inst.opcode == IROpcode::CmpLT) setcc = "setl";
                    else if (inst.opcode == IROpcode::CmpLE) setcc = "setle";
                    else if (inst.opcode == IROpcode::CmpGT) setcc = "setg";
                    else setcc = "setge";

                    if (inst.src1.type == IRType::Ptr) {
                        out << "    cmpq %r11, %r10\n";
                    } else {
                        out << "    cmpl %r11d, %r10d\n";
                    }
                    out << "    " << setcc << " %al\n";
                    out << "    movzbl %al, %eax\n";
                    out << "    movslq %eax, %r10\n";
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Neg: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    if (inst.type == IRType::Ptr) {
                        out << "    negq %r10\n";
                    } else {
                        out << "    negl %r10d\n";
                        out << "    movslq %r10d, %r10\n";
                    }
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Not: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    if (inst.src1.type == IRType::Ptr) {
                        out << "    testq %r10, %r10\n";
                    } else {
                        out << "    testl %r10d, %r10d\n";
                    }
                    out << "    sete %al\n";
                    out << "    movzbl %al, %eax\n";
                    out << "    movslq %eax, %r10\n";
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::BitNot: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    if (inst.type == IRType::Ptr) {
                        out << "    notq %r10\n";
                    } else {
                        out << "    notl %r10d\n";
                        out << "    movslq %r10d, %r10\n";
                    }
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Cast: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    if (inst.type == IRType::I8) {
                        out << "    movsbl %r10b, %eax\n";
                        out << "    movslq %eax, %r10\n";
                    } else if (inst.type == IRType::I32) {
                        out << "    movslq %r10d, %r10\n";
                    }
                    emitStoreReg(out, "%r10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Branch: {
                    std::string target = inst.targetLabel;
                    std::replace(target.begin(), target.end(), '.', '_');
                    out << "    jmp " << localLabel(cleanFn + "_" + target) << "\n";
                    break;
                }
                case IROpcode::CondBranch: {
                    loadOperand(out, inst.src1, "%r10", frame);
                    std::string trueLbl = inst.targetLabel;
                    std::replace(trueLbl.begin(), trueLbl.end(), '.', '_');
                    std::string falseLbl = inst.falseLabel;
                    std::replace(falseLbl.begin(), falseLbl.end(), '.', '_');

                    if (inst.src1.type == IRType::Ptr) {
                        out << "    testq %r10, %r10\n";
                    } else {
                        out << "    testl %r10d, %r10d\n";
                    }
                    out << "    jnz " << localLabel(cleanFn + "_" + trueLbl) << "\n";
                    out << "    jmp " << localLabel(cleanFn + "_" + falseLbl) << "\n";
                    break;
                }
                case IROpcode::Return: {
                    if (!inst.src1.isNone()) {
                        loadOperand(out, inst.src1, "%rax", frame);
                    }
                    out << "    jmp " << localLabel(cleanFn + "_epilogue") << "\n";
                    break;
                }
                case IROpcode::Call: {
                    size_t argCount = inst.args.size();
                    for (size_t i = 0; i < argCount; ++i) {
                        if (i < 6) {
                            loadOperand(out, inst.args[i], SYSV_ARG_REGS_64[i], frame);
                        } else {
                            loadOperand(out, inst.args[i], "%r10", frame);
                            out << "    movq %r10, " << ((i - 6) * 8) << "(%rsp)\n";
                        }
                    }
                    out << "    call " << mangledName(inst.targetLabel) << "\n";
                    if (!inst.dest.isNone()) {
                        if (inst.type != IRType::Ptr) {
                            out << "    movslq %eax, %rax\n";
                        }
                        emitStoreReg(out, "%rax", inst.dest.regNum, frame);
                    }
                    break;
                }
            }
        }
    }

    // Epilogue
    out << localLabel(cleanFn + "_epilogue") << ":\n";
    out << "    movq %rbp, %rsp\n";
    out << "    popq %rbp\n";
    out << "    retq\n\n";
}

void X86_64CodeGenerator::emitBuiltins(const IRModule& module, std::ostream& out) {
    bool hasPrintInt = (module.findFunction("print_int") != nullptr);
    bool hasPrintChar = (module.findFunction("print_char") != nullptr);
    bool hasPrintStr = (module.findFunction("print_str") != nullptr);

    if (targetOS == TargetOS::macOS) {
        out << ".section __TEXT,__text,regular,pure_instructions\n";
    } else {
        out << ".section .text\n";
    }

    if (!hasPrintChar) {
        std::string sym = mangledName("print_char");
        out << ".globl " << sym << "\n";
        out << sym << ":\n";
        out << "    jmp " << mangledName("putchar") << "\n\n";
    }

    if (!hasPrintStr) {
        std::string sym = mangledName("print_str");
        std::string loopLbl = localLabel("ps_loop");
        std::string doneLbl = localLabel("ps_done");

        out << ".globl " << sym << "\n";
        out << sym << ":\n";
        out << "    pushq %rbx\n";
        out << "    movq %rdi, %rbx\n";
        out << "    testq %rbx, %rbx\n";
        out << "    jz " << doneLbl << "\n";
        out << loopLbl << ":\n";
        out << "    movzbl (%rbx), %edi\n";
        out << "    testl %edi, %edi\n";
        out << "    jz " << doneLbl << "\n";
        out << "    call " << mangledName("putchar") << "\n";
        out << "    incq %rbx\n";
        out << "    jmp " << loopLbl << "\n";
        out << doneLbl << ":\n";
        out << "    popq %rbx\n";
        out << "    retq\n\n";
    }

    if (!hasPrintInt) {
        std::string sym = mangledName("print_int");
        std::string notZeroLbl = localLabel("pi_not_zero");
        std::string loopLbl = localLabel("pi_loop");
        std::string signLbl = localLabel("pi_sign");
        std::string doneLbl = localLabel("pi_done");

        out << ".globl " << sym << "\n";
        out << sym << ":\n";
        out << "    pushq %rbp\n";
        out << "    movq %rsp, %rbp\n";
        out << "    pushq %rbx\n";
        out << "    pushq %r12\n";
        out << "    subq $48, %rsp\n";
        out << "    movslq %edi, %rbx\n";
        out << "    leaq -1(%rbp), %r12\n";
        out << "    movb $0, (%r12)\n";
        out << "    decq %r12\n";
        out << "    movb $10, (%r12)\n";
        out << "    testq %rbx, %rbx\n";
        out << "    jnz " << notZeroLbl << "\n";
        out << "    decq %r12\n";
        out << "    movb $48, (%r12)\n";
        out << "    jmp " << doneLbl << "\n";
        out << notZeroLbl << ":\n";
        out << "    xorl %r10d, %r10d\n";
        out << "    cmpq $0, %rbx\n";
        out << "    jge " << loopLbl << "\n";
        out << "    movl $1, %r10d\n";
        out << "    negq %rbx\n";
        out << loopLbl << ":\n";
        out << "    testq %rbx, %rbx\n";
        out << "    jz " << signLbl << "\n";
        out << "    movq %rbx, %rax\n";
        out << "    xorl %edx, %edx\n";
        out << "    movq $10, %rcx\n";
        out << "    divq %rcx\n";
        out << "    addb $48, %dl\n";
        out << "    decq %r12\n";
        out << "    movb %dl, (%r12)\n";
        out << "    movq %rax, %rbx\n";
        out << "    jmp " << loopLbl << "\n";
        out << signLbl << ":\n";
        out << "    testl %r10d, %r10d\n";
        out << "    jz " << doneLbl << "\n";
        out << "    decq %r12\n";
        out << "    movb $45, (%r12)\n";
        out << doneLbl << ":\n";
        out << "    movq %r12, %rdi\n";
        out << "    call " << mangledName("print_str") << "\n";
        out << "    addq $48, %rsp\n";
        out << "    popq %r12\n";
        out << "    popq %rbx\n";
        out << "    popq %rbp\n";
        out << "    retq\n\n";
    }
}

void X86_64CodeGenerator::generate(const IRModule& module, std::ostream& out) {
    if (targetOS == TargetOS::macOS) {
        out << "    .subsections_via_symbols\n\n";
    }

    emitStrings(module, out);
    emitGlobals(module, out);

    for (const auto& fn : module.getFunctions()) {
        emitFunction(*fn, module, out);
    }

    emitBuiltins(module, out);
}
