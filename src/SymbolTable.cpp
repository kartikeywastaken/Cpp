#include "SymbolTable.h"
#include "AST.h"

bool Scope::declareVariable(const std::string& name, TypePtr type, std::shared_ptr<VarDecl> decl) {
    if (symbols.find(name) != symbols.end()) {
        return false;
    }
    symbols[name] = {Symbol::Kind::Variable, name, std::move(type), std::move(decl), nullptr, nullptr};
    return true;
}

bool Scope::declareFunction(const std::string& name, TypePtr type, std::shared_ptr<FunctionDecl> decl) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        // Function re-declaration or prototype definition
        if (it->second.kind != Symbol::Kind::Function) return false;
        // Check signature equivalence
        if (!it->second.type->equals(*type)) return false;
        // Check if both are definitions (redefinition error)
        if (it->second.funcDecl && it->second.funcDecl->isDefinition() && decl && decl->isDefinition()) {
            return false;
        }
        // If incoming has body, update definition
        if (decl && decl->isDefinition()) {
            it->second.funcDecl = std::move(decl);
        }
        return true;
    }
    symbols[name] = {Symbol::Kind::Function, name, std::move(type), nullptr, std::move(decl), nullptr};
    return true;
}

bool Scope::declareStruct(const std::string& name, TypePtr type, std::shared_ptr<StructDecl> decl) {
    auto it = structSymbols.find(name);
    if (it != structSymbols.end()) {
        auto existing = std::static_pointer_cast<StructType>(it->second.type);
        auto incoming = std::static_pointer_cast<StructType>(type);
        if (existing->isComplete() && incoming->isComplete()) {
            return false; // Redefinition of struct
        }
        if (!existing->isComplete() && incoming->isComplete()) {
            existing->setMembers(incoming->getMembers());
        }
        return true;
    }
    structSymbols[name] = {Symbol::Kind::Struct, name, std::move(type), nullptr, nullptr, std::move(decl)};
    return true;
}

const Symbol* Scope::lookupLocal(const std::string& name) const {
    auto it = symbols.find(name);
    if (it != symbols.end()) return &it->second;
    return nullptr;
}

const Symbol* Scope::lookup(const std::string& name) const {
    const Symbol* sym = lookupLocal(name);
    if (sym) return sym;
    if (parent) return parent->lookup(name);
    return nullptr;
}

const Symbol* Scope::lookupStructLocal(const std::string& name) const {
    auto it = structSymbols.find(name);
    if (it != structSymbols.end()) return &it->second;
    return nullptr;
}

const Symbol* Scope::lookupStruct(const std::string& name) const {
    const Symbol* sym = lookupStructLocal(name);
    if (sym) return sym;
    if (parent) return parent->lookupStruct(name);
    return nullptr;
}

SymbolTable::SymbolTable() {
    globalScope = std::make_shared<Scope>();
    currentScope = globalScope;
}

void SymbolTable::enterScope(bool isLoop, bool isFunction) {
    currentScope = std::make_shared<Scope>(currentScope, isLoop, isFunction);
}

void SymbolTable::exitScope() {
    if (currentScope->getParent()) {
        currentScope = currentScope->getParent();
    }
}

bool SymbolTable::declareVariable(const std::string& name, TypePtr type, std::shared_ptr<VarDecl> decl) {
    return currentScope->declareVariable(name, std::move(type), std::move(decl));
}

bool SymbolTable::declareFunction(const std::string& name, TypePtr type, std::shared_ptr<FunctionDecl> decl) {
    return globalScope->declareFunction(name, std::move(type), std::move(decl));
}

bool SymbolTable::declareStruct(const std::string& name, TypePtr type, std::shared_ptr<StructDecl> decl) {
    return globalScope->declareStruct(name, std::move(type), std::move(decl));
}

const Symbol* SymbolTable::lookupVariable(const std::string& name) const {
    return currentScope->lookup(name);
}

const Symbol* SymbolTable::lookupFunction(const std::string& name) const {
    return globalScope->lookup(name);
}

const Symbol* SymbolTable::lookupStruct(const std::string& name) const {
    return globalScope->lookupStruct(name);
}

bool SymbolTable::isInLoop() const {
    std::shared_ptr<Scope> s = currentScope;
    while (s) {
        if (s->isLoop()) return true;
        s = s->getParent();
    }
    return false;
}
