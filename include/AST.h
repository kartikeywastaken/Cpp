#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "Token.h"
#include "Type.h"
#include "Diagnostic.h"

enum class ASTKind {
    // Declarations
    VarDecl,
    ParamDecl,
    FunctionDecl,
    StructDecl,
    Program,

    // Statements
    CompoundStmt,
    ExprStmt,
    IfStmt,
    WhileStmt,
    DoWhileStmt,
    ForStmt,
    ReturnStmt,
    BreakStmt,
    ContinueStmt,
    DeclStmt,

    // Expressions
    IntegerLiteralExpr,
    CharLiteralExpr,
    StringLiteralExpr,
    VarExpr,
    BinaryExpr,
    UnaryExpr,
    AssignExpr,
    CallExpr,
    SubscriptExpr,
    MemberExpr,
    CastExpr,
    SizeofExpr,
    ConditionalExpr
};

enum class BinaryOp {
    Add, Sub, Mul, Div, Mod,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    LogicalAnd, LogicalOr,
    BitAnd, BitOr, BitXor,
    ShiftLeft, ShiftRight
};

enum class UnaryOp {
    Plus, Minus, LogicalNot, BitNot,
    Deref, AddrOf,
    PreInc, PreDec, PostInc, PostDec
};

enum class AssignOp {
    Assign,
    AddAssign, SubAssign, MulAssign, DivAssign, ModAssign,
    AndAssign, OrAssign, XorAssign,
    ShlAssign, ShrAssign
};

std::string_view binaryOpToString(BinaryOp op);
std::string_view unaryOpToString(UnaryOp op);
std::string_view assignOpToString(AssignOp op);

class ASTVisitor;

class ASTNode {
public:
    ASTNode(ASTKind kind, SourceLocation loc) : kind(kind), location(loc) {}
    virtual ~ASTNode() = default;

    ASTKind getKind() const { return kind; }
    const SourceLocation& getLocation() const { return location; }

    virtual void dump(std::ostream& out, int indent = 0) const = 0;
    virtual void accept(ASTVisitor& visitor) = 0;

private:
    ASTKind kind;
    SourceLocation location;
};

using ASTNodePtr = std::shared_ptr<ASTNode>;

// Base Expression
class Expr : public ASTNode {
public:
    Expr(ASTKind kind, SourceLocation loc) : ASTNode(kind, loc) {}

    TypePtr getEvalType() const { return evalType; }
    void setEvalType(TypePtr t) { evalType = std::move(t); }

    bool getIsLValue() const { return isLValue; }
    void setIsLValue(bool lval) { isLValue = lval; }

private:
    TypePtr evalType;
    bool isLValue = false;
};

using ExprPtr = std::shared_ptr<Expr>;

// Base Statement
class Stmt : public ASTNode {
public:
    Stmt(ASTKind kind, SourceLocation loc) : ASTNode(kind, loc) {}
};

using StmtPtr = std::shared_ptr<Stmt>;

// Base Declaration
class Decl : public ASTNode {
public:
    Decl(ASTKind kind, SourceLocation loc) : ASTNode(kind, loc) {}
};

using DeclPtr = std::shared_ptr<Decl>;

// Expressions
class IntegerLiteralExpr : public Expr {
public:
    IntegerLiteralExpr(SourceLocation loc, int64_t value)
        : Expr(ASTKind::IntegerLiteralExpr, loc), value(value) {}

    int64_t getValue() const { return value; }
    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    int64_t value;
};

class CharLiteralExpr : public Expr {
public:
    CharLiteralExpr(SourceLocation loc, char value)
        : Expr(ASTKind::CharLiteralExpr, loc), value(value) {}

    char getValue() const { return value; }
    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    char value;
};

class StringLiteralExpr : public Expr {
public:
    StringLiteralExpr(SourceLocation loc, std::string value)
        : Expr(ASTKind::StringLiteralExpr, loc), value(std::move(value)) {}

    const std::string& getValue() const { return value; }
    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    std::string value;
};

class VarDecl;

class VarExpr : public Expr {
public:
    VarExpr(SourceLocation loc, std::string name)
        : Expr(ASTKind::VarExpr, loc), name(std::move(name)) {}

    const std::string& getName() const { return name; }
    std::shared_ptr<VarDecl> getDecl() const { return decl.lock(); }
    void setDecl(std::shared_ptr<VarDecl> d) { decl = d; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    std::string name;
    std::weak_ptr<VarDecl> decl;
};

class BinaryExpr : public Expr {
public:
    BinaryExpr(SourceLocation loc, BinaryOp op, ExprPtr left, ExprPtr right)
        : Expr(ASTKind::BinaryExpr, loc), op(op), left(std::move(left)), right(std::move(right)) {}

    BinaryOp getOp() const { return op; }
    ExprPtr getLeft() const { return left; }
    ExprPtr getRight() const { return right; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    BinaryOp op;
    ExprPtr left;
    ExprPtr right;
};

class UnaryExpr : public Expr {
public:
    UnaryExpr(SourceLocation loc, UnaryOp op, ExprPtr operand, bool isPrefix = true)
        : Expr(ASTKind::UnaryExpr, loc), op(op), operand(std::move(operand)), isPrefix(isPrefix) {}

    UnaryOp getOp() const { return op; }
    ExprPtr getOperand() const { return operand; }
    bool getIsPrefix() const { return isPrefix; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    UnaryOp op;
    ExprPtr operand;
    bool isPrefix;
};

class AssignExpr : public Expr {
public:
    AssignExpr(SourceLocation loc, AssignOp op, ExprPtr lhs, ExprPtr rhs)
        : Expr(ASTKind::AssignExpr, loc), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    AssignOp getOp() const { return op; }
    ExprPtr getLhs() const { return lhs; }
    ExprPtr getRhs() const { return rhs; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    AssignOp op;
    ExprPtr lhs;
    ExprPtr rhs;
};

class FunctionDecl;

class CallExpr : public Expr {
public:
    CallExpr(SourceLocation loc, std::string calleeName, std::vector<ExprPtr> args)
        : Expr(ASTKind::CallExpr, loc), calleeName(std::move(calleeName)), args(std::move(args)) {}

    const std::string& getCalleeName() const { return calleeName; }
    const std::vector<ExprPtr>& getArgs() const { return args; }
    std::shared_ptr<FunctionDecl> getDecl() const { return decl.lock(); }
    void setDecl(std::shared_ptr<FunctionDecl> d) { decl = d; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    std::string calleeName;
    std::vector<ExprPtr> args;
    std::weak_ptr<FunctionDecl> decl;
};

class SubscriptExpr : public Expr {
public:
    SubscriptExpr(SourceLocation loc, ExprPtr base, ExprPtr index)
        : Expr(ASTKind::SubscriptExpr, loc), base(std::move(base)), index(std::move(index)) {}

    ExprPtr getBase() const { return base; }
    ExprPtr getIndex() const { return index; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    ExprPtr base;
    ExprPtr index;
};

class MemberExpr : public Expr {
public:
    MemberExpr(SourceLocation loc, ExprPtr base, std::string memberName, bool isArrow)
        : Expr(ASTKind::MemberExpr, loc), base(std::move(base)), memberName(std::move(memberName)), isArrow(isArrow) {}

    ExprPtr getBase() const { return base; }
    const std::string& getMemberName() const { return memberName; }
    bool getIsArrow() const { return isArrow; }
    size_t getMemberOffset() const { return memberOffset; }
    void setMemberOffset(size_t off) { memberOffset = off; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    ExprPtr base;
    std::string memberName;
    bool isArrow;
    size_t memberOffset = 0;
};

class CastExpr : public Expr {
public:
    CastExpr(SourceLocation loc, TypePtr targetType, ExprPtr operand)
        : Expr(ASTKind::CastExpr, loc), targetType(std::move(targetType)), operand(std::move(operand)) {}

    TypePtr getTargetType() const { return targetType; }
    ExprPtr getOperand() const { return operand; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    TypePtr targetType;
    ExprPtr operand;
};

class SizeofExpr : public Expr {
public:
    SizeofExpr(SourceLocation loc, TypePtr queriedType)
        : Expr(ASTKind::SizeofExpr, loc), queriedType(std::move(queriedType)) {}
    SizeofExpr(SourceLocation loc, ExprPtr queriedExpr)
        : Expr(ASTKind::SizeofExpr, loc), queriedExpr(std::move(queriedExpr)) {}

    TypePtr getQueriedType() const { return queriedType; }
    void setQueriedType(TypePtr t) { queriedType = std::move(t); }
    ExprPtr getQueriedExpr() const { return queriedExpr; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    TypePtr queriedType;
    ExprPtr queriedExpr;
};

class ConditionalExpr : public Expr {
public:
    ConditionalExpr(SourceLocation loc, ExprPtr cond, ExprPtr trueExpr, ExprPtr falseExpr)
        : Expr(ASTKind::ConditionalExpr, loc), cond(std::move(cond)), trueExpr(std::move(trueExpr)), falseExpr(std::move(falseExpr)) {}

    ExprPtr getCond() const { return cond; }
    ExprPtr getTrueExpr() const { return trueExpr; }
    ExprPtr getFalseExpr() const { return falseExpr; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    ExprPtr cond;
    ExprPtr trueExpr;
    ExprPtr falseExpr;
};

// Statements
class CompoundStmt : public Stmt {
public:
    CompoundStmt(SourceLocation loc, std::vector<StmtPtr> stmts)
        : Stmt(ASTKind::CompoundStmt, loc), statements(std::move(stmts)) {}

    const std::vector<StmtPtr>& getStatements() const { return statements; }
    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    std::vector<StmtPtr> statements;
};

class ExprStmt : public Stmt {
public:
    ExprStmt(SourceLocation loc, ExprPtr expr)
        : Stmt(ASTKind::ExprStmt, loc), expr(std::move(expr)) {}

    ExprPtr getExpr() const { return expr; }
    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    ExprPtr expr;
};

class IfStmt : public Stmt {
public:
    IfStmt(SourceLocation loc, ExprPtr cond, StmtPtr thenBranch, StmtPtr elseBranch = nullptr)
        : Stmt(ASTKind::IfStmt, loc), cond(std::move(cond)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}

    ExprPtr getCond() const { return cond; }
    StmtPtr getThenBranch() const { return thenBranch; }
    StmtPtr getElseBranch() const { return elseBranch; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    ExprPtr cond;
    StmtPtr thenBranch;
    StmtPtr elseBranch;
};

class WhileStmt : public Stmt {
public:
    WhileStmt(SourceLocation loc, ExprPtr cond, StmtPtr body)
        : Stmt(ASTKind::WhileStmt, loc), cond(std::move(cond)), body(std::move(body)) {}

    ExprPtr getCond() const { return cond; }
    StmtPtr getBody() const { return body; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    ExprPtr cond;
    StmtPtr body;
};

class DoWhileStmt : public Stmt {
public:
    DoWhileStmt(SourceLocation loc, StmtPtr body, ExprPtr cond)
        : Stmt(ASTKind::DoWhileStmt, loc), body(std::move(body)), cond(std::move(cond)) {}

    StmtPtr getBody() const { return body; }
    ExprPtr getCond() const { return cond; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    StmtPtr body;
    ExprPtr cond;
};

class ForStmt : public Stmt {
public:
    ForStmt(SourceLocation loc, StmtPtr init, ExprPtr cond, ExprPtr step, StmtPtr body)
        : Stmt(ASTKind::ForStmt, loc), init(std::move(init)), cond(std::move(cond)), step(std::move(step)), body(std::move(body)) {}

    StmtPtr getInit() const { return init; }
    ExprPtr getCond() const { return cond; }
    ExprPtr getStep() const { return step; }
    StmtPtr getBody() const { return body; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    StmtPtr init;
    ExprPtr cond;
    ExprPtr step;
    StmtPtr body;
};

class ReturnStmt : public Stmt {
public:
    ReturnStmt(SourceLocation loc, ExprPtr value = nullptr)
        : Stmt(ASTKind::ReturnStmt, loc), value(std::move(value)) {}

    ExprPtr getValue() const { return value; }
    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    ExprPtr value;
};

class BreakStmt : public Stmt {
public:
    explicit BreakStmt(SourceLocation loc) : Stmt(ASTKind::BreakStmt, loc) {}
    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;
};

class ContinueStmt : public Stmt {
public:
    explicit ContinueStmt(SourceLocation loc) : Stmt(ASTKind::ContinueStmt, loc) {}
    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;
};

class DeclStmt : public Stmt {
public:
    DeclStmt(SourceLocation loc, std::vector<std::shared_ptr<VarDecl>> decls)
        : Stmt(ASTKind::DeclStmt, loc), declarations(std::move(decls)) {}

    const std::vector<std::shared_ptr<VarDecl>>& getDeclarations() const { return declarations; }
    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    std::vector<std::shared_ptr<VarDecl>> declarations;
};

// Declarations
class VarDecl : public Decl {
public:
    VarDecl(SourceLocation loc, std::string name, TypePtr type, ExprPtr initExpr = nullptr, bool isGlobal = false)
        : Decl(ASTKind::VarDecl, loc), name(std::move(name)), type(std::move(type)), initExpr(std::move(initExpr)), isGlobal(isGlobal) {}

    const std::string& getName() const { return name; }
    TypePtr getType() const { return type; }
    void setType(TypePtr t) { type = std::move(t); }
    ExprPtr getInitExpr() const { return initExpr; }
    bool getIsGlobal() const { return isGlobal; }
    void setIsGlobal(bool g) { isGlobal = g; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    std::string name;
    TypePtr type;
    ExprPtr initExpr;
    bool isGlobal = false;
};

class ParamDecl : public Decl {
public:
    ParamDecl(SourceLocation loc, std::string name, TypePtr type)
        : Decl(ASTKind::ParamDecl, loc), name(std::move(name)), type(std::move(type)) {}

    const std::string& getName() const { return name; }
    TypePtr getType() const { return type; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    std::string name;
    TypePtr type;
};

class FunctionDecl : public Decl {
public:
    FunctionDecl(SourceLocation loc, std::string name, TypePtr returnType,
                 std::vector<std::shared_ptr<ParamDecl>> params,
                 std::shared_ptr<CompoundStmt> body = nullptr)
        : Decl(ASTKind::FunctionDecl, loc), name(std::move(name)), returnType(std::move(returnType)),
          params(std::move(params)), body(std::move(body)) {}

    const std::string& getName() const { return name; }
    TypePtr getReturnType() const { return returnType; }
    const std::vector<std::shared_ptr<ParamDecl>>& getParams() const { return params; }
    std::shared_ptr<CompoundStmt> getBody() const { return body; }
    void setBody(std::shared_ptr<CompoundStmt> b) { body = std::move(b); }
    bool isDefinition() const { return body != nullptr; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    std::string name;
    TypePtr returnType;
    std::vector<std::shared_ptr<ParamDecl>> params;
    std::shared_ptr<CompoundStmt> body;
};

class StructDecl : public Decl {
public:
    StructDecl(SourceLocation loc, std::string name, TypePtr structType)
        : Decl(ASTKind::StructDecl, loc), name(std::move(name)), structType(std::move(structType)) {}

    const std::string& getName() const { return name; }
    TypePtr getStructType() const { return structType; }

    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    std::string name;
    TypePtr structType;
};

class Program : public ASTNode {
public:
    Program(SourceLocation loc, std::vector<DeclPtr> decls)
        : ASTNode(ASTKind::Program, loc), declarations(std::move(decls)) {}

    const std::vector<DeclPtr>& getDeclarations() const { return declarations; }
    void dump(std::ostream& out, int indent = 0) const override;
    void accept(ASTVisitor& visitor) override;

private:
    std::vector<DeclPtr> declarations;
};

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(Program& node) = 0;

    // Declarations
    virtual void visit(VarDecl& node) = 0;
    virtual void visit(ParamDecl& node) = 0;
    virtual void visit(FunctionDecl& node) = 0;
    virtual void visit(StructDecl& node) = 0;

    // Statements
    virtual void visit(CompoundStmt& node) = 0;
    virtual void visit(ExprStmt& node) = 0;
    virtual void visit(IfStmt& node) = 0;
    virtual void visit(WhileStmt& node) = 0;
    virtual void visit(DoWhileStmt& node) = 0;
    virtual void visit(ForStmt& node) = 0;
    virtual void visit(ReturnStmt& node) = 0;
    virtual void visit(BreakStmt& node) = 0;
    virtual void visit(ContinueStmt& node) = 0;
    virtual void visit(DeclStmt& node) = 0;

    // Expressions
    virtual void visit(IntegerLiteralExpr& node) = 0;
    virtual void visit(CharLiteralExpr& node) = 0;
    virtual void visit(StringLiteralExpr& node) = 0;
    virtual void visit(VarExpr& node) = 0;
    virtual void visit(BinaryExpr& node) = 0;
    virtual void visit(UnaryExpr& node) = 0;
    virtual void visit(AssignExpr& node) = 0;
    virtual void visit(CallExpr& node) = 0;
    virtual void visit(SubscriptExpr& node) = 0;
    virtual void visit(MemberExpr& node) = 0;
    virtual void visit(CastExpr& node) = 0;
    virtual void visit(SizeofExpr& node) = 0;
    virtual void visit(ConditionalExpr& node) = 0;
};

#endif // AST_H
