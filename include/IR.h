#ifndef IR_H
#define IR_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>

enum class IRType {
    Void,
    I8,
    I32,
    Ptr
};

std::string irTypeToString(IRType type);

enum class IROpcode {
    // Memory
    Alloca,
    Load,
    Store,
    GetElementPtr,

    // Binary Arithmetic & Bitwise
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    And,
    Or,
    Xor,
    Shl,
    Shr,

    // Comparisons
    CmpEQ,
    CmpNE,
    CmpLT,
    CmpLE,
    CmpGT,
    CmpGE,

    // Unary
    Neg,
    Not,
    BitNot,

    // Cast
    Cast,

    // Control Flow
    Branch,
    CondBranch,
    Return,

    // Calls
    Call
};

std::string irOpcodeToString(IROpcode op);

struct IROperand {
    enum class Kind {
        ConstantInt,
        ConstantString,
        Register,
        GlobalVar,
        None
    };

    Kind kind = Kind::None;
    IRType type = IRType::I32;
    int64_t intValue = 0;
    int regNum = -1;
    std::string name; // for global var or string label

    bool isNone() const { return kind == Kind::None; }
    bool isConstant() const { return kind == Kind::ConstantInt; }
    bool isRegister() const { return kind == Kind::Register; }
    bool isGlobal() const { return kind == Kind::GlobalVar; }

    static IROperand makeInt(int64_t val, IRType type = IRType::I32) {
        IROperand op;
        op.kind = Kind::ConstantInt;
        op.type = type;
        op.intValue = val;
        return op;
    }

    static IROperand makeReg(int num, IRType type = IRType::I32) {
        IROperand op;
        op.kind = Kind::Register;
        op.type = type;
        op.regNum = num;
        return op;
    }

    static IROperand makeGlobal(const std::string& name, IRType type = IRType::Ptr) {
        IROperand op;
        op.kind = Kind::GlobalVar;
        op.type = type;
        op.name = name;
        return op;
    }

    static IROperand makeString(const std::string& label) {
        IROperand op;
        op.kind = Kind::ConstantString;
        op.type = IRType::Ptr;
        op.name = label;
        return op;
    }

    static IROperand makeNone() {
        return IROperand();
    }

    std::string toString() const;
};

struct IRInstruction {
    IROpcode opcode;
    IRType type = IRType::Void;
    IROperand dest = IROperand::makeNone();
    IROperand src1 = IROperand::makeNone();
    IROperand src2 = IROperand::makeNone();
    std::vector<IROperand> args; // For Call
    std::string targetLabel;     // For Branch / CondBranch true
    std::string falseLabel;      // For CondBranch false
    size_t allocSize = 0;        // For Alloca in bytes
    int64_t offset = 0;          // For GetElementPtr byte offset

    std::string toString() const;
    bool isTerminator() const {
        return opcode == IROpcode::Branch ||
               opcode == IROpcode::CondBranch ||
               opcode == IROpcode::Return;
    }
};

class IRBasicBlock {
public:
    explicit IRBasicBlock(std::string name) : name(std::move(name)) {}

    const std::string& getName() const { return name; }
    const std::vector<IRInstruction>& getInstructions() const { return instructions; }
    std::vector<IRInstruction>& getInstructions() { return instructions; }

    void addInstruction(IRInstruction inst) {
        instructions.push_back(std::move(inst));
    }

    bool hasTerminator() const {
        return !instructions.empty() && instructions.back().isTerminator();
    }

private:
    std::string name;
    std::vector<IRInstruction> instructions;
};

class IRFunction {
public:
    IRFunction(std::string name, IRType returnType,
               std::vector<std::string> paramNames,
               std::vector<IRType> paramTypes)
        : name(std::move(name)), returnType(returnType),
          paramNames(std::move(paramNames)), paramTypes(std::move(paramTypes)) {}

    const std::string& getName() const { return name; }
    IRType getReturnType() const { return returnType; }
    const std::vector<std::string>& getParamNames() const { return paramNames; }
    const std::vector<IRType>& getParamTypes() const { return paramTypes; }

    const std::vector<std::shared_ptr<IRBasicBlock>>& getBlocks() const { return blocks; }
    std::vector<std::shared_ptr<IRBasicBlock>>& getBlocks() { return blocks; }

    std::shared_ptr<IRBasicBlock> createBlock(const std::string& prefix = "bb");
    void addBlock(std::shared_ptr<IRBasicBlock> block);

    int allocateRegister() { return nextRegNum++; }

private:
    std::string name;
    IRType returnType;
    std::vector<std::string> paramNames;
    std::vector<IRType> paramTypes;
    std::vector<std::shared_ptr<IRBasicBlock>> blocks;
    int nextRegNum = 0;
    int nextBlockId = 0;
};

struct IRGlobalVar {
    std::string name;
    IRType type;
    size_t sizeBytes;
    int64_t initValue = 0;
    bool hasInit = false;
};

struct IRStringConstant {
    std::string label;
    std::string value;
};

class IRModule {
public:
    IRModule() = default;

    void addGlobalVar(IRGlobalVar gvar) { globals.push_back(std::move(gvar)); }
    void addStringConstant(IRStringConstant str) { strings.push_back(std::move(str)); }
    void addFunction(std::shared_ptr<IRFunction> fn) { functions.push_back(std::move(fn)); }

    const std::vector<IRGlobalVar>& getGlobals() const { return globals; }
    const std::vector<IRStringConstant>& getStrings() const { return strings; }
    const std::vector<std::shared_ptr<IRFunction>>& getFunctions() const { return functions; }

    std::shared_ptr<IRFunction> findFunction(const std::string& name) const;
    const IRGlobalVar* findGlobal(const std::string& name) const;
    const IRStringConstant* findString(const std::string& label) const;

    std::string allocateStringLiteral(const std::string& value);

private:
    std::vector<IRGlobalVar> globals;
    std::vector<IRStringConstant> strings;
    std::vector<std::shared_ptr<IRFunction>> functions;
    int nextStringId = 0;
};

#endif // IR_H
