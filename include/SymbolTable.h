#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "Type.h"

class VarDecl;
class FunctionDecl;
class StructDecl;

struct Symbol {
    enum class Kind {
        Variable,
        Function,
        Struct
    };

    Kind kind;
    std::string name;
    TypePtr type;
    std::shared_ptr<VarDecl> varDecl = nullptr;
    std::shared_ptr<FunctionDecl> funcDecl = nullptr;
    std::shared_ptr<StructDecl> structDecl = nullptr;
};

class Scope {
public:
    explicit Scope(std::shared_ptr<Scope> parent = nullptr, bool isLoop = false, bool isFunction = false)
        : parent(parent), isLoopScope(isLoop), isFunctionScope(isFunction) {}

    std::shared_ptr<Scope> getParent() const { return parent; }
    bool isLoop() const { return isLoopScope; }
    bool isFunction() const { return isFunctionScope; }

    bool declareVariable(const std::string& name, TypePtr type, std::shared_ptr<VarDecl> decl);
    bool declareFunction(const std::string& name, TypePtr type, std::shared_ptr<FunctionDecl> decl);
    bool declareStruct(const std::string& name, TypePtr type, std::shared_ptr<StructDecl> decl);

    const Symbol* lookupLocal(const std::string& name) const;
    const Symbol* lookup(const std::string& name) const;

    const Symbol* lookupStructLocal(const std::string& name) const;
    const Symbol* lookupStruct(const std::string& name) const;

private:
    std::shared_ptr<Scope> parent;
    bool isLoopScope = false;
    bool isFunctionScope = false;
    std::unordered_map<std::string, Symbol> symbols;
    std::unordered_map<std::string, Symbol> structSymbols;
};

class SymbolTable {
public:
    SymbolTable();

    void enterScope(bool isLoop = false, bool isFunction = false);
    void exitScope();

    bool declareVariable(const std::string& name, TypePtr type, std::shared_ptr<VarDecl> decl);
    bool declareFunction(const std::string& name, TypePtr type, std::shared_ptr<FunctionDecl> decl);
    bool declareStruct(const std::string& name, TypePtr type, std::shared_ptr<StructDecl> decl);

    const Symbol* lookupVariable(const std::string& name) const;
    const Symbol* lookupFunction(const std::string& name) const;
    const Symbol* lookupStruct(const std::string& name) const;

    bool isInLoop() const;
    std::shared_ptr<Scope> getCurrentScope() const { return currentScope; }

    void setCurrentFunction(std::shared_ptr<FunctionDecl> fn) { currentFunction = fn; }
    std::shared_ptr<FunctionDecl> getCurrentFunction() const { return currentFunction; }

private:
    std::shared_ptr<Scope> globalScope;
    std::shared_ptr<Scope> currentScope;
    std::shared_ptr<FunctionDecl> currentFunction = nullptr;
};

#endif // SYMBOL_TABLE_H
