#include "IRInterpreter.h"
#include <cstring>
#include <stdexcept>

IRInterpreter::IRInterpreter(const IRModule& module, std::ostream& out)
    : module(module), outStream(out), nextAddr(0x1000) {
    memoryPool.resize(0x200000, 0); // 2 MB initial memory
}

uint64_t IRInterpreter::allocateMemory(size_t size) {
    // 8-byte align allocations
    if (nextAddr % 8 != 0) {
        nextAddr += 8 - (nextAddr % 8);
    }
    uint64_t addr = nextAddr;
    nextAddr += size;

    if (nextAddr >= memoryPool.size()) {
        memoryPool.resize(nextAddr + 0x100000, 0);
    }
    return addr;
}

void IRInterpreter::writeMemory(uint64_t addr, int64_t value, IRType type) {
    if (addr >= memoryPool.size()) {
        memoryPool.resize(addr + 64, 0);
    }

    switch (type) {
        case IRType::I8: {
            uint8_t byteVal = static_cast<uint8_t>(value & 0xFF);
            std::memcpy(&memoryPool[addr], &byteVal, 1);
            break;
        }
        case IRType::I32: {
            int32_t intVal = static_cast<int32_t>(value);
            std::memcpy(&memoryPool[addr], &intVal, 4);
            break;
        }
        case IRType::Ptr: {
            uint64_t ptrVal = static_cast<uint64_t>(value);
            std::memcpy(&memoryPool[addr], &ptrVal, 8);
            break;
        }
        default:
            break;
    }
}

int64_t IRInterpreter::readMemory(uint64_t addr, IRType type) {
    if (addr >= memoryPool.size()) {
        return 0;
    }

    switch (type) {
        case IRType::I8: {
            int8_t byteVal = 0;
            std::memcpy(&byteVal, &memoryPool[addr], 1);
            return static_cast<int64_t>(byteVal);
        }
        case IRType::I32: {
            int32_t intVal = 0;
            std::memcpy(&intVal, &memoryPool[addr], 4);
            return static_cast<int64_t>(intVal);
        }
        case IRType::Ptr: {
            uint64_t ptrVal = 0;
            std::memcpy(&ptrVal, &memoryPool[addr], 8);
            return static_cast<int64_t>(ptrVal);
        }
        default:
            return 0;
    }
}

const char* IRInterpreter::getMemoryPointer(uint64_t addr) {
    if (addr >= memoryPool.size()) return "";
    return reinterpret_cast<const char*>(&memoryPool[addr]);
}

void IRInterpreter::initGlobalsAndStrings() {
    // Strings
    for (const auto& s : module.getStrings()) {
        uint64_t addr = allocateMemory(s.value.size() + 1);
        std::memcpy(&memoryPool[addr], s.value.c_str(), s.value.size() + 1);
        stringAddresses[s.label] = addr;
    }

    // Globals
    for (const auto& g : module.getGlobals()) {
        uint64_t addr = allocateMemory(g.sizeBytes);
        if (g.hasInit) {
            writeMemory(addr, g.initValue, g.type);
        } else {
            std::memset(&memoryPool[addr], 0, g.sizeBytes);
        }
        globalAddresses[g.name] = addr;
    }
}

int64_t IRInterpreter::evalOperand(const IROperand& op, StackFrame& frame) {
    switch (op.kind) {
        case IROperand::Kind::ConstantInt:
            return op.intValue;
        case IROperand::Kind::ConstantString: {
            auto it = stringAddresses.find(op.name);
            return (it != stringAddresses.end()) ? static_cast<int64_t>(it->second) : 0;
        }
        case IROperand::Kind::Register: {
            auto it = frame.registers.find(op.regNum);
            return (it != frame.registers.end()) ? it->second : 0;
        }
        case IROperand::Kind::GlobalVar: {
            auto it = globalAddresses.find(op.name);
            return (it != globalAddresses.end()) ? static_cast<int64_t>(it->second) : 0;
        }
        case IROperand::Kind::None:
            return 0;
    }
    return 0;
}

void IRInterpreter::setOperand(const IROperand& op, int64_t value, StackFrame& frame) {
    if (op.isRegister()) {
        frame.registers[op.regNum] = value;
    }
}

bool IRInterpreter::executeBuiltin(const std::string& name, const std::vector<int64_t>& args, int64_t& outResult) {
    if (name == "print_int") {
        int64_t val = args.empty() ? 0 : args[0];
        if (captureMode) {
            capturedOutput << val << "\n";
        } else {
            outStream << val << "\n";
        }
        outResult = 0;
        return true;
    }
    if (name == "print_char") {
        char ch = args.empty() ? '\0' : static_cast<char>(args[0]);
        if (captureMode) {
            capturedOutput << ch;
        } else {
            outStream << ch;
        }
        outResult = 0;
        return true;
    }
    if (name == "print_str") {
        uint64_t addr = args.empty() ? 0 : static_cast<uint64_t>(args[0]);
        const char* str = getMemoryPointer(addr);
        if (captureMode) {
            capturedOutput << str;
        } else {
            outStream << str;
        }
        outResult = 0;
        return true;
    }
    if (name == "putchar") {
        char ch = args.empty() ? '\0' : static_cast<char>(args[0]);
        if (captureMode) {
            capturedOutput << ch;
        } else {
            outStream << ch;
        }
        outResult = static_cast<int64_t>(ch);
        return true;
    }
    if (name == "puts") {
        uint64_t addr = args.empty() ? 0 : static_cast<uint64_t>(args[0]);
        const char* str = getMemoryPointer(addr);
        if (captureMode) {
            capturedOutput << str << "\n";
        } else {
            outStream << str << "\n";
        }
        outResult = 0;
        return true;
    }
    if (name == "exit") {
        exited = true;
        exitCode = args.empty() ? 0 : args[0];
        outResult = exitCode;
        return true;
    }

    return false;
}

int64_t IRInterpreter::executeFunction(const IRFunction& fn, const std::vector<int64_t>& args) {
    StackFrame frame;
    frame.funcName = fn.getName();

    // Map incoming arguments to parameter registers (%t0, %t1, ...)
    for (size_t i = 0; i < args.size(); ++i) {
        frame.registers[static_cast<int>(i)] = args[i];
    }

    const auto& blocks = fn.getBlocks();
    if (blocks.empty()) return 0;

    std::shared_ptr<IRBasicBlock> currentBlock = blocks[0];

    while (currentBlock && !exited) {
        std::shared_ptr<IRBasicBlock> nextBlock = nullptr;

        for (const auto& inst : currentBlock->getInstructions()) {
            if (exited) return exitCode;

            switch (inst.opcode) {
                case IROpcode::Alloca: {
                    uint64_t addr = allocateMemory(inst.allocSize);
                    setOperand(inst.dest, static_cast<int64_t>(addr), frame);
                    break;
                }
                case IROpcode::Load: {
                    uint64_t addr = static_cast<uint64_t>(evalOperand(inst.src1, frame));
                    int64_t val = readMemory(addr, inst.type);
                    setOperand(inst.dest, val, frame);
                    break;
                }
                case IROpcode::Store: {
                    int64_t val = evalOperand(inst.src1, frame);
                    uint64_t addr = static_cast<uint64_t>(evalOperand(inst.dest, frame));
                    writeMemory(addr, val, inst.type);
                    break;
                }
                case IROpcode::GetElementPtr: {
                    uint64_t base = static_cast<uint64_t>(evalOperand(inst.src1, frame));
                    uint64_t addr = base + inst.offset;
                    setOperand(inst.dest, static_cast<int64_t>(addr), frame);
                    break;
                }
                case IROpcode::Add: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a + b, frame);
                    break;
                }
                case IROpcode::Sub: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a - b, frame);
                    break;
                }
                case IROpcode::Mul: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a * b, frame);
                    break;
                }
                case IROpcode::Div: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, b != 0 ? a / b : 0, frame);
                    break;
                }
                case IROpcode::Mod: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, b != 0 ? a % b : 0, frame);
                    break;
                }
                case IROpcode::And: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a & b, frame);
                    break;
                }
                case IROpcode::Or: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a | b, frame);
                    break;
                }
                case IROpcode::Xor: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a ^ b, frame);
                    break;
                }
                case IROpcode::Shl: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a << b, frame);
                    break;
                }
                case IROpcode::Shr: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a >> b, frame);
                    break;
                }
                case IROpcode::CmpEQ: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a == b ? 1 : 0, frame);
                    break;
                }
                case IROpcode::CmpNE: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a != b ? 1 : 0, frame);
                    break;
                }
                case IROpcode::CmpLT: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a < b ? 1 : 0, frame);
                    break;
                }
                case IROpcode::CmpLE: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a <= b ? 1 : 0, frame);
                    break;
                }
                case IROpcode::CmpGT: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a > b ? 1 : 0, frame);
                    break;
                }
                case IROpcode::CmpGE: {
                    int64_t a = evalOperand(inst.src1, frame);
                    int64_t b = evalOperand(inst.src2, frame);
                    setOperand(inst.dest, a >= b ? 1 : 0, frame);
                    break;
                }
                case IROpcode::Neg: {
                    int64_t a = evalOperand(inst.src1, frame);
                    setOperand(inst.dest, -a, frame);
                    break;
                }
                case IROpcode::Not: {
                    int64_t a = evalOperand(inst.src1, frame);
                    setOperand(inst.dest, a == 0 ? 1 : 0, frame);
                    break;
                }
                case IROpcode::BitNot: {
                    int64_t a = evalOperand(inst.src1, frame);
                    setOperand(inst.dest, ~a, frame);
                    break;
                }
                case IROpcode::Cast: {
                    int64_t a = evalOperand(inst.src1, frame);
                    if (inst.type == IRType::I8) a = static_cast<int8_t>(a);
                    else if (inst.type == IRType::I32) a = static_cast<int32_t>(a);
                    setOperand(inst.dest, a, frame);
                    break;
                }
                case IROpcode::Branch: {
                    for (const auto& blk : blocks) {
                        if (blk->getName() == inst.targetLabel) {
                            nextBlock = blk;
                            break;
                        }
                    }
                    break;
                }
                case IROpcode::CondBranch: {
                    int64_t cond = evalOperand(inst.src1, frame);
                    std::string label = (cond != 0) ? inst.targetLabel : inst.falseLabel;
                    for (const auto& blk : blocks) {
                        if (blk->getName() == label) {
                            nextBlock = blk;
                            break;
                        }
                    }
                    break;
                }
                case IROpcode::Return: {
                    if (!inst.src1.isNone()) {
                        frame.returnValue = evalOperand(inst.src1, frame);
                    }
                    return frame.returnValue;
                }
                case IROpcode::Call: {
                    std::vector<int64_t> callArgs;
                    for (const auto& arg : inst.args) {
                        callArgs.push_back(evalOperand(arg, frame));
                    }

                    int64_t retVal = 0;
                    if (executeBuiltin(inst.targetLabel, callArgs, retVal)) {
                        if (!inst.dest.isNone()) {
                            setOperand(inst.dest, retVal, frame);
                        }
                    } else {
                        auto targetFn = module.findFunction(inst.targetLabel);
                        if (targetFn) {
                            retVal = executeFunction(*targetFn, callArgs);
                            if (!inst.dest.isNone()) {
                                setOperand(inst.dest, retVal, frame);
                            }
                        }
                    }
                    break;
                }
            }
        }

        currentBlock = nextBlock;
    }

    return frame.returnValue;
}

int64_t IRInterpreter::run(const std::string& entryFunc, const std::vector<int64_t>& args) {
    initGlobalsAndStrings();
    auto mainFn = module.findFunction(entryFunc);
    if (!mainFn) {
        throw std::runtime_error("Entry function '" + entryFunc + "' not found in IR module");
    }
    return executeFunction(*mainFn, args);
}
