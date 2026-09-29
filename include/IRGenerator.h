#ifndef IR_GENERATOR_H
#define IR_GENERATOR_H

#include <memory>
#include <unordered_map>
#include <vector>
#include "AST.h"
#include "IR.h"
#include "Diagnostic.h"

struct LoopContext {
    std::shared_ptr<IRBasicBlock> continueTarget;
    std::shared_ptr<IRBasicBlock> breakTarget;
};

class IRGenerator {
public:
    IRGenerator(DiagnosticEngine& diag);

    std::unique_ptr<IRModule> generate(Program& program);

private:
    IRType toIRType(const TypePtr& type);
    size_t getTypeSize(const TypePtr& type);

    void emitGlobalVar(VarDecl& decl);
    void emitFunction(FunctionDecl& decl);
    void emitStatement(Stmt& stmt);
    void emitCompoundStmt(CompoundStmt& stmt);

    IROperand emitExpr(Expr& expr);
    IROperand emitAddress(Expr& expr);
    std::optional<int64_t> evaluateConstantExpr(const ExprPtr& expr);

    // Helpers
    void setCurrentBlock(std::shared_ptr<IRBasicBlock> block);
    void emitInstruction(IRInstruction inst);
    IROperand emitAlloca(IRType type, size_t sizeBytes);
    IROperand emitLoad(IRType type, IROperand ptr);
    void emitStore(IRType type, IROperand val, IROperand ptr);
    IROperand emitBinary(IROpcode op, IRType type, IROperand src1, IROperand src2);
    IROperand emitUnary(IROpcode op, IRType type, IROperand src);
    IROperand emitComparison(IROpcode op, IROperand src1, IROperand src2);

    [[maybe_unused]] DiagnosticEngine& diag;
    std::unique_ptr<IRModule> module;
    std::shared_ptr<IRFunction> currentFunction;
    std::shared_ptr<IRBasicBlock> currentBlock;

    // Scoped variable map: name -> address operand
    std::vector<std::unordered_map<std::string, IROperand>> localVars;
    std::vector<LoopContext> loopStack;

    void enterScope();
    void exitScope();
    void declareLocal(const std::string& name, IROperand addr);
    IROperand lookupVariable(const std::string& name);
};

#endif // IR_GENERATOR_H
