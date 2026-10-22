#include "CodeGenARM64.h"
#include <iomanip>
#include <sstream>
#include <algorithm>

ARM64CodeGenerator::ARM64CodeGenerator(TargetOS os)
    : targetOS(os) {}

std::string ARM64CodeGenerator::mangledName(const std::string& name) const {
    if (targetOS == TargetOS::macOS) {
        return "_" + name;
    }
    return name;
}

std::string ARM64CodeGenerator::stringLabel(const std::string& label) const {
    std::string clean = label;
    std::replace(clean.begin(), clean.end(), '.', '_');
    if (targetOS == TargetOS::macOS) {
        return "l_" + clean;
    }
    return ".L" + clean;
}

std::string ARM64CodeGenerator::localLabel(const std::string& name) const {
    if (targetOS == TargetOS::macOS) {
        return "L_" + name;
    }
    return ".L" + name;
}

std::string ARM64CodeGenerator::escapeString(const std::string& str) const {
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

void ARM64CodeGenerator::emitMovImm32(std::ostream& out, const std::string& reg, int32_t val) {
    if (val >= -32768 && val <= 65535) {
        out << "    mov " << reg << ", #" << val << "\n";
    } else {
        uint32_t u = static_cast<uint32_t>(val);
        out << "    mov " << reg << ", #" << (u & 0xFFFF) << "\n";
        if ((u >> 16) & 0xFFFF) {
            out << "    movk " << reg << ", #" << ((u >> 16) & 0xFFFF) << ", lsl #16\n";
        }
    }
}

void ARM64CodeGenerator::emitMovImm64(std::ostream& out, const std::string& reg, int64_t val) {
    if (val >= -32768 && val <= 65535) {
        out << "    mov " << reg << ", #" << val << "\n";
    } else {
        uint64_t u = static_cast<uint64_t>(val);
        out << "    mov " << reg << ", #" << (u & 0xFFFF) << "\n";
        if ((u >> 16) & 0xFFFF) {
            out << "    movk " << reg << ", #" << ((u >> 16) & 0xFFFF) << ", lsl #16\n";
        }
        if ((u >> 32) & 0xFFFF) {
            out << "    movk " << reg << ", #" << ((u >> 32) & 0xFFFF) << ", lsl #32\n";
        }
        if ((u >> 48) & 0xFFFF) {
            out << "    movk " << reg << ", #" << ((u >> 48) & 0xFFFF) << ", lsl #48\n";
        }
    }
}

ARM64CodeGenerator::FunctionFrame ARM64CodeGenerator::computeStackLayout(const IRFunction& fn) {
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
    frame.outgoingArgBytes = (maxCallArgs > 8) ? (((maxCallArgs - 8) * 8 + 15) & ~15ULL) : 0;
    frame.frameSize = ((frame.totalAllocaBytes + frame.totalRegBytes + frame.outgoingArgBytes) + 15) & ~15ULL;

    return frame;
}

void ARM64CodeGenerator::emitLoadReg(std::ostream& out, const std::string& reg, int regNum, const FunctionFrame& frame) {
    size_t offset = frame.getRegOffset(regNum);
    if (offset <= 32760 && (offset % 8 == 0)) {
        out << "    ldr " << reg << ", [sp, #" << offset << "]\n";
    } else {
        emitMovImm64(out, "x16", offset);
        out << "    ldr " << reg << ", [sp, x16]\n";
    }
}

void ARM64CodeGenerator::emitStoreReg(std::ostream& out, const std::string& reg, int regNum, const FunctionFrame& frame) {
    size_t offset = frame.getRegOffset(regNum);
    if (offset <= 32760 && (offset % 8 == 0)) {
        out << "    str " << reg << ", [sp, #" << offset << "]\n";
    } else {
        emitMovImm64(out, "x16", offset);
        out << "    str " << reg << ", [sp, x16]\n";
    }
}

void ARM64CodeGenerator::loadOperand(std::ostream& out, const IROperand& op, const std::string& destReg, const FunctionFrame& frame) {
    switch (op.kind) {
        case IROperand::Kind::ConstantInt: {
            if (destReg[0] == 'w') {
                emitMovImm32(out, destReg, static_cast<int32_t>(op.intValue));
            } else {
                emitMovImm64(out, destReg, op.intValue);
            }
            break;
        }
        case IROperand::Kind::Register: {
            emitLoadReg(out, destReg, op.regNum, frame);
            break;
        }
        case IROperand::Kind::GlobalVar: {
            std::string sym = mangledName(op.name);
            std::string reg64 = destReg;
            if (reg64[0] == 'w') reg64[0] = 'x';
            if (targetOS == TargetOS::macOS) {
                out << "    adrp " << reg64 << ", " << sym << "@PAGE\n";
                out << "    add " << reg64 << ", " << reg64 << ", " << sym << "@PAGEOFF\n";
            } else {
                out << "    adrp " << reg64 << ", " << sym << "\n";
                out << "    add " << reg64 << ", " << reg64 << ", :lo12:" << sym << "\n";
            }
            break;
        }
        case IROperand::Kind::ConstantString: {
            std::string sym = stringLabel(op.name);
            std::string reg64 = destReg;
            if (reg64[0] == 'w') reg64[0] = 'x';
            if (targetOS == TargetOS::macOS) {
                out << "    adrp " << reg64 << ", " << sym << "@PAGE\n";
                out << "    add " << reg64 << ", " << reg64 << ", " << sym << "@PAGEOFF\n";
            } else {
                out << "    adrp " << reg64 << ", " << sym << "\n";
                out << "    add " << reg64 << ", " << reg64 << ", :lo12:" << sym << "\n";
            }
            break;
        }
        case IROperand::Kind::None: {
            out << "    mov " << destReg << ", #0\n";
            break;
        }
    }
}

void ARM64CodeGenerator::emitGlobals(const IRModule& module, std::ostream& out) {
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

void ARM64CodeGenerator::emitStrings(const IRModule& module, std::ostream& out) {
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

void ARM64CodeGenerator::emitFunction(const IRFunction& fn, const IRModule& module, std::ostream& out) {
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
    out << ".p2align 2\n";
    out << sym << ":\n";

    // Prologue
    out << "    stp x29, x30, [sp, #-16]!\n";
    out << "    mov x29, sp\n";
    if (frame.frameSize > 0) {
        if (frame.frameSize <= 4095) {
            out << "    sub sp, sp, #" << frame.frameSize << "\n";
        } else {
            emitMovImm64(out, "x16", frame.frameSize);
            out << "    sub sp, sp, x16\n";
        }
    }

    // Save incoming arguments into parameter virtual register slots (%t0, %t1, ...)
    size_t paramCount = fn.getParamNames().size();
    for (size_t i = 0; i < paramCount; ++i) {
        if (i < 8) {
            emitStoreReg(out, "x" + std::to_string(i), static_cast<int>(i), frame);
        } else {
            out << "    ldr x16, [x29, #" << (16 + (i - 8) * 8) << "]\n";
            emitStoreReg(out, "x16", static_cast<int>(i), frame);
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
                    if (offset <= 4095) {
                        out << "    add x8, sp, #" << offset << "\n";
                    } else {
                        emitMovImm64(out, "x16", offset);
                        out << "    add x8, sp, x16\n";
                    }
                    emitStoreReg(out, "x8", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Load: {
                    loadOperand(out, inst.src1, "x8", frame);
                    if (inst.type == IRType::I8) {
                        out << "    ldrsb w9, [x8]\n";
                        out << "    sxtw x9, w9\n";
                    } else if (inst.type == IRType::I32) {
                        out << "    ldr w9, [x8]\n";
                        out << "    sxtw x9, w9\n";
                    } else { // Ptr
                        out << "    ldr x9, [x8]\n";
                    }
                    emitStoreReg(out, "x9", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Store: {
                    loadOperand(out, inst.dest, "x8", frame);
                    if (inst.type == IRType::I8) {
                        loadOperand(out, inst.src1, "w9", frame);
                        out << "    strb w9, [x8]\n";
                    } else if (inst.type == IRType::I32) {
                        loadOperand(out, inst.src1, "w9", frame);
                        out << "    str w9, [x8]\n";
                    } else { // Ptr
                        loadOperand(out, inst.src1, "x9", frame);
                        out << "    str x9, [x8]\n";
                    }
                    break;
                }
                case IROpcode::GetElementPtr: {
                    loadOperand(out, inst.src1, "x8", frame);
                    if (inst.offset != 0) {
                        if (inst.offset > 0 && inst.offset <= 4095) {
                            out << "    add x8, x8, #" << inst.offset << "\n";
                        } else if (inst.offset < 0 && -inst.offset <= 4095) {
                            out << "    sub x8, x8, #" << (-inst.offset) << "\n";
                        } else {
                            emitMovImm64(out, "x16", inst.offset);
                            out << "    add x8, x8, x16\n";
                        }
                    }
                    emitStoreReg(out, "x8", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Add:
                case IROpcode::Sub:
                case IROpcode::Mul: {
                    std::string opStr;
                    if (inst.opcode == IROpcode::Add) opStr = "add";
                    else if (inst.opcode == IROpcode::Sub) opStr = "sub";
                    else opStr = "mul";

                    if (inst.type == IRType::Ptr) {
                        loadOperand(out, inst.src1, "x8", frame);
                        loadOperand(out, inst.src2, "x9", frame);
                        out << "    " << opStr << " x10, x8, x9\n";
                    } else {
                        loadOperand(out, inst.src1, "w8", frame);
                        loadOperand(out, inst.src2, "w9", frame);
                        out << "    " << opStr << " w10, w8, w9\n";
                        out << "    sxtw x10, w10\n";
                    }
                    emitStoreReg(out, "x10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Div: {
                    int curDiv = divCounter++;
                    std::string zeroLbl = localLabel("div_zero_" + cleanFn + "_" + std::to_string(curDiv));
                    std::string endLbl = localLabel("div_end_" + cleanFn + "_" + std::to_string(curDiv));

                    if (inst.type == IRType::Ptr) {
                        loadOperand(out, inst.src1, "x8", frame);
                        loadOperand(out, inst.src2, "x9", frame);
                        out << "    cbz x9, " << zeroLbl << "\n";
                        out << "    sdiv x10, x8, x9\n";
                        out << "    b " << endLbl << "\n";
                        out << zeroLbl << ":\n";
                        out << "    mov x10, #0\n";
                        out << endLbl << ":\n";
                    } else {
                        loadOperand(out, inst.src1, "w8", frame);
                        loadOperand(out, inst.src2, "w9", frame);
                        out << "    cbz w9, " << zeroLbl << "\n";
                        out << "    sdiv w10, w8, w9\n";
                        out << "    b " << endLbl << "\n";
                        out << zeroLbl << ":\n";
                        out << "    mov w10, #0\n";
                        out << endLbl << ":\n";
                        out << "    sxtw x10, w10\n";
                    }
                    emitStoreReg(out, "x10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Mod: {
                    int curDiv = divCounter++;
                    std::string zeroLbl = localLabel("mod_zero_" + cleanFn + "_" + std::to_string(curDiv));
                    std::string endLbl = localLabel("mod_end_" + cleanFn + "_" + std::to_string(curDiv));

                    if (inst.type == IRType::Ptr) {
                        loadOperand(out, inst.src1, "x8", frame);
                        loadOperand(out, inst.src2, "x9", frame);
                        out << "    cbz x9, " << zeroLbl << "\n";
                        out << "    sdiv x11, x8, x9\n";
                        out << "    msub x10, x11, x9, x8\n";
                        out << "    b " << endLbl << "\n";
                        out << zeroLbl << ":\n";
                        out << "    mov x10, #0\n";
                        out << endLbl << ":\n";
                    } else {
                        loadOperand(out, inst.src1, "w8", frame);
                        loadOperand(out, inst.src2, "w9", frame);
                        out << "    cbz w9, " << zeroLbl << "\n";
                        out << "    sdiv w11, w8, w9\n";
                        out << "    msub w10, w11, w9, w8\n";
                        out << "    b " << endLbl << "\n";
                        out << zeroLbl << ":\n";
                        out << "    mov w10, #0\n";
                        out << endLbl << ":\n";
                        out << "    sxtw x10, w10\n";
                    }
                    emitStoreReg(out, "x10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::And:
                case IROpcode::Or:
                case IROpcode::Xor:
                case IROpcode::Shl:
                case IROpcode::Shr: {
                    std::string opStr;
                    if (inst.opcode == IROpcode::And) opStr = "and";
                    else if (inst.opcode == IROpcode::Or) opStr = "orr";
                    else if (inst.opcode == IROpcode::Xor) opStr = "eor";
                    else if (inst.opcode == IROpcode::Shl) opStr = "lsl";
                    else opStr = "asr";

                    if (inst.type == IRType::Ptr) {
                        loadOperand(out, inst.src1, "x8", frame);
                        loadOperand(out, inst.src2, "x9", frame);
                        out << "    " << opStr << " x10, x8, x9\n";
                    } else {
                        loadOperand(out, inst.src1, "w8", frame);
                        loadOperand(out, inst.src2, "w9", frame);
                        out << "    " << opStr << " w10, w8, w9\n";
                        out << "    sxtw x10, w10\n";
                    }
                    emitStoreReg(out, "x10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::CmpEQ:
                case IROpcode::CmpNE:
                case IROpcode::CmpLT:
                case IROpcode::CmpLE:
                case IROpcode::CmpGT:
                case IROpcode::CmpGE: {
                    std::string cc;
                    if (inst.opcode == IROpcode::CmpEQ) cc = "eq";
                    else if (inst.opcode == IROpcode::CmpNE) cc = "ne";
                    else if (inst.opcode == IROpcode::CmpLT) cc = "lt";
                    else if (inst.opcode == IROpcode::CmpLE) cc = "le";
                    else if (inst.opcode == IROpcode::CmpGT) cc = "gt";
                    else cc = "ge";

                    if (inst.src1.type == IRType::Ptr) {
                        loadOperand(out, inst.src1, "x8", frame);
                        loadOperand(out, inst.src2, "x9", frame);
                        out << "    cmp x8, x9\n";
                    } else {
                        loadOperand(out, inst.src1, "w8", frame);
                        loadOperand(out, inst.src2, "w9", frame);
                        out << "    cmp w8, w9\n";
                    }
                    out << "    cset w10, " << cc << "\n";
                    out << "    uxtw x10, w10\n";
                    emitStoreReg(out, "x10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Neg: {
                    if (inst.type == IRType::Ptr) {
                        loadOperand(out, inst.src1, "x8", frame);
                        out << "    neg x10, x8\n";
                    } else {
                        loadOperand(out, inst.src1, "w8", frame);
                        out << "    neg w10, w8\n";
                        out << "    sxtw x10, w10\n";
                    }
                    emitStoreReg(out, "x10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Not: {
                    if (inst.src1.type == IRType::Ptr) {
                        loadOperand(out, inst.src1, "x8", frame);
                        out << "    cmp x8, #0\n";
                    } else {
                        loadOperand(out, inst.src1, "w8", frame);
                        out << "    cmp w8, #0\n";
                    }
                    out << "    cset w10, eq\n";
                    out << "    uxtw x10, w10\n";
                    emitStoreReg(out, "x10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::BitNot: {
                    if (inst.type == IRType::Ptr) {
                        loadOperand(out, inst.src1, "x8", frame);
                        out << "    mvn x10, x8\n";
                    } else {
                        loadOperand(out, inst.src1, "w8", frame);
                        out << "    mvn w10, w8\n";
                        out << "    sxtw x10, w10\n";
                    }
                    emitStoreReg(out, "x10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Cast: {
                    loadOperand(out, inst.src1, "x8", frame);
                    if (inst.type == IRType::I8) {
                        out << "    sxtb w10, w8\n";
                        out << "    sxtw x10, w10\n";
                    } else if (inst.type == IRType::I32) {
                        out << "    sxtw x10, w8\n";
                    } else { // Ptr
                        out << "    mov x10, x8\n";
                    }
                    emitStoreReg(out, "x10", inst.dest.regNum, frame);
                    break;
                }
                case IROpcode::Branch: {
                    std::string target = inst.targetLabel;
                    std::replace(target.begin(), target.end(), '.', '_');
                    out << "    b " << localLabel(cleanFn + "_" + target) << "\n";
                    break;
                }
                case IROpcode::CondBranch: {
                    std::string trueLbl = inst.targetLabel;
                    std::replace(trueLbl.begin(), trueLbl.end(), '.', '_');
                    std::string falseLbl = inst.falseLabel;
                    std::replace(falseLbl.begin(), falseLbl.end(), '.', '_');

                    if (inst.src1.type == IRType::Ptr) {
                        loadOperand(out, inst.src1, "x8", frame);
                        out << "    cbnz x8, " << localLabel(cleanFn + "_" + trueLbl) << "\n";
                    } else {
                        loadOperand(out, inst.src1, "w8", frame);
                        out << "    cbnz w8, " << localLabel(cleanFn + "_" + trueLbl) << "\n";
                    }
                    out << "    b " << localLabel(cleanFn + "_" + falseLbl) << "\n";
                    break;
                }
                case IROpcode::Return: {
                    if (!inst.src1.isNone()) {
                        if (inst.type == IRType::Ptr) {
                            loadOperand(out, inst.src1, "x0", frame);
                        } else {
                            loadOperand(out, inst.src1, "w0", frame);
                        }
                    }
                    out << "    b " << localLabel(cleanFn + "_epilogue") << "\n";
                    break;
                }
                case IROpcode::Call: {
                    size_t argCount = inst.args.size();
                    for (size_t i = 0; i < argCount; ++i) {
                        if (i < 8) {
                            loadOperand(out, inst.args[i], "x" + std::to_string(i), frame);
                        } else {
                            loadOperand(out, inst.args[i], "x10", frame);
                            out << "    str x10, [sp, #" << ((i - 8) * 8) << "]\n";
                        }
                    }
                    out << "    bl " << mangledName(inst.targetLabel) << "\n";
                    if (!inst.dest.isNone()) {
                        if (inst.type == IRType::Ptr) {
                            emitStoreReg(out, "x0", inst.dest.regNum, frame);
                        } else {
                            out << "    sxtw x0, w0\n";
                            emitStoreReg(out, "x0", inst.dest.regNum, frame);
                        }
                    }
                    break;
                }
            }
        }
    }

    // Epilogue
    out << localLabel(cleanFn + "_epilogue") << ":\n";
    out << "    mov sp, x29\n";
    out << "    ldp x29, x30, [sp], #16\n";
    out << "    ret\n\n";
}

void ARM64CodeGenerator::emitBuiltins(const IRModule& module, std::ostream& out) {
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
        out << ".p2align 2\n";
        out << sym << ":\n";
        out << "    b " << mangledName("putchar") << "\n\n";
    }

    if (!hasPrintStr) {
        std::string sym = mangledName("print_str");
        std::string loopLbl = localLabel("ps_loop");
        std::string doneLbl = localLabel("ps_done");

        out << ".globl " << sym << "\n";
        out << ".p2align 2\n";
        out << sym << ":\n";
        out << "    stp x29, x30, [sp, #-32]!\n";
        out << "    mov x29, sp\n";
        out << "    str x19, [sp, #16]\n";
        out << "    mov x19, x0\n";
        out << "    cbz x19, " << doneLbl << "\n";
        out << loopLbl << ":\n";
        out << "    ldrb w0, [x19]\n";
        out << "    cbz w0, " << doneLbl << "\n";
        out << "    bl " << mangledName("putchar") << "\n";
        out << "    add x19, x19, #1\n";
        out << "    b " << loopLbl << "\n";
        out << doneLbl << ":\n";
        out << "    ldr x19, [sp, #16]\n";
        out << "    ldp x29, x30, [sp], #32\n";
        out << "    ret\n\n";
    }

    if (!hasPrintInt) {
        std::string sym = mangledName("print_int");
        std::string notZeroLbl = localLabel("pi_not_zero");
        std::string loopLbl = localLabel("pi_loop");
        std::string signLbl = localLabel("pi_sign");
        std::string doneLbl = localLabel("pi_done");

        out << ".globl " << sym << "\n";
        out << ".p2align 2\n";
        out << sym << ":\n";
        out << "    stp x29, x30, [sp, #-48]!\n";
        out << "    mov x29, sp\n";
        out << "    str x19, [sp, #32]\n";
        out << "    str x20, [sp, #40]\n";
        out << "    sxtw x19, w0\n";
        out << "    add x20, sp, #30\n";
        out << "    mov w8, #0\n";
        out << "    strb w8, [x20]\n";
        out << "    mov w8, #10\n"; // '\n'
        out << "    sub x20, x20, #1\n";
        out << "    strb w8, [x20]\n";
        out << "    cbnz x19, " << notZeroLbl << "\n";
        out << "    sub x20, x20, #1\n";
        out << "    mov w8, #'0'\n";
        out << "    strb w8, [x20]\n";
        out << "    b " << doneLbl << "\n";
        out << notZeroLbl << ":\n";
        out << "    mov w8, #0\n";
        out << "    cmp x19, #0\n";
        out << "    bge " << loopLbl << "\n";
        out << "    mov w8, #1\n";
        out << "    neg x19, x19\n";
        out << loopLbl << ":\n";
        out << "    cbz x19, " << signLbl << "\n";
        out << "    mov x9, #10\n";
        out << "    udiv x10, x19, x9\n";
        out << "    msub x11, x10, x9, x19\n";
        out << "    add x11, x11, #'0'\n";
        out << "    sub x20, x20, #1\n";
        out << "    strb w11, [x20]\n";
        out << "    mov x19, x10\n";
        out << "    b " << loopLbl << "\n";
        out << signLbl << ":\n";
        out << "    cbz w8, " << doneLbl << "\n";
        out << "    sub x20, x20, #1\n";
        out << "    mov w8, #'-'\n";
        out << "    strb w8, [x20]\n";
        out << doneLbl << ":\n";
        out << "    mov x0, x20\n";
        out << "    bl " << mangledName("print_str") << "\n";
        out << "    ldr x19, [sp, #32]\n";
        out << "    ldr x20, [sp, #40]\n";
        out << "    ldp x29, x30, [sp], #48\n";
        out << "    ret\n\n";
    }
}

void ARM64CodeGenerator::generate(const IRModule& module, std::ostream& out) {
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
