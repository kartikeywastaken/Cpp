#include "IR.h"
#include <sstream>

std::string irTypeToString(IRType type) {
    switch (type) {
        case IRType::Void: return "void";
        case IRType::I8:   return "i8";
        case IRType::I32:  return "i32";
        case IRType::Ptr:  return "ptr";
    }
    return "void";
}

std::string irOpcodeToString(IROpcode op) {
    switch (op) {
        case IROpcode::Alloca:        return "alloca";
        case IROpcode::Load:          return "load";
        case IROpcode::Store:         return "store";
        case IROpcode::GetElementPtr: return "gep";
        case IROpcode::Add:           return "add";
        case IROpcode::Sub:           return "sub";
        case IROpcode::Mul:           return "mul";
        case IROpcode::Div:           return "sdiv";
        case IROpcode::Mod:           return "smod";
        case IROpcode::And:           return "and";
        case IROpcode::Or:            return "or";
        case IROpcode::Xor:           return "xor";
        case IROpcode::Shl:           return "shl";
        case IROpcode::Shr:           return "shr";
        case IROpcode::CmpEQ:         return "cmp_eq";
        case IROpcode::CmpNE:         return "cmp_ne";
        case IROpcode::CmpLT:         return "cmp_lt";
        case IROpcode::CmpLE:         return "cmp_le";
        case IROpcode::CmpGT:         return "cmp_gt";
        case IROpcode::CmpGE:         return "cmp_ge";
        case IROpcode::Neg:           return "neg";
        case IROpcode::Not:           return "not";
        case IROpcode::BitNot:        return "bitnot";
        case IROpcode::Cast:          return "cast";
        case IROpcode::Branch:        return "br";
        case IROpcode::CondBranch:    return "condbr";
        case IROpcode::Return:        return "ret";
        case IROpcode::Call:          return "call";
    }
    return "unknown";
}

std::string IROperand::toString() const {
    switch (kind) {
        case Kind::ConstantInt:
            return std::to_string(intValue);
        case Kind::ConstantString:
            return "@" + name;
        case Kind::Register:
            return "%t" + std::to_string(regNum);
        case Kind::GlobalVar:
            return "@" + name;
        case Kind::None:
            return "none";
    }
    return "none";
}

std::string IRInstruction::toString() const {
    std::ostringstream ss;

    if (!dest.isNone() && opcode != IROpcode::Store && opcode != IROpcode::Branch &&
        opcode != IROpcode::CondBranch && opcode != IROpcode::Return) {
        ss << dest.toString() << " = ";
    }

    ss << irOpcodeToString(opcode) << " ";
    if (type != IRType::Void) {
        ss << irTypeToString(type) << " ";
    }

    switch (opcode) {
        case IROpcode::Alloca:
            ss << "(bytes=" << allocSize << ")";
            break;
        case IROpcode::Load:
            ss << src1.toString();
            break;
        case IROpcode::Store:
            ss << src1.toString() << ", " << dest.toString();
            break;
        case IROpcode::GetElementPtr:
            ss << src1.toString() << ", offset=" << offset;
            break;
        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Mod:
        case IROpcode::And:
        case IROpcode::Or:
        case IROpcode::Xor:
        case IROpcode::Shl:
        case IROpcode::Shr:
        case IROpcode::CmpEQ:
        case IROpcode::CmpNE:
        case IROpcode::CmpLT:
        case IROpcode::CmpLE:
        case IROpcode::CmpGT:
        case IROpcode::CmpGE:
            ss << src1.toString() << ", " << src2.toString();
            break;
        case IROpcode::Neg:
        case IROpcode::Not:
        case IROpcode::BitNot:
        case IROpcode::Cast:
            ss << src1.toString();
            break;
        case IROpcode::Branch:
            ss << "label %" << targetLabel;
            break;
        case IROpcode::CondBranch:
            ss << src1.toString() << ", label %" << targetLabel << ", label %" << falseLabel;
            break;
        case IROpcode::Return:
            if (!src1.isNone()) {
                ss << src1.toString();
            }
            break;
        case IROpcode::Call:
            ss << "@" << targetLabel << "(";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << args[i].toString();
            }
            ss << ")";
            break;
    }

    return ss.str();
}

std::shared_ptr<IRBasicBlock> IRFunction::createBlock(const std::string& prefix) {
    std::string bname = prefix + "_" + std::to_string(nextBlockId++);
    auto block = std::make_shared<IRBasicBlock>(bname);
    blocks.push_back(block);
    return block;
}

void IRFunction::addBlock(std::shared_ptr<IRBasicBlock> block) {
    blocks.push_back(std::move(block));
}

std::shared_ptr<IRFunction> IRModule::findFunction(const std::string& name) const {
    for (const auto& fn : functions) {
        if (fn->getName() == name) return fn;
    }
    return nullptr;
}

const IRGlobalVar* IRModule::findGlobal(const std::string& name) const {
    for (const auto& g : globals) {
        if (g.name == name) return &g;
    }
    return nullptr;
}

const IRStringConstant* IRModule::findString(const std::string& label) const {
    for (const auto& s : strings) {
        if (s.label == label) return &s;
    }
    return nullptr;
}

std::string IRModule::allocateStringLiteral(const std::string& value) {
    for (const auto& s : strings) {
        if (s.value == value) return s.label;
    }
    std::string label = "str." + std::to_string(nextStringId++);
    strings.push_back({label, value});
    return label;
}
