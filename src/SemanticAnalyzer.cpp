#include "SemanticAnalyzer.h"
#include <iostream>
#include <unordered_set>

SemanticAnalyzer::SemanticAnalyzer(DiagnosticEngine& diag)
    : diag(diag) {
    registerBuiltins();
}

void SemanticAnalyzer::registerBuiltins() {
    auto voidType = Type::getVoid();
    auto intType = Type::getInt();
    auto charType = Type::getChar();
    auto charPtrType = Type::getPointer(charType);

    symbolTable.declareFunction("print_int", Type::getFunction(voidType, {intType}), nullptr);
    symbolTable.declareFunction("print_char", Type::getFunction(voidType, {charType}), nullptr);
    symbolTable.declareFunction("print_str", Type::getFunction(voidType, {charPtrType}), nullptr);
    symbolTable.declareFunction("putchar", Type::getFunction(intType, {intType}), nullptr);
    symbolTable.declareFunction("puts", Type::getFunction(intType, {charPtrType}), nullptr);
    symbolTable.declareFunction("exit", Type::getFunction(voidType, {intType}), nullptr);
}

TypePtr SemanticAnalyzer::resolveType(TypePtr type, const SourceLocation& loc) {
    if (!type) return Type::getInt();

    if (type->isStruct()) {
        auto st = std::static_pointer_cast<StructType>(type);
        const Symbol* sym = symbolTable.lookupStruct(st->getName());
        if (sym && sym->type) {
            return sym->type;
        }
        return type;
    }

    if (type->isPointer()) {
        auto pt = std::static_pointer_cast<PointerType>(type);
        TypePtr resolvedPointee = resolveType(pt->getPointee(), loc);
        return Type::getPointer(resolvedPointee);
    }

    if (type->isArray()) {
        auto at = std::static_pointer_cast<ArrayType>(type);
        TypePtr resolvedElem = resolveType(at->getElementType(), loc);
        return Type::getArray(resolvedElem, at->getNumElements());
    }

    return type;
}

bool SemanticAnalyzer::checkTypeCompatibility(TypePtr target, TypePtr source, const SourceLocation& loc, const std::string& context) {
    if (!target || !source) return false;

    if (target->equals(*source)) return true;

    // Both integers (char <-> int)
    if (target->isInteger() && source->isInteger()) return true;

    // Pointers
    if (target->isPointer() && source->isPointer()) {
        auto tPtr = std::static_pointer_cast<PointerType>(target);
        auto sPtr = std::static_pointer_cast<PointerType>(source);
        if (tPtr->getPointee()->isVoid() || sPtr->getPointee()->isVoid()) {
            return true;
        }
        if (tPtr->getPointee()->equals(*sPtr->getPointee())) {
            return true;
        }
        diag.warning(loc, "assignment from incompatible pointer type (" + source->toString() + " to " + target->toString() + ") in " + context);
        return true;
    }

    // Pointer and integer 0 (NULL)
    if (target->isPointer() && source->isInteger()) {
        return true;
    }

    // Array decaying to pointer
    if (target->isPointer() && source->isArray()) {
        auto tPtr = std::static_pointer_cast<PointerType>(target);
        auto sArr = std::static_pointer_cast<ArrayType>(source);
        if (tPtr->getPointee()->equals(*sArr->getElementType()) || tPtr->getPointee()->isVoid()) {
            return true;
        }
    }

    diag.error(loc, "incompatible types: cannot convert '" + source->toString() + "' to '" + target->toString() + "' in " + context);
    return false;
}

bool SemanticAnalyzer::isAssignableLValue(const Expr& expr) {
    return expr.getIsLValue();
}

bool SemanticAnalyzer::analyze(Program& program) {
    visit(program);
    return !diag.hasErrors();
}

void SemanticAnalyzer::visit(Program& node) {
    for (const auto& decl : node.getDeclarations()) {
        decl->accept(*this);
    }
}

void SemanticAnalyzer::visit(StructDecl& node) {
    auto structType = std::static_pointer_cast<StructType>(node.getStructType());
    std::vector<StructMember> resolvedMembers;
    std::unordered_set<std::string> seenMembers;

    for (const auto& member : structType->getMembers()) {
        if (seenMembers.count(member.name)) {
            diag.error(node.getLocation(), "duplicate member '" + member.name + "' in struct '" + node.getName() + "'");
        }
        seenMembers.insert(member.name);

        TypePtr mType = resolveType(member.type, node.getLocation());
        if (mType->isVoid()) {
            diag.error(node.getLocation(), "member '" + member.name + "' declared void");
        }
        if (mType->isStruct()) {
            auto st = std::static_pointer_cast<StructType>(mType);
            if (!st->isComplete()) {
                diag.error(node.getLocation(), "field '" + member.name + "' has incomplete type 'struct " + st->getName() + "'");
            }
        }
        resolvedMembers.push_back({member.name, mType, 0});
    }
    structType->setMembers(resolvedMembers);

    if (!symbolTable.declareStruct(node.getName(), structType, std::make_shared<StructDecl>(node))) {
        diag.error(node.getLocation(), "redefinition of struct '" + node.getName() + "'");
    }
}

void SemanticAnalyzer::visit(VarDecl& node) {
    TypePtr type = resolveType(node.getType(), node.getLocation());
    node.setType(type);

    if (type->isVoid()) {
        diag.error(node.getLocation(), "variable '" + node.getName() + "' declared void");
        return;
    }

    if (type->isStruct()) {
        auto st = std::static_pointer_cast<StructType>(type);
        if (!st->isComplete()) {
            diag.error(node.getLocation(), "variable '" + node.getName() + "' has incomplete type 'struct " + st->getName() + "'");
        }
    }

    if (node.getInitExpr()) {
        node.getInitExpr()->accept(*this);
        checkTypeCompatibility(type, node.getInitExpr()->getEvalType(), node.getLocation(), "variable initialization");
        if (node.getIsGlobal()) {
            auto val = evaluateConstantExpr(node.getInitExpr());
            if (!val.has_value()) {
                diag.error(node.getLocation(), "initializer element is not a compile-time constant");
            }
        }
    }

    if (!symbolTable.declareVariable(node.getName(), type, std::make_shared<VarDecl>(node))) {
        diag.error(node.getLocation(), "redeclaration of variable '" + node.getName() + "'");
    }
}

void SemanticAnalyzer::visit(ParamDecl& node) {
    TypePtr type = resolveType(node.getType(), node.getLocation());
    if (type->isVoid()) {
        diag.error(node.getLocation(), "parameter '" + node.getName() + "' declared void");
    }
    if (!symbolTable.declareVariable(node.getName(), type, nullptr)) {
        diag.error(node.getLocation(), "duplicate parameter '" + node.getName() + "'");
    }
}

void SemanticAnalyzer::visit(FunctionDecl& node) {
    TypePtr retType = resolveType(node.getReturnType(), node.getLocation());

    std::vector<TypePtr> paramTypes;
    for (const auto& param : node.getParams()) {
        TypePtr pType = resolveType(param->getType(), param->getLocation());
        paramTypes.push_back(pType);
    }

    TypePtr funcType = Type::getFunction(retType, paramTypes);
    if (!symbolTable.declareFunction(node.getName(), funcType, std::make_shared<FunctionDecl>(node))) {
        diag.error(node.getLocation(), "redefinition of or conflicting types for function '" + node.getName() + "'");
    }

    if (node.getBody()) {
        auto prevFn = symbolTable.getCurrentFunction();
        symbolTable.setCurrentFunction(std::make_shared<FunctionDecl>(node));
        symbolTable.enterScope(false, true);

        for (const auto& param : node.getParams()) {
            param->accept(*this);
        }

        // Visit body statements directly without double-entering scope for the outer compound
        for (const auto& stmt : node.getBody()->getStatements()) {
            stmt->accept(*this);
        }

        symbolTable.exitScope();
        symbolTable.setCurrentFunction(prevFn);
    }
}

void SemanticAnalyzer::visit(CompoundStmt& node) {
    symbolTable.enterScope();
    for (const auto& stmt : node.getStatements()) {
        stmt->accept(*this);
    }
    symbolTable.exitScope();
}

void SemanticAnalyzer::visit(ExprStmt& node) {
    if (node.getExpr()) {
        node.getExpr()->accept(*this);
    }
}

void SemanticAnalyzer::visit(IfStmt& node) {
    node.getCond()->accept(*this);
    if (!node.getCond()->getEvalType() || (!node.getCond()->getEvalType()->isScalar() && !node.getCond()->getEvalType()->isArray())) {
        diag.error(node.getCond()->getLocation(), "condition in 'if' statement must be scalar");
    }

    node.getThenBranch()->accept(*this);
    if (node.getElseBranch()) {
        node.getElseBranch()->accept(*this);
    }
}

void SemanticAnalyzer::visit(WhileStmt& node) {
    node.getCond()->accept(*this);
    if (!node.getCond()->getEvalType() || (!node.getCond()->getEvalType()->isScalar() && !node.getCond()->getEvalType()->isArray())) {
        diag.error(node.getCond()->getLocation(), "condition in 'while' statement must be scalar");
    }

    symbolTable.enterScope(/*isLoop=*/true);
    node.getBody()->accept(*this);
    symbolTable.exitScope();
}

void SemanticAnalyzer::visit(DoWhileStmt& node) {
    symbolTable.enterScope(/*isLoop=*/true);
    node.getBody()->accept(*this);
    symbolTable.exitScope();

    node.getCond()->accept(*this);
    if (!node.getCond()->getEvalType() || (!node.getCond()->getEvalType()->isScalar() && !node.getCond()->getEvalType()->isArray())) {
        diag.error(node.getCond()->getLocation(), "condition in 'do-while' statement must be scalar");
    }
}

void SemanticAnalyzer::visit(ForStmt& node) {
    symbolTable.enterScope(/*isLoop=*/false);

    if (node.getInit()) {
        node.getInit()->accept(*this);
    }

    if (node.getCond()) {
        node.getCond()->accept(*this);
        if (!node.getCond()->getEvalType() || (!node.getCond()->getEvalType()->isScalar() && !node.getCond()->getEvalType()->isArray())) {
            diag.error(node.getCond()->getLocation(), "condition in 'for' statement must be scalar");
        }
    }

    if (node.getStep()) {
        node.getStep()->accept(*this);
    }

    symbolTable.enterScope(/*isLoop=*/true);
    node.getBody()->accept(*this);
    symbolTable.exitScope();

    symbolTable.exitScope();
}

void SemanticAnalyzer::visit(ReturnStmt& node) {
    auto curFn = symbolTable.getCurrentFunction();
    if (!curFn) {
        diag.error(node.getLocation(), "'return' statement outside function");
        return;
    }

    TypePtr retType = curFn->getReturnType();

    if (node.getValue()) {
        node.getValue()->accept(*this);
        if (retType->isVoid()) {
            diag.error(node.getLocation(), "void function cannot return a value");
        } else {
            checkTypeCompatibility(retType, node.getValue()->getEvalType(), node.getLocation(), "return statement");
        }
    } else {
        if (!retType->isVoid()) {
            diag.warning(node.getLocation(), "non-void function should return a value");
        }
    }
}

void SemanticAnalyzer::visit(BreakStmt& node) {
    if (!symbolTable.isInLoop()) {
        diag.error(node.getLocation(), "'break' statement not within loop");
    }
}

void SemanticAnalyzer::visit(ContinueStmt& node) {
    if (!symbolTable.isInLoop()) {
        diag.error(node.getLocation(), "'continue' statement not within loop");
    }
}

void SemanticAnalyzer::visit(DeclStmt& node) {
    for (const auto& decl : node.getDeclarations()) {
        decl->accept(*this);
    }
}

void SemanticAnalyzer::visit(IntegerLiteralExpr& node) {
    node.setEvalType(Type::getInt());
    node.setIsLValue(false);
}

void SemanticAnalyzer::visit(CharLiteralExpr& node) {
    node.setEvalType(Type::getChar());
    node.setIsLValue(false);
}

void SemanticAnalyzer::visit(StringLiteralExpr& node) {
    node.setEvalType(Type::getPointer(Type::getChar()));
    node.setIsLValue(false);
}

void SemanticAnalyzer::visit(VarExpr& node) {
    const Symbol* sym = symbolTable.lookupVariable(node.getName());
    if (sym && sym->kind == Symbol::Kind::Variable) {
        node.setEvalType(sym->type);
        node.setIsLValue(!sym->type->isArray());
        node.setDecl(sym->varDecl);
        return;
    }

    const Symbol* fnSym = symbolTable.lookupFunction(node.getName());
    if (fnSym && fnSym->kind == Symbol::Kind::Function) {
        node.setEvalType(fnSym->type);
        node.setIsLValue(false);
        return;
    }

    diag.error(node.getLocation(), "use of undeclared identifier '" + node.getName() + "'");
    node.setEvalType(Type::getInt());
    node.setIsLValue(false);
}

void SemanticAnalyzer::visit(BinaryExpr& node) {
    node.getLeft()->accept(*this);
    node.getRight()->accept(*this);
    node.setIsLValue(false);

    TypePtr lType = node.getLeft()->getEvalType();
    TypePtr rType = node.getRight()->getEvalType();

    if (!lType || !rType) {
        node.setEvalType(Type::getInt());
        return;
    }

    // Array decay to pointer in binary expr
    if (lType->isArray()) {
        lType = Type::getPointer(std::static_pointer_cast<ArrayType>(lType)->getElementType());
    }
    if (rType->isArray()) {
        rType = Type::getPointer(std::static_pointer_cast<ArrayType>(rType)->getElementType());
    }

    BinaryOp op = node.getOp();

    // Addition
    if (op == BinaryOp::Add) {
        if (lType->isInteger() && rType->isInteger()) {
            node.setEvalType(Type::getInt());
            return;
        }
        if (lType->isPointer() && rType->isInteger()) {
            node.setEvalType(lType);
            return;
        }
        if (lType->isInteger() && rType->isPointer()) {
            node.setEvalType(rType);
            return;
        }
        diag.error(node.getLocation(), "invalid operands to binary '+' ('" + lType->toString() + "' and '" + rType->toString() + "')");
        node.setEvalType(Type::getInt());
        return;
    }

    // Subtraction
    if (op == BinaryOp::Sub) {
        if (lType->isInteger() && rType->isInteger()) {
            node.setEvalType(Type::getInt());
            return;
        }
        if (lType->isPointer() && rType->isInteger()) {
            node.setEvalType(lType);
            return;
        }
        if (lType->isPointer() && rType->isPointer()) {
            node.setEvalType(Type::getInt()); // Pointer distance
            return;
        }
        diag.error(node.getLocation(), "invalid operands to binary '-' ('" + lType->toString() + "' and '" + rType->toString() + "')");
        node.setEvalType(Type::getInt());
        return;
    }

    // Multiplication, Division, Modulo
    if (op == BinaryOp::Mul || op == BinaryOp::Div || op == BinaryOp::Mod) {
        if (!lType->isInteger() || !rType->isInteger()) {
            diag.error(node.getLocation(), "operands to arithmetic operator must be integer");
        }
        node.setEvalType(Type::getInt());
        return;
    }

    // Bitwise operators
    if (op == BinaryOp::BitAnd || op == BinaryOp::BitOr || op == BinaryOp::BitXor ||
        op == BinaryOp::ShiftLeft || op == BinaryOp::ShiftRight) {
        if (!lType->isInteger() || !rType->isInteger()) {
            diag.error(node.getLocation(), "operands to bitwise operator must be integer");
        }
        node.setEvalType(Type::getInt());
        return;
    }

    // Relational and Equality
    if (op == BinaryOp::Equal || op == BinaryOp::NotEqual ||
        op == BinaryOp::Less || op == BinaryOp::LessEqual ||
        op == BinaryOp::Greater || op == BinaryOp::GreaterEqual) {
        if (!lType->isScalar() || !rType->isScalar()) {
            diag.error(node.getLocation(), "operands to comparison operator must be scalar");
        }
        node.setEvalType(Type::getInt());
        return;
    }

    // Logical And/Or
    if (op == BinaryOp::LogicalAnd || op == BinaryOp::LogicalOr) {
        if (!lType->isScalar() || !rType->isScalar()) {
            diag.error(node.getLocation(), "operands to logical operator must be scalar");
        }
        node.setEvalType(Type::getInt());
        return;
    }

    node.setEvalType(Type::getInt());
}

void SemanticAnalyzer::visit(UnaryExpr& node) {
    node.getOperand()->accept(*this);
    TypePtr opType = node.getOperand()->getEvalType();

    if (!opType) {
        node.setEvalType(Type::getInt());
        node.setIsLValue(false);
        return;
    }

    UnaryOp op = node.getOp();

    switch (op) {
        case UnaryOp::Plus:
        case UnaryOp::Minus:
        case UnaryOp::BitNot:
            if (!opType->isInteger()) {
                diag.error(node.getLocation(), "operand to unary arithmetic/bitwise operator must be integer");
            }
            node.setEvalType(Type::getInt());
            node.setIsLValue(false);
            break;

        case UnaryOp::LogicalNot:
            if (!opType->isScalar() && !opType->isArray()) {
                diag.error(node.getLocation(), "operand to '!' must be scalar");
            }
            node.setEvalType(Type::getInt());
            node.setIsLValue(false);
            break;

        case UnaryOp::PreInc:
        case UnaryOp::PreDec:
        case UnaryOp::PostInc:
        case UnaryOp::PostDec:
            if (!node.getOperand()->getIsLValue()) {
                diag.error(node.getLocation(), "lvalue required as increment/decrement operand");
            }
            if (!opType->isScalar()) {
                diag.error(node.getLocation(), "cannot increment/decrement non-scalar type");
            }
            node.setEvalType(opType);
            node.setIsLValue(op == UnaryOp::PreInc || op == UnaryOp::PreDec);
            break;

        case UnaryOp::AddrOf:
            if (!node.getOperand()->getIsLValue() && !opType->isArray()) {
                diag.error(node.getLocation(), "cannot take address of non-lvalue");
            }
            node.setEvalType(Type::getPointer(opType));
            node.setIsLValue(false);
            break;

        case UnaryOp::Deref: {
            TypePtr effective = opType;
            if (effective->isArray()) {
                effective = Type::getPointer(std::static_pointer_cast<ArrayType>(effective)->getElementType());
            }
            if (!effective->isPointer()) {
                diag.error(node.getLocation(), "cannot dereference non-pointer type '" + opType->toString() + "'");
                node.setEvalType(Type::getInt());
                node.setIsLValue(false);
            } else {
                auto pt = std::static_pointer_cast<PointerType>(effective);
                if (pt->getPointee()->isVoid()) {
                    diag.error(node.getLocation(), "cannot dereference 'void*' pointer");
                }
                node.setEvalType(pt->getPointee());
                node.setIsLValue(true);
            }
            break;
        }
    }
}

void SemanticAnalyzer::visit(AssignExpr& node) {
    node.getLhs()->accept(*this);
    node.getRhs()->accept(*this);

    TypePtr lType = node.getLhs()->getEvalType();
    TypePtr rType = node.getRhs()->getEvalType();

    if (!node.getLhs()->getIsLValue()) {
        diag.error(node.getLocation(), "lvalue required as left operand of assignment");
    }

    if (lType && rType) {
        if (lType->isPointer()) {
            if (node.getOp() == AssignOp::Assign) {
                checkTypeCompatibility(lType, rType, node.getLocation(), "assignment");
            } else if (node.getOp() == AssignOp::AddAssign || node.getOp() == AssignOp::SubAssign) {
                if (!rType->isInteger()) {
                    diag.error(node.getLocation(), "cannot add or subtract non-integer from pointer");
                }
            } else {
                diag.error(node.getLocation(), "invalid operands to binary expression ('" + lType->toString() + "' and '" + rType->toString() + "')");
            }
        } else {
            checkTypeCompatibility(lType, rType, node.getLocation(), "assignment");
        }
    }

    node.setEvalType(lType ? lType : Type::getInt());
    node.setIsLValue(false);
}

void SemanticAnalyzer::visit(CallExpr& node) {
    for (const auto& arg : node.getArgs()) {
        arg->accept(*this);
    }

    const Symbol* sym = symbolTable.lookupFunction(node.getCalleeName());
    if (!sym || sym->kind != Symbol::Kind::Function) {
        diag.error(node.getLocation(), "call to undeclared function '" + node.getCalleeName() + "'");
        node.setEvalType(Type::getInt());
        node.setIsLValue(false);
        return;
    }

    auto fnType = std::static_pointer_cast<FunctionType>(sym->type);
    node.setEvalType(fnType->getReturnType());
    node.setIsLValue(false);
    node.setDecl(sym->funcDecl);

    const auto& params = fnType->getParamTypes();
    if (node.getArgs().size() != params.size()) {
        diag.error(node.getLocation(), "function '" + node.getCalleeName() + "' expects " +
                   std::to_string(params.size()) + " arguments, but got " + std::to_string(node.getArgs().size()));
        return;
    }

    for (size_t i = 0; i < params.size(); ++i) {
        checkTypeCompatibility(params[i], node.getArgs()[i]->getEvalType(), node.getArgs()[i]->getLocation(), "function argument");
    }
}

void SemanticAnalyzer::visit(SubscriptExpr& node) {
    node.getBase()->accept(*this);
    node.getIndex()->accept(*this);

    TypePtr baseType = node.getBase()->getEvalType();
    TypePtr indexType = node.getIndex()->getEvalType();

    if (!baseType || !indexType) {
        node.setEvalType(Type::getInt());
        node.setIsLValue(false);
        return;
    }

    if (!indexType->isInteger()) {
        diag.error(node.getIndex()->getLocation(), "array subscript is not an integer");
    }

    if (baseType->isArray()) {
        auto at = std::static_pointer_cast<ArrayType>(baseType);
        node.setEvalType(at->getElementType());
        node.setIsLValue(true);
    } else if (baseType->isPointer()) {
        auto pt = std::static_pointer_cast<PointerType>(baseType);
        node.setEvalType(pt->getPointee());
        node.setIsLValue(true);
    } else {
        diag.error(node.getLocation(), "subscripted value is not an array or pointer ('" + baseType->toString() + "')");
        node.setEvalType(Type::getInt());
        node.setIsLValue(false);
    }
}

void SemanticAnalyzer::visit(MemberExpr& node) {
    node.getBase()->accept(*this);
    TypePtr baseType = node.getBase()->getEvalType();

    if (!baseType) {
        node.setEvalType(Type::getInt());
        node.setIsLValue(false);
        return;
    }

    std::shared_ptr<StructType> st = nullptr;

    if (node.getIsArrow()) {
        if (!baseType->isPointer()) {
            diag.error(node.getLocation(), "base of '->' must be a pointer to struct");
            node.setEvalType(Type::getInt());
            return;
        }
        auto pt = std::static_pointer_cast<PointerType>(baseType);
        if (!pt->getPointee()->isStruct()) {
            diag.error(node.getLocation(), "base of '->' does not point to a struct");
            node.setEvalType(Type::getInt());
            return;
        }
        st = std::static_pointer_cast<StructType>(resolveType(pt->getPointee(), node.getLocation()));
    } else {
        if (!baseType->isStruct()) {
            diag.error(node.getLocation(), "base of '.' must be a struct");
            node.setEvalType(Type::getInt());
            return;
        }
        st = std::static_pointer_cast<StructType>(resolveType(baseType, node.getLocation()));
    }

    const StructMember* member = st->findMember(node.getMemberName());
    if (!member) {
        diag.error(node.getLocation(), "struct '" + st->getName() + "' has no member named '" + node.getMemberName() + "'");
        node.setEvalType(Type::getInt());
        node.setIsLValue(false);
        return;
    }

    node.setMemberOffset(member->offset);
    node.setEvalType(member->type);
    node.setIsLValue(true);
}

void SemanticAnalyzer::visit(CastExpr& node) {
    node.getOperand()->accept(*this);
    TypePtr target = resolveType(node.getTargetType(), node.getLocation());

    if (!target->isScalar() && !target->isVoid()) {
        diag.error(node.getLocation(), "cannot cast to non-scalar type '" + target->toString() + "'");
    }

    node.setEvalType(target);
    node.setIsLValue(false);
}

void SemanticAnalyzer::visit(SizeofExpr& node) {
    node.setEvalType(Type::getInt());
    node.setIsLValue(false);

    if (node.getQueriedType()) {
        TypePtr t = resolveType(node.getQueriedType(), node.getLocation());
        if (t->isVoid()) {
            diag.error(node.getLocation(), "invalid application of 'sizeof' to void type");
        }
        node.setQueriedType(t);
    } else if (node.getQueriedExpr()) {
        node.getQueriedExpr()->accept(*this);
    }
}

void SemanticAnalyzer::visit(ConditionalExpr& node) {
    node.getCond()->accept(*this);
    if (!node.getCond()->getEvalType() || (!node.getCond()->getEvalType()->isScalar() && !node.getCond()->getEvalType()->isArray())) {
        diag.error(node.getCond()->getLocation(), "condition in '?:' must be scalar");
    }

    node.getTrueExpr()->accept(*this);
    node.getFalseExpr()->accept(*this);
    node.setIsLValue(false);

    TypePtr tType = node.getTrueExpr()->getEvalType();
    TypePtr fType = node.getFalseExpr()->getEvalType();

    if (!tType || !fType) {
        node.setEvalType(Type::getInt());
        return;
    }

    if (tType->isPointer() || fType->isPointer()) {
        if (tType->isPointer()) node.setEvalType(tType);
        else node.setEvalType(fType);
    } else {
        node.setEvalType(Type::getInt());
    }
}

std::optional<int64_t> SemanticAnalyzer::evaluateConstantExpr(const ExprPtr& expr) {
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
            TypePtr t = resolveType(se->getQueriedType(), se->getLocation());
            return static_cast<int64_t>(t->getSize());
        } else if (se->getQueriedExpr()) {
            TypePtr t = se->getQueriedExpr()->getEvalType();
            if (t) return static_cast<int64_t>(t->getSize());
        }
        return std::nullopt;
    }
    if (expr->getKind() == ASTKind::CastExpr) {
        auto ce = std::static_pointer_cast<CastExpr>(expr);
        auto val = evaluateConstantExpr(ce->getOperand());
        if (!val) return std::nullopt;
        TypePtr target = resolveType(ce->getTargetType(), ce->getLocation());
        if (target->isChar()) return static_cast<int64_t>(static_cast<int8_t>(*val));
        if (target->isInt()) return static_cast<int64_t>(static_cast<int32_t>(*val));
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
