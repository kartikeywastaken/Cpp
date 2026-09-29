#include "IRGenerator.h"

IRGenerator::IRGenerator(DiagnosticEngine& diag)
    : diag(diag) {}

IRType IRGenerator::toIRType(const TypePtr& type) {
    if (!type || type->isVoid()) return IRType::Void;
    if (type->isChar()) return IRType::I8;
    if (type->isInt()) return IRType::I32;
    return IRType::Ptr; // pointer, array, struct
}

size_t IRGenerator::getTypeSize(const TypePtr& type) {
    if (!type) return 4;
    size_t sz = type->getSize();
    return sz == 0 ? 4 : sz;
}

void IRGenerator::enterScope() {
    localVars.emplace_back();
}

void IRGenerator::exitScope() {
    if (!localVars.empty()) {
        localVars.pop_back();
    }
}

void IRGenerator::declareLocal(const std::string& name, IROperand addr) {
    if (!localVars.empty()) {
        localVars.back()[name] = addr;
    }
}

IROperand IRGenerator::lookupVariable(const std::string& name) {
    for (auto it = localVars.rbegin(); it != localVars.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    // Check if it's a global variable
    if (module->findGlobal(name)) {
        return IROperand::makeGlobal(name, IRType::Ptr);
    }
    return IROperand::makeNone();
}

void IRGenerator::setCurrentBlock(std::shared_ptr<IRBasicBlock> block) {
    currentBlock = std::move(block);
}

void IRGenerator::emitInstruction(IRInstruction inst) {
    if (currentBlock && !currentBlock->hasTerminator()) {
        currentBlock->addInstruction(std::move(inst));
    }
}

IROperand IRGenerator::emitAlloca(IRType type, size_t sizeBytes) {
    int reg = currentFunction->allocateRegister();
    IROperand dest = IROperand::makeReg(reg, IRType::Ptr);
    IRInstruction inst;
    inst.opcode = IROpcode::Alloca;
    inst.type = type;
    inst.dest = dest;
    inst.allocSize = sizeBytes;
    emitInstruction(inst);
    return dest;
}

IROperand IRGenerator::emitLoad(IRType type, IROperand ptr) {
    int reg = currentFunction->allocateRegister();
    IROperand dest = IROperand::makeReg(reg, type);
    IRInstruction inst;
    inst.opcode = IROpcode::Load;
    inst.type = type;
    inst.dest = dest;
    inst.src1 = ptr;
    emitInstruction(inst);
    return dest;
}

void IRGenerator::emitStore(IRType type, IROperand val, IROperand ptr) {
    IRInstruction inst;
    inst.opcode = IROpcode::Store;
    inst.type = type;
    inst.src1 = val;
    inst.dest = ptr;
    emitInstruction(inst);
}

IROperand IRGenerator::emitBinary(IROpcode op, IRType type, IROperand src1, IROperand src2) {
    int reg = currentFunction->allocateRegister();
    IROperand dest = IROperand::makeReg(reg, type);
    IRInstruction inst;
    inst.opcode = op;
    inst.type = type;
    inst.dest = dest;
    inst.src1 = src1;
    inst.src2 = src2;
    emitInstruction(inst);
    return dest;
}

IROperand IRGenerator::emitUnary(IROpcode op, IRType type, IROperand src) {
    int reg = currentFunction->allocateRegister();
    IROperand dest = IROperand::makeReg(reg, type);
    IRInstruction inst;
    inst.opcode = op;
    inst.type = type;
    inst.dest = dest;
    inst.src1 = src;
    emitInstruction(inst);
    return dest;
}

IROperand IRGenerator::emitComparison(IROpcode op, IROperand src1, IROperand src2) {
    int reg = currentFunction->allocateRegister();
    IROperand dest = IROperand::makeReg(reg, IRType::I32);
    IRInstruction inst;
    inst.opcode = op;
    inst.type = IRType::I32;
    inst.dest = dest;
    inst.src1 = src1;
    inst.src2 = src2;
    emitInstruction(inst);
    return dest;
}

std::unique_ptr<IRModule> IRGenerator::generate(Program& program) {
    module = std::make_unique<IRModule>();

    for (const auto& decl : program.getDeclarations()) {
        if (decl->getKind() == ASTKind::VarDecl) {
            emitGlobalVar(static_cast<VarDecl&>(*decl));
        } else if (decl->getKind() == ASTKind::FunctionDecl) {
            emitFunction(static_cast<FunctionDecl&>(*decl));
        }
        // StructDecl only defines types, no IR code emitted
    }

    return std::move(module);
}

void IRGenerator::emitGlobalVar(VarDecl& decl) {
    IRGlobalVar gvar;
    gvar.name = decl.getName();
    gvar.type = toIRType(decl.getType());
    gvar.sizeBytes = getTypeSize(decl.getType());

    if (decl.getInitExpr()) {
        auto val = evaluateConstantExpr(decl.getInitExpr());
        if (val.has_value()) {
            gvar.initValue = *val;
            gvar.hasInit = true;
        }
    }

    module->addGlobalVar(gvar);
}

void IRGenerator::emitFunction(FunctionDecl& decl) {
    if (!decl.isDefinition()) {
        return; // Prototype only
    }

    IRType retType = toIRType(decl.getReturnType());
    std::vector<std::string> pNames;
    std::vector<IRType> pTypes;

    for (const auto& p : decl.getParams()) {
        pNames.push_back(p->getName());
        pTypes.push_back(toIRType(p->getType()));
    }

    currentFunction = std::make_shared<IRFunction>(decl.getName(), retType, pNames, pTypes);
    module->addFunction(currentFunction);

    auto entryBlock = currentFunction->createBlock("entry");
    setCurrentBlock(entryBlock);

    enterScope();

    // Allocate virtual registers for all incoming parameters first (0, 1, ..., N-1)
    std::vector<IROperand> paramVals;
    for (size_t i = 0; i < pNames.size(); ++i) {
        int inReg = currentFunction->allocateRegister();
        paramVals.push_back(IROperand::makeReg(inReg, pTypes[i]));
    }

    // Allocate stack slots for parameters and store incoming arguments
    for (size_t i = 0; i < pNames.size(); ++i) {
        IROperand slot = emitAlloca(pTypes[i], getTypeSize(decl.getParams()[i]->getType()));
        emitStore(pTypes[i], paramVals[i], slot);
        declareLocal(pNames[i], slot);
    }

    // Emit function body
    emitCompoundStmt(*decl.getBody());

    // Terminate block if not already terminated
    if (currentBlock && !currentBlock->hasTerminator()) {
        IRInstruction retInst;
        retInst.opcode = IROpcode::Return;
        if (retType == IRType::Void) {
            retInst.type = IRType::Void;
        } else {
            retInst.type = retType;
            retInst.src1 = IROperand::makeInt(0, retType);
        }
        emitInstruction(retInst);
    }

    exitScope();
    currentFunction = nullptr;
    currentBlock = nullptr;
}

void IRGenerator::emitStatement(Stmt& stmt) {
    if (currentBlock && currentBlock->hasTerminator()) return;

    switch (stmt.getKind()) {
        case ASTKind::CompoundStmt:
            emitCompoundStmt(static_cast<CompoundStmt&>(stmt));
            break;
        case ASTKind::ExprStmt: {
            auto& es = static_cast<ExprStmt&>(stmt);
            if (es.getExpr()) {
                emitExpr(*es.getExpr());
            }
            break;
        }
        case ASTKind::DeclStmt: {
            auto& ds = static_cast<DeclStmt&>(stmt);
            for (const auto& var : ds.getDeclarations()) {
                IRType varType = toIRType(var->getType());
                size_t sz = getTypeSize(var->getType());
                IROperand slot = emitAlloca(varType, sz);
                declareLocal(var->getName(), slot);

                if (var->getInitExpr()) {
                    if (var->getType() && var->getType()->isStruct()) {
                        IROperand srcAddr = emitAddress(*var->getInitExpr());
                        size_t offset = 0;
                        while (offset < sz) {
                            if (offset + 8 <= sz) {
                                IROperand srcPtr = (offset == 0) ? srcAddr : emitBinary(IROpcode::Add, IRType::Ptr, srcAddr, IROperand::makeInt(offset, IRType::I32));
                                IROperand val = emitLoad(IRType::Ptr, srcPtr);
                                IROperand dstPtr = (offset == 0) ? slot : emitBinary(IROpcode::Add, IRType::Ptr, slot, IROperand::makeInt(offset, IRType::I32));
                                emitStore(IRType::Ptr, val, dstPtr);
                                offset += 8;
                            } else if (offset + 4 <= sz) {
                                IROperand srcPtr = (offset == 0) ? srcAddr : emitBinary(IROpcode::Add, IRType::Ptr, srcAddr, IROperand::makeInt(offset, IRType::I32));
                                IROperand val = emitLoad(IRType::I32, srcPtr);
                                IROperand dstPtr = (offset == 0) ? slot : emitBinary(IROpcode::Add, IRType::Ptr, slot, IROperand::makeInt(offset, IRType::I32));
                                emitStore(IRType::I32, val, dstPtr);
                                offset += 4;
                            } else {
                                IROperand srcPtr = (offset == 0) ? srcAddr : emitBinary(IROpcode::Add, IRType::Ptr, srcAddr, IROperand::makeInt(offset, IRType::I32));
                                IROperand val = emitLoad(IRType::I8, srcPtr);
                                IROperand dstPtr = (offset == 0) ? slot : emitBinary(IROpcode::Add, IRType::Ptr, slot, IROperand::makeInt(offset, IRType::I32));
                                emitStore(IRType::I8, val, dstPtr);
                                offset += 1;
                            }
                        }
                    } else {
                        IROperand initVal = emitExpr(*var->getInitExpr());
                        emitStore(varType, initVal, slot);
                    }
                }
            }
            break;
        }
        case ASTKind::IfStmt: {
            auto& is = static_cast<IfStmt&>(stmt);
            IROperand cond = emitExpr(*is.getCond());

            auto thenBlock = currentFunction->createBlock("if.then");
            auto elseBlock = is.getElseBranch() ? currentFunction->createBlock("if.else") : nullptr;
            auto mergeBlock = currentFunction->createBlock("if.end");

            std::string falseLabel = elseBlock ? elseBlock->getName() : mergeBlock->getName();

            IRInstruction brInst;
            brInst.opcode = IROpcode::CondBranch;
            brInst.src1 = cond;
            brInst.targetLabel = thenBlock->getName();
            brInst.falseLabel = falseLabel;
            emitInstruction(brInst);

            // Then branch
            setCurrentBlock(thenBlock);
            emitStatement(*is.getThenBranch());
            if (currentBlock && !currentBlock->hasTerminator()) {
                IRInstruction jmp;
                jmp.opcode = IROpcode::Branch;
                jmp.targetLabel = mergeBlock->getName();
                emitInstruction(jmp);
            }

            // Else branch
            if (elseBlock) {
                setCurrentBlock(elseBlock);
                emitStatement(*is.getElseBranch());
                if (currentBlock && !currentBlock->hasTerminator()) {
                    IRInstruction jmp;
                    jmp.opcode = IROpcode::Branch;
                    jmp.targetLabel = mergeBlock->getName();
                    emitInstruction(jmp);
                }
            }

            setCurrentBlock(mergeBlock);
            break;
        }
        case ASTKind::WhileStmt: {
            auto& ws = static_cast<WhileStmt&>(stmt);
            auto condBlock = currentFunction->createBlock("while.cond");
            auto bodyBlock = currentFunction->createBlock("while.body");
            auto endBlock = currentFunction->createBlock("while.end");

            IRInstruction jmp;
            jmp.opcode = IROpcode::Branch;
            jmp.targetLabel = condBlock->getName();
            emitInstruction(jmp);

            // Cond block
            setCurrentBlock(condBlock);
            IROperand cond = emitExpr(*ws.getCond());
            IRInstruction condBr;
            condBr.opcode = IROpcode::CondBranch;
            condBr.src1 = cond;
            condBr.targetLabel = bodyBlock->getName();
            condBr.falseLabel = endBlock->getName();
            emitInstruction(condBr);

            // Body block
            loopStack.push_back({condBlock, endBlock});
            setCurrentBlock(bodyBlock);
            emitStatement(*ws.getBody());
            if (currentBlock && !currentBlock->hasTerminator()) {
                IRInstruction backJmp;
                backJmp.opcode = IROpcode::Branch;
                backJmp.targetLabel = condBlock->getName();
                emitInstruction(backJmp);
            }
            loopStack.pop_back();

            setCurrentBlock(endBlock);
            break;
        }
        case ASTKind::DoWhileStmt: {
            auto& dws = static_cast<DoWhileStmt&>(stmt);
            auto bodyBlock = currentFunction->createBlock("dowhile.body");
            auto condBlock = currentFunction->createBlock("dowhile.cond");
            auto endBlock = currentFunction->createBlock("dowhile.end");

            IRInstruction jmp;
            jmp.opcode = IROpcode::Branch;
            jmp.targetLabel = bodyBlock->getName();
            emitInstruction(jmp);

            // Body block
            loopStack.push_back({condBlock, endBlock});
            setCurrentBlock(bodyBlock);
            emitStatement(*dws.getBody());
            if (currentBlock && !currentBlock->hasTerminator()) {
                IRInstruction condJmp;
                condJmp.opcode = IROpcode::Branch;
                condJmp.targetLabel = condBlock->getName();
                emitInstruction(condJmp);
            }
            loopStack.pop_back();

            // Cond block
            setCurrentBlock(condBlock);
            IROperand cond = emitExpr(*dws.getCond());
            IRInstruction condBr;
            condBr.opcode = IROpcode::CondBranch;
            condBr.src1 = cond;
            condBr.targetLabel = bodyBlock->getName();
            condBr.falseLabel = endBlock->getName();
            emitInstruction(condBr);

            setCurrentBlock(endBlock);
            break;
        }
        case ASTKind::ForStmt: {
            auto& fs = static_cast<ForStmt&>(stmt);
            enterScope();

            if (fs.getInit()) {
                emitStatement(*fs.getInit());
            }

            auto condBlock = currentFunction->createBlock("for.cond");
            auto bodyBlock = currentFunction->createBlock("for.body");
            auto stepBlock = currentFunction->createBlock("for.step");
            auto endBlock = currentFunction->createBlock("for.end");

            IRInstruction jmp;
            jmp.opcode = IROpcode::Branch;
            jmp.targetLabel = condBlock->getName();
            emitInstruction(jmp);

            // Cond block
            setCurrentBlock(condBlock);
            if (fs.getCond()) {
                IROperand cond = emitExpr(*fs.getCond());
                IRInstruction condBr;
                condBr.opcode = IROpcode::CondBranch;
                condBr.src1 = cond;
                condBr.targetLabel = bodyBlock->getName();
                condBr.falseLabel = endBlock->getName();
                emitInstruction(condBr);
            } else {
                IRInstruction bodyJmp;
                bodyJmp.opcode = IROpcode::Branch;
                bodyJmp.targetLabel = bodyBlock->getName();
                emitInstruction(bodyJmp);
            }

            // Body block
            loopStack.push_back({stepBlock, endBlock});
            setCurrentBlock(bodyBlock);
            emitStatement(*fs.getBody());
            if (currentBlock && !currentBlock->hasTerminator()) {
                IRInstruction stepJmp;
                stepJmp.opcode = IROpcode::Branch;
                stepJmp.targetLabel = stepBlock->getName();
                emitInstruction(stepJmp);
            }
            loopStack.pop_back();

            // Step block
            setCurrentBlock(stepBlock);
            if (fs.getStep()) {
                emitExpr(*fs.getStep());
            }
            if (currentBlock && !currentBlock->hasTerminator()) {
                IRInstruction loopJmp;
                loopJmp.opcode = IROpcode::Branch;
                loopJmp.targetLabel = condBlock->getName();
                emitInstruction(loopJmp);
            }

            exitScope();
            setCurrentBlock(endBlock);
            break;
        }
        case ASTKind::ReturnStmt: {
            auto& rs = static_cast<ReturnStmt&>(stmt);
            IRInstruction retInst;
            retInst.opcode = IROpcode::Return;
            if (rs.getValue()) {
                IROperand val = emitExpr(*rs.getValue());
                retInst.type = val.type;
                retInst.src1 = val;
            } else {
                retInst.type = IRType::Void;
            }
            emitInstruction(retInst);
            break;
        }
        case ASTKind::BreakStmt: {
            if (!loopStack.empty()) {
                IRInstruction brInst;
                brInst.opcode = IROpcode::Branch;
                brInst.targetLabel = loopStack.back().breakTarget->getName();
                emitInstruction(brInst);
            }
            break;
        }
        case ASTKind::ContinueStmt: {
            if (!loopStack.empty()) {
                IRInstruction brInst;
                brInst.opcode = IROpcode::Branch;
                brInst.targetLabel = loopStack.back().continueTarget->getName();
                emitInstruction(brInst);
            }
            break;
        }
        default:
            break;
    }
}

void IRGenerator::emitCompoundStmt(CompoundStmt& stmt) {
    enterScope();
    for (const auto& s : stmt.getStatements()) {
        emitStatement(*s);
    }
    exitScope();
}

IROperand IRGenerator::emitAddress(Expr& expr) {
    switch (expr.getKind()) {
        case ASTKind::VarExpr: {
            auto& ve = static_cast<VarExpr&>(expr);
            return lookupVariable(ve.getName());
        }
        case ASTKind::UnaryExpr: {
            auto& ue = static_cast<UnaryExpr&>(expr);
            if (ue.getOp() == UnaryOp::Deref) {
                // Address of *p is the value of p
                return emitExpr(*ue.getOperand());
            }
            break;
        }
        case ASTKind::SubscriptExpr: {
            auto& se = static_cast<SubscriptExpr&>(expr);
            IROperand baseAddr;
            if (se.getBase()->getEvalType()->isArray()) {
                baseAddr = emitAddress(*se.getBase());
            } else {
                baseAddr = emitExpr(*se.getBase());
            }

            IROperand idx = emitExpr(*se.getIndex());
            size_t elemSize = getTypeSize(se.getEvalType());

            IROperand byteOffset;
            if (idx.isConstant()) {
                byteOffset = IROperand::makeInt(idx.intValue * elemSize, IRType::I32);
            } else {
                IROperand scale = IROperand::makeInt(elemSize, IRType::I32);
                byteOffset = emitBinary(IROpcode::Mul, IRType::I32, idx, scale);
            }

            int reg = currentFunction->allocateRegister();
            IROperand dest = IROperand::makeReg(reg, IRType::Ptr);
            IRInstruction inst;
            inst.opcode = IROpcode::GetElementPtr;
            inst.type = IRType::Ptr;
            inst.dest = dest;
            inst.src1 = baseAddr;
            if (byteOffset.isConstant()) {
                inst.offset = byteOffset.intValue;
            } else {
                return emitBinary(IROpcode::Add, IRType::Ptr, baseAddr, byteOffset);
            }
            emitInstruction(inst);
            return dest;
        }
        case ASTKind::MemberExpr: {
            auto& me = static_cast<MemberExpr&>(expr);
            IROperand basePtr;
            if (me.getIsArrow()) {
                basePtr = emitExpr(*me.getBase());
            } else {
                basePtr = emitAddress(*me.getBase());
            }

            if (me.getMemberOffset() == 0) {
                return basePtr;
            }

            int reg = currentFunction->allocateRegister();
            IROperand dest = IROperand::makeReg(reg, IRType::Ptr);
            IRInstruction inst;
            inst.opcode = IROpcode::GetElementPtr;
            inst.type = IRType::Ptr;
            inst.dest = dest;
            inst.src1 = basePtr;
            inst.offset = me.getMemberOffset();
            emitInstruction(inst);
            return dest;
        }
        default:
            break;
    }
    return IROperand::makeNone();
}

IROperand IRGenerator::emitExpr(Expr& expr) {
    switch (expr.getKind()) {
        case ASTKind::IntegerLiteralExpr: {
            auto& lit = static_cast<IntegerLiteralExpr&>(expr);
            return IROperand::makeInt(lit.getValue(), IRType::I32);
        }
        case ASTKind::CharLiteralExpr: {
            auto& lit = static_cast<CharLiteralExpr&>(expr);
            return IROperand::makeInt(lit.getValue(), IRType::I8);
        }
        case ASTKind::StringLiteralExpr: {
            auto& lit = static_cast<StringLiteralExpr&>(expr);
            std::string label = module->allocateStringLiteral(lit.getValue());
            return IROperand::makeString(label);
        }
        case ASTKind::VarExpr: {
            auto& ve = static_cast<VarExpr&>(expr);
            IROperand addr = lookupVariable(ve.getName());
            if (ve.getEvalType() && ve.getEvalType()->isArray()) {
                // Array decays to pointer to first element (the address itself)
                return addr;
            }
            IRType irType = toIRType(ve.getEvalType());
            return emitLoad(irType, addr);
        }
        case ASTKind::SubscriptExpr: {
            auto& se = static_cast<SubscriptExpr&>(expr);
            IROperand addr = emitAddress(se);
            if (se.getEvalType() && se.getEvalType()->isArray()) {
                return addr;
            }
            IRType irType = toIRType(se.getEvalType());
            return emitLoad(irType, addr);
        }
        case ASTKind::MemberExpr: {
            auto& me = static_cast<MemberExpr&>(expr);
            IROperand addr = emitAddress(me);
            if (me.getEvalType() && me.getEvalType()->isArray()) {
                return addr;
            }
            IRType irType = toIRType(me.getEvalType());
            return emitLoad(irType, addr);
        }
        case ASTKind::AssignExpr: {
            auto& ae = static_cast<AssignExpr&>(expr);
            IROperand addr = emitAddress(*ae.getLhs());

            if (ae.getLhs()->getEvalType() && ae.getLhs()->getEvalType()->isStruct()) {
                IROperand rhsAddr = emitAddress(*ae.getRhs());
                size_t sz = getTypeSize(ae.getLhs()->getEvalType());
                size_t offset = 0;
                while (offset < sz) {
                    if (offset + 8 <= sz) {
                        IROperand srcPtr = (offset == 0) ? rhsAddr : emitBinary(IROpcode::Add, IRType::Ptr, rhsAddr, IROperand::makeInt(offset, IRType::I32));
                        IROperand val = emitLoad(IRType::Ptr, srcPtr);
                        IROperand dstPtr = (offset == 0) ? addr : emitBinary(IROpcode::Add, IRType::Ptr, addr, IROperand::makeInt(offset, IRType::I32));
                        emitStore(IRType::Ptr, val, dstPtr);
                        offset += 8;
                    } else if (offset + 4 <= sz) {
                        IROperand srcPtr = (offset == 0) ? rhsAddr : emitBinary(IROpcode::Add, IRType::Ptr, rhsAddr, IROperand::makeInt(offset, IRType::I32));
                        IROperand val = emitLoad(IRType::I32, srcPtr);
                        IROperand dstPtr = (offset == 0) ? addr : emitBinary(IROpcode::Add, IRType::Ptr, addr, IROperand::makeInt(offset, IRType::I32));
                        emitStore(IRType::I32, val, dstPtr);
                        offset += 4;
                    } else {
                        IROperand srcPtr = (offset == 0) ? rhsAddr : emitBinary(IROpcode::Add, IRType::Ptr, rhsAddr, IROperand::makeInt(offset, IRType::I32));
                        IROperand val = emitLoad(IRType::I8, srcPtr);
                        IROperand dstPtr = (offset == 0) ? addr : emitBinary(IROpcode::Add, IRType::Ptr, addr, IROperand::makeInt(offset, IRType::I32));
                        emitStore(IRType::I8, val, dstPtr);
                        offset += 1;
                    }
                }
                return addr;
            }

            IROperand rhs = emitExpr(*ae.getRhs());
            IRType irType = toIRType(ae.getLhs()->getEvalType());

            if (ae.getOp() == AssignOp::Assign) {
                emitStore(irType, rhs, addr);
                return rhs;
            }

            // Compound assignment: load lhs, perform op, store, return result
            IROperand lhsVal = emitLoad(irType, addr);
            if ((ae.getOp() == AssignOp::AddAssign || ae.getOp() == AssignOp::SubAssign) &&
                ae.getLhs()->getEvalType() && ae.getLhs()->getEvalType()->isPointer()) {
                auto pt = std::static_pointer_cast<PointerType>(ae.getLhs()->getEvalType());
                size_t elemSz = getTypeSize(pt->getPointee());
                if (elemSz > 1) {
                    IROperand scale = IROperand::makeInt(elemSz, IRType::I32);
                    rhs = emitBinary(IROpcode::Mul, IRType::I32, rhs, scale);
                }
            }

            IROpcode binOp = IROpcode::Add;
            switch (ae.getOp()) {
                case AssignOp::AddAssign: binOp = IROpcode::Add; break;
                case AssignOp::SubAssign: binOp = IROpcode::Sub; break;
                case AssignOp::MulAssign: binOp = IROpcode::Mul; break;
                case AssignOp::DivAssign: binOp = IROpcode::Div; break;
                case AssignOp::ModAssign: binOp = IROpcode::Mod; break;
                case AssignOp::AndAssign: binOp = IROpcode::And; break;
                case AssignOp::OrAssign:  binOp = IROpcode::Or;  break;
                case AssignOp::XorAssign: binOp = IROpcode::Xor; break;
                case AssignOp::ShlAssign: binOp = IROpcode::Shl; break;
                case AssignOp::ShrAssign: binOp = IROpcode::Shr; break;
                default: break;
            }

            IROperand res = emitBinary(binOp, irType, lhsVal, rhs);
            emitStore(irType, res, addr);
            return res;
        }
        case ASTKind::UnaryExpr: {
            auto& ue = static_cast<UnaryExpr&>(expr);
            UnaryOp op = ue.getOp();

            if (op == UnaryOp::AddrOf) {
                return emitAddress(*ue.getOperand());
            }

            if (op == UnaryOp::Deref) {
                IROperand ptr = emitExpr(*ue.getOperand());
                IRType pointeeType = toIRType(ue.getEvalType());
                if (ue.getEvalType()->isArray()) {
                    return ptr;
                }
                return emitLoad(pointeeType, ptr);
            }

            if (op == UnaryOp::PreInc || op == UnaryOp::PreDec ||
                op == UnaryOp::PostInc || op == UnaryOp::PostDec) {
                IROperand addr = emitAddress(*ue.getOperand());
                IRType irType = toIRType(ue.getOperand()->getEvalType());
                IROperand oldVal = emitLoad(irType, addr);

                int64_t stepVal = 1;
                if (ue.getOperand()->getEvalType()->isPointer()) {
                    auto pt = std::static_pointer_cast<PointerType>(ue.getOperand()->getEvalType());
                    stepVal = getTypeSize(pt->getPointee());
                }

                IROperand step = IROperand::makeInt(stepVal, irType);
                IROpcode bop = (op == UnaryOp::PreInc || op == UnaryOp::PostInc) ? IROpcode::Add : IROpcode::Sub;
                IROperand newVal = emitBinary(bop, irType, oldVal, step);
                emitStore(irType, newVal, addr);

                return (op == UnaryOp::PreInc || op == UnaryOp::PreDec) ? newVal : oldVal;
            }

            IROperand opnd = emitExpr(*ue.getOperand());
            IRType irType = toIRType(ue.getEvalType());

            switch (op) {
                case UnaryOp::Plus:       return opnd;
                case UnaryOp::Minus:      return emitUnary(IROpcode::Neg, irType, opnd);
                case UnaryOp::BitNot:     return emitUnary(IROpcode::BitNot, irType, opnd);
                case UnaryOp::LogicalNot: return emitUnary(IROpcode::Not, IRType::I32, opnd);
                default: break;
            }
            return opnd;
        }
        case ASTKind::BinaryExpr: {
            auto& be = static_cast<BinaryExpr&>(expr);
            BinaryOp op = be.getOp();

            // Short-circuit Logical AND (&&)
            if (op == BinaryOp::LogicalAnd) {
                IROperand resSlot = emitAlloca(IRType::I32, 4);
                emitStore(IRType::I32, IROperand::makeInt(0, IRType::I32), resSlot);

                auto evalRhs = currentFunction->createBlock("and.rhs");
                auto merge = currentFunction->createBlock("and.end");

                IROperand lhs = emitExpr(*be.getLeft());
                IRInstruction condBr;
                condBr.opcode = IROpcode::CondBranch;
                condBr.src1 = lhs;
                condBr.targetLabel = evalRhs->getName();
                condBr.falseLabel = merge->getName();
                emitInstruction(condBr);

                setCurrentBlock(evalRhs);
                IROperand rhs = emitExpr(*be.getRight());
                IROperand rhsBool = emitComparison(IROpcode::CmpNE, rhs, IROperand::makeInt(0, rhs.type));
                emitStore(IRType::I32, rhsBool, resSlot);
                if (currentBlock && !currentBlock->hasTerminator()) {
                    IRInstruction jmp;
                    jmp.opcode = IROpcode::Branch;
                    jmp.targetLabel = merge->getName();
                    emitInstruction(jmp);
                }

                setCurrentBlock(merge);
                return emitLoad(IRType::I32, resSlot);
            }

            // Short-circuit Logical OR (||)
            if (op == BinaryOp::LogicalOr) {
                IROperand resSlot = emitAlloca(IRType::I32, 4);
                emitStore(IRType::I32, IROperand::makeInt(1, IRType::I32), resSlot);

                auto evalRhs = currentFunction->createBlock("or.rhs");
                auto merge = currentFunction->createBlock("or.end");

                IROperand lhs = emitExpr(*be.getLeft());
                IRInstruction condBr;
                condBr.opcode = IROpcode::CondBranch;
                condBr.src1 = lhs;
                condBr.targetLabel = merge->getName();
                condBr.falseLabel = evalRhs->getName();
                emitInstruction(condBr);

                setCurrentBlock(evalRhs);
                IROperand rhs = emitExpr(*be.getRight());
                IROperand rhsBool = emitComparison(IROpcode::CmpNE, rhs, IROperand::makeInt(0, rhs.type));
                emitStore(IRType::I32, rhsBool, resSlot);
                if (currentBlock && !currentBlock->hasTerminator()) {
                    IRInstruction jmp;
                    jmp.opcode = IROpcode::Branch;
                    jmp.targetLabel = merge->getName();
                    emitInstruction(jmp);
                }

                setCurrentBlock(merge);
                return emitLoad(IRType::I32, resSlot);
            }

            IROperand lhs = emitExpr(*be.getLeft());
            IROperand rhs = emitExpr(*be.getRight());
            TypePtr lType = be.getLeft()->getEvalType();
            TypePtr rType = be.getRight()->getEvalType();

            // Decay array to pointer
            if (lType && lType->isArray()) {
                lType = Type::getPointer(std::static_pointer_cast<ArrayType>(lType)->getElementType());
            }
            if (rType && rType->isArray()) {
                rType = Type::getPointer(std::static_pointer_cast<ArrayType>(rType)->getElementType());
            }

            // Pointer arithmetic
            if (op == BinaryOp::Add) {
                if (lType && lType->isPointer()) {
                    auto pt = std::static_pointer_cast<PointerType>(lType);
                    size_t elemSz = getTypeSize(pt->getPointee());
                    if (elemSz > 1) {
                        IROperand scale = IROperand::makeInt(elemSz, IRType::I32);
                        rhs = emitBinary(IROpcode::Mul, IRType::I32, rhs, scale);
                    }
                    return emitBinary(IROpcode::Add, IRType::Ptr, lhs, rhs);
                } else if (rType && rType->isPointer()) {
                    auto pt = std::static_pointer_cast<PointerType>(rType);
                    size_t elemSz = getTypeSize(pt->getPointee());
                    if (elemSz > 1) {
                        IROperand scale = IROperand::makeInt(elemSz, IRType::I32);
                        lhs = emitBinary(IROpcode::Mul, IRType::I32, lhs, scale);
                    }
                    return emitBinary(IROpcode::Add, IRType::Ptr, rhs, lhs);
                }
            }

            if (op == BinaryOp::Sub) {
                if (lType && lType->isPointer()) {
                    auto pt = std::static_pointer_cast<PointerType>(lType);
                    size_t elemSz = getTypeSize(pt->getPointee());
                    if (rType && rType->isPointer()) {
                        // Pointer distance: (lhs - rhs) / elemSz
                        IROperand diff = emitBinary(IROpcode::Sub, IRType::I32, lhs, rhs);
                        if (elemSz > 1) {
                            IROperand scale = IROperand::makeInt(elemSz, IRType::I32);
                            return emitBinary(IROpcode::Div, IRType::I32, diff, scale);
                        }
                        return diff;
                    } else {
                        // Pointer - int
                        if (elemSz > 1) {
                            IROperand scale = IROperand::makeInt(elemSz, IRType::I32);
                            rhs = emitBinary(IROpcode::Mul, IRType::I32, rhs, scale);
                        }
                        return emitBinary(IROpcode::Sub, IRType::Ptr, lhs, rhs);
                    }
                }
            }

            // General operators
            IRType resType = toIRType(be.getEvalType());

            switch (op) {
                case BinaryOp::Add:          return emitBinary(IROpcode::Add, resType, lhs, rhs);
                case BinaryOp::Sub:          return emitBinary(IROpcode::Sub, resType, lhs, rhs);
                case BinaryOp::Mul:          return emitBinary(IROpcode::Mul, resType, lhs, rhs);
                case BinaryOp::Div:          return emitBinary(IROpcode::Div, resType, lhs, rhs);
                case BinaryOp::Mod:          return emitBinary(IROpcode::Mod, resType, lhs, rhs);
                case BinaryOp::BitAnd:       return emitBinary(IROpcode::And, resType, lhs, rhs);
                case BinaryOp::BitOr:        return emitBinary(IROpcode::Or,  resType, lhs, rhs);
                case BinaryOp::BitXor:       return emitBinary(IROpcode::Xor, resType, lhs, rhs);
                case BinaryOp::ShiftLeft:    return emitBinary(IROpcode::Shl, resType, lhs, rhs);
                case BinaryOp::ShiftRight:   return emitBinary(IROpcode::Shr, resType, lhs, rhs);
                case BinaryOp::Equal:        return emitComparison(IROpcode::CmpEQ, lhs, rhs);
                case BinaryOp::NotEqual:     return emitComparison(IROpcode::CmpNE, lhs, rhs);
                case BinaryOp::Less:         return emitComparison(IROpcode::CmpLT, lhs, rhs);
                case BinaryOp::LessEqual:    return emitComparison(IROpcode::CmpLE, lhs, rhs);
                case BinaryOp::Greater:      return emitComparison(IROpcode::CmpGT, lhs, rhs);
                case BinaryOp::GreaterEqual: return emitComparison(IROpcode::CmpGE, lhs, rhs);
                default: break;
            }
            break;
        }
        case ASTKind::CallExpr: {
            auto& ce = static_cast<CallExpr&>(expr);
            std::vector<IROperand> argOps;
            for (const auto& a : ce.getArgs()) {
                argOps.push_back(emitExpr(*a));
            }

            IRType retType = toIRType(ce.getEvalType());
            IROperand dest = (retType != IRType::Void) ?
                IROperand::makeReg(currentFunction->allocateRegister(), retType) :
                IROperand::makeNone();

            IRInstruction inst;
            inst.opcode = IROpcode::Call;
            inst.type = retType;
            inst.dest = dest;
            inst.targetLabel = ce.getCalleeName();
            inst.args = argOps;
            emitInstruction(inst);
            return dest;
        }
        case ASTKind::CastExpr: {
            auto& ce = static_cast<CastExpr&>(expr);
            IROperand src = emitExpr(*ce.getOperand());
            IRType targetType = toIRType(ce.getTargetType());
            if (targetType == src.type) return src;

            int reg = currentFunction->allocateRegister();
            IROperand dest = IROperand::makeReg(reg, targetType);
            IRInstruction inst;
            inst.opcode = IROpcode::Cast;
            inst.type = targetType;
            inst.dest = dest;
            inst.src1 = src;
            emitInstruction(inst);
            return dest;
        }
        case ASTKind::SizeofExpr: {
            auto& se = static_cast<SizeofExpr&>(expr);
            size_t sz = 0;
            if (se.getQueriedType()) {
                sz = getTypeSize(se.getQueriedType());
            } else if (se.getQueriedExpr()) {
                sz = getTypeSize(se.getQueriedExpr()->getEvalType());
            }
            return IROperand::makeInt(sz, IRType::I32);
        }
        case ASTKind::ConditionalExpr: {
            auto& ce = static_cast<ConditionalExpr&>(expr);
            IRType resType = toIRType(ce.getEvalType());
            IROperand resSlot = IROperand::makeNone();
            if (resType != IRType::Void) {
                resSlot = emitAlloca(resType, getTypeSize(ce.getEvalType()));
            }

            auto trueBlock = currentFunction->createBlock("cond.true");
            auto falseBlock = currentFunction->createBlock("cond.false");
            auto mergeBlock = currentFunction->createBlock("cond.end");

            IROperand cond = emitExpr(*ce.getCond());
            IRInstruction condBr;
            condBr.opcode = IROpcode::CondBranch;
            condBr.src1 = cond;
            condBr.targetLabel = trueBlock->getName();
            condBr.falseLabel = falseBlock->getName();
            emitInstruction(condBr);

            // True block
            setCurrentBlock(trueBlock);
            IROperand trueVal = emitExpr(*ce.getTrueExpr());
            if (!resSlot.isNone()) {
                emitStore(resType, trueVal, resSlot);
            }
            if (currentBlock && !currentBlock->hasTerminator()) {
                IRInstruction jmp;
                jmp.opcode = IROpcode::Branch;
                jmp.targetLabel = mergeBlock->getName();
                emitInstruction(jmp);
            }

            // False block
            setCurrentBlock(falseBlock);
            IROperand falseVal = emitExpr(*ce.getFalseExpr());
            if (!resSlot.isNone()) {
                emitStore(resType, falseVal, resSlot);
            }
            if (currentBlock && !currentBlock->hasTerminator()) {
                IRInstruction jmp;
                jmp.opcode = IROpcode::Branch;
                jmp.targetLabel = mergeBlock->getName();
                emitInstruction(jmp);
            }

            setCurrentBlock(mergeBlock);
            if (!resSlot.isNone()) {
                return emitLoad(resType, resSlot);
            }
            return IROperand::makeNone();
        }
        default:
            break;
    }

    return IROperand::makeNone();
}

std::optional<int64_t> IRGenerator::evaluateConstantExpr(const ExprPtr& expr) {
    if (!expr) return std::nullopt;

    if (expr->getKind() == ASTKind::IntegerLiteralExpr) {
        return std::static_pointer_cast<IntegerLiteralExpr>(expr)->getValue();
    }
    if (expr->getKind() == ASTKind::CharLiteralExpr) {
        return static_cast<int64_t>(std::static_pointer_cast<CharLiteralExpr>(expr)->getValue());
    }
    if (expr->getKind() == ASTKind::SizeofExpr) {
        auto se = std::static_pointer_cast<SizeofExpr>(expr);
        if (se->getQueriedType()) {
            return static_cast<int64_t>(getTypeSize(se->getQueriedType()));
        } else if (se->getQueriedExpr()) {
            TypePtr t = se->getQueriedExpr()->getEvalType();
            if (t) return static_cast<int64_t>(getTypeSize(t));
        }
        return std::nullopt;
    }
    if (expr->getKind() == ASTKind::CastExpr) {
        auto ce = std::static_pointer_cast<CastExpr>(expr);
        auto val = evaluateConstantExpr(ce->getOperand());
        if (!val) return std::nullopt;
        TypePtr target = ce->getTargetType();
        if (target && target->isChar()) return static_cast<int64_t>(static_cast<int8_t>(*val));
        if (target && target->isInt()) return static_cast<int64_t>(static_cast<int32_t>(*val));
        return *val;
    }
    if (expr->getKind() == ASTKind::UnaryExpr) {
        auto ue = std::static_pointer_cast<UnaryExpr>(expr);
        auto val = evaluateConstantExpr(ue->getOperand());
        if (!val) return std::nullopt;
        switch (ue->getOp()) {
            case UnaryOp::Plus: return *val;
            case UnaryOp::Minus: return -(*val);
            case UnaryOp::BitNot: return ~(*val);
            case UnaryOp::LogicalNot: return (*val == 0 ? 1 : 0);
            default: return std::nullopt;
        }
    }
    if (expr->getKind() == ASTKind::BinaryExpr) {
        auto be = std::static_pointer_cast<BinaryExpr>(expr);
        auto l = evaluateConstantExpr(be->getLeft());
        auto r = evaluateConstantExpr(be->getRight());
        if (!l || !r) return std::nullopt;
        switch (be->getOp()) {
            case BinaryOp::Add: return *l + *r;
            case BinaryOp::Sub: return *l - *r;
            case BinaryOp::Mul: return *l * *r;
            case BinaryOp::Div: return *r != 0 ? *l / *r : 0;
            case BinaryOp::Mod: return *r != 0 ? *l % *r : 0;
            case BinaryOp::BitAnd: return *l & *r;
            case BinaryOp::BitOr:  return *l | *r;
            case BinaryOp::BitXor: return *l ^ *r;
            case BinaryOp::ShiftLeft: return *l << *r;
            case BinaryOp::ShiftRight: return *l >> *r;
            case BinaryOp::Equal: return *l == *r ? 1 : 0;
            case BinaryOp::NotEqual: return *l != *r ? 1 : 0;
            case BinaryOp::Less: return *l < *r ? 1 : 0;
            case BinaryOp::LessEqual: return *l <= *r ? 1 : 0;
            case BinaryOp::Greater: return *l > *r ? 1 : 0;
            case BinaryOp::GreaterEqual: return *l >= *r ? 1 : 0;
            case BinaryOp::LogicalAnd: return (*l != 0 && *r != 0) ? 1 : 0;
            case BinaryOp::LogicalOr: return (*l != 0 || *r != 0) ? 1 : 0;
        }
    }
    if (expr->getKind() == ASTKind::ConditionalExpr) {
        auto ce = std::static_pointer_cast<ConditionalExpr>(expr);
        auto c = evaluateConstantExpr(ce->getCond());
        if (!c) return std::nullopt;
        return (*c != 0) ? evaluateConstantExpr(ce->getTrueExpr()) : evaluateConstantExpr(ce->getFalseExpr());
    }
    return std::nullopt;
}
