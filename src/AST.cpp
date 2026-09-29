#include "AST.h"

std::string_view binaryOpToString(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add:          return "+";
        case BinaryOp::Sub:          return "-";
        case BinaryOp::Mul:          return "*";
        case BinaryOp::Div:          return "/";
        case BinaryOp::Mod:          return "%";
        case BinaryOp::Equal:        return "==";
        case BinaryOp::NotEqual:     return "!=";
        case BinaryOp::Less:         return "<";
        case BinaryOp::LessEqual:    return "<=";
        case BinaryOp::Greater:      return ">";
        case BinaryOp::GreaterEqual: return ">=";
        case BinaryOp::LogicalAnd:   return "&&";
        case BinaryOp::LogicalOr:    return "||";
        case BinaryOp::BitAnd:       return "&";
        case BinaryOp::BitOr:        return "|";
        case BinaryOp::BitXor:       return "^";
        case BinaryOp::ShiftLeft:    return "<<";
        case BinaryOp::ShiftRight:   return ">>";
    }
    return "?";
}

std::string_view unaryOpToString(UnaryOp op) {
    switch (op) {
        case UnaryOp::Plus:       return "+";
        case UnaryOp::Minus:      return "-";
        case UnaryOp::LogicalNot: return "!";
        case UnaryOp::BitNot:     return "~";
        case UnaryOp::Deref:      return "*";
        case UnaryOp::AddrOf:     return "&";
        case UnaryOp::PreInc:     return "++(pre)";
        case UnaryOp::PreDec:     return "--(pre)";
        case UnaryOp::PostInc:    return "(post)++";
        case UnaryOp::PostDec:    return "(post)--";
    }
    return "?";
}

std::string_view assignOpToString(AssignOp op) {
    switch (op) {
        case AssignOp::Assign:     return "=";
        case AssignOp::AddAssign:  return "+=";
        case AssignOp::SubAssign:  return "-=";
        case AssignOp::MulAssign:  return "*=";
        case AssignOp::DivAssign:  return "/=";
        case AssignOp::ModAssign:  return "%=";
        case AssignOp::AndAssign:  return "&=";
        case AssignOp::OrAssign:   return "|=";
        case AssignOp::XorAssign:  return "^=";
        case AssignOp::ShlAssign:  return "<<=";
        case AssignOp::ShrAssign:  return ">>=";
    }
    return "=";
}

static void printIndent(std::ostream& out, int indent) {
    for (int i = 0; i < indent; ++i) out << "  ";
}

// Accept implementations
void Program::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VarDecl::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ParamDecl::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FunctionDecl::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void StructDecl::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CompoundStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ExprStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IfStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void WhileStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void DoWhileStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ForStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ReturnStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void BreakStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ContinueStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void DeclStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IntegerLiteralExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CharLiteralExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void StringLiteralExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VarExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void BinaryExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void UnaryExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void AssignExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CallExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void SubscriptExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void MemberExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CastExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void SizeofExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ConditionalExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }

// Dump implementations
void Program::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "Program\n";
    for (const auto& decl : declarations) {
        decl->dump(out, indent + 1);
    }
}

void VarDecl::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "VarDecl: " << name << " : " << (type ? type->toString() : "<unknown>");
    if (isGlobal) out << " [global]";
    out << "\n";
    if (initExpr) {
        initExpr->dump(out, indent + 1);
    }
}

void ParamDecl::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "ParamDecl: " << name << " : " << (type ? type->toString() : "<unknown>") << "\n";
}

void FunctionDecl::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "FunctionDecl: " << name << " : " << (returnType ? returnType->toString() : "<unknown>") << "\n";
    for (const auto& param : params) {
        param->dump(out, indent + 1);
    }
    if (body) {
        body->dump(out, indent + 1);
    }
}

void StructDecl::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "StructDecl: struct " << name << "\n";
}

void CompoundStmt::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "CompoundStmt\n";
    for (const auto& stmt : statements) {
        stmt->dump(out, indent + 1);
    }
}

void ExprStmt::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "ExprStmt\n";
    if (expr) {
        expr->dump(out, indent + 1);
    }
}

void IfStmt::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "IfStmt\n";
    printIndent(out, indent + 1);
    out << "Condition:\n";
    cond->dump(out, indent + 2);
    printIndent(out, indent + 1);
    out << "Then:\n";
    thenBranch->dump(out, indent + 2);
    if (elseBranch) {
        printIndent(out, indent + 1);
        out << "Else:\n";
        elseBranch->dump(out, indent + 2);
    }
}

void WhileStmt::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "WhileStmt\n";
    printIndent(out, indent + 1);
    out << "Condition:\n";
    cond->dump(out, indent + 2);
    printIndent(out, indent + 1);
    out << "Body:\n";
    body->dump(out, indent + 2);
}

void DoWhileStmt::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "DoWhileStmt\n";
    printIndent(out, indent + 1);
    out << "Body:\n";
    body->dump(out, indent + 2);
    printIndent(out, indent + 1);
    out << "Condition:\n";
    cond->dump(out, indent + 2);
}

void ForStmt::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "ForStmt\n";
    if (init) {
        printIndent(out, indent + 1);
        out << "Init:\n";
        init->dump(out, indent + 2);
    }
    if (cond) {
        printIndent(out, indent + 1);
        out << "Cond:\n";
        cond->dump(out, indent + 2);
    }
    if (step) {
        printIndent(out, indent + 1);
        out << "Step:\n";
        step->dump(out, indent + 2);
    }
    printIndent(out, indent + 1);
    out << "Body:\n";
    body->dump(out, indent + 2);
}

void ReturnStmt::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "ReturnStmt\n";
    if (value) {
        value->dump(out, indent + 1);
    }
}

void BreakStmt::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "BreakStmt\n";
}

void ContinueStmt::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "ContinueStmt\n";
}

void DeclStmt::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "DeclStmt\n";
    for (const auto& decl : declarations) {
        decl->dump(out, indent + 1);
    }
}

void IntegerLiteralExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "IntegerLiteral: " << value;
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
}

void CharLiteralExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "CharLiteral: '" << value << "'";
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
}

void StringLiteralExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "StringLiteral: \"" << value << "\"";
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
}

void VarExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "VarExpr: " << name;
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
}

void BinaryExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "BinaryExpr: " << binaryOpToString(op);
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
    left->dump(out, indent + 1);
    right->dump(out, indent + 1);
}

void UnaryExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "UnaryExpr: " << unaryOpToString(op);
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
    operand->dump(out, indent + 1);
}

void AssignExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "AssignExpr: " << assignOpToString(op);
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
    lhs->dump(out, indent + 1);
    rhs->dump(out, indent + 1);
}

void CallExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "CallExpr: " << calleeName << "()";
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
    for (const auto& arg : args) {
        arg->dump(out, indent + 1);
    }
}

void SubscriptExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "SubscriptExpr []";
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
    base->dump(out, indent + 1);
    index->dump(out, indent + 1);
}

void MemberExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "MemberExpr " << (isArrow ? "->" : ".") << memberName;
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
    base->dump(out, indent + 1);
}

void CastExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "CastExpr (" << (targetType ? targetType->toString() : "<null>") << ")";
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
    operand->dump(out, indent + 1);
}

void SizeofExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "SizeofExpr";
    if (queriedType) out << " (type: " << queriedType->toString() << ")";
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
    if (queriedExpr) {
        queriedExpr->dump(out, indent + 1);
    }
}

void ConditionalExpr::dump(std::ostream& out, int indent) const {
    printIndent(out, indent);
    out << "ConditionalExpr (?:)";
    if (getEvalType()) out << " <type: " << getEvalType()->toString() << ">";
    out << "\n";
    printIndent(out, indent + 1);
    out << "Condition:\n";
    cond->dump(out, indent + 2);
    printIndent(out, indent + 1);
    out << "TrueBranch:\n";
    trueExpr->dump(out, indent + 2);
    printIndent(out, indent + 1);
    out << "FalseBranch:\n";
    falseExpr->dump(out, indent + 2);
}
