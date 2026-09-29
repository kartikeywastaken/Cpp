#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include <memory>
#include "AST.h"
#include "SymbolTable.h"
#include "Diagnostic.h"

class SemanticAnalyzer : public ASTVisitor {
public:
    SemanticAnalyzer(DiagnosticEngine& diag);

    bool analyze(Program& program);

    // Visitor overrides
    void visit(Program& node) override;
    void visit(VarDecl& node) override;
    void visit(ParamDecl& node) override;
    void visit(FunctionDecl& node) override;
    void visit(StructDecl& node) override;

    void visit(CompoundStmt& node) override;
    void visit(ExprStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(DoWhileStmt& node) override;
    void visit(ForStmt& node) override;
    void visit(ReturnStmt& node) override;
    void visit(BreakStmt& node) override;
    void visit(ContinueStmt& node) override;
    void visit(DeclStmt& node) override;

    void visit(IntegerLiteralExpr& node) override;
    void visit(CharLiteralExpr& node) override;
    void visit(StringLiteralExpr& node) override;
    void visit(VarExpr& node) override;
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(AssignExpr& node) override;
    void visit(CallExpr& node) override;
    void visit(SubscriptExpr& node) override;
    void visit(MemberExpr& node) override;
    void visit(CastExpr& node) override;
    void visit(SizeofExpr& node) override;
    void visit(ConditionalExpr& node) override;

    SymbolTable& getSymbolTable() { return symbolTable; }
    std::optional<int64_t> evaluateConstantExpr(const ExprPtr& expr);

private:
    TypePtr resolveType(TypePtr type, const SourceLocation& loc);
    bool checkTypeCompatibility(TypePtr target, TypePtr source, const SourceLocation& loc, const std::string& context);
    bool isAssignableLValue(const Expr& expr);
    void registerBuiltins();

    DiagnosticEngine& diag;
    SymbolTable symbolTable;
};

#endif // SEMANTIC_ANALYZER_H
