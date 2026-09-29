#ifndef TYPE_H
#define TYPE_H

#include <string>
#include <vector>
#include <memory>
#include <optional>

enum class TypeKind {
    Void,
    Char,
    Int,
    Pointer,
    Array,
    Struct,
    Function
};

class Type;
using TypePtr = std::shared_ptr<Type>;

struct StructMember {
    std::string name;
    TypePtr type;
    size_t offset = 0;
};

class Type : public std::enable_shared_from_this<Type> {
public:
    explicit Type(TypeKind kind) : kind(kind) {}
    virtual ~Type() = default;

    TypeKind getKind() const { return kind; }

    bool isVoid() const { return kind == TypeKind::Void; }
    bool isChar() const { return kind == TypeKind::Char; }
    bool isInt() const { return kind == TypeKind::Int; }
    bool isInteger() const { return isChar() || isInt(); }
    bool isPointer() const { return kind == TypeKind::Pointer; }
    bool isArray() const { return kind == TypeKind::Array; }
    bool isStruct() const { return kind == TypeKind::Struct; }
    bool isFunction() const { return kind == TypeKind::Function; }
    bool isScalar() const { return isInteger() || isPointer(); }

    virtual size_t getSize() const = 0;
    virtual size_t getAlignment() const = 0;
    virtual std::string toString() const = 0;
    virtual bool equals(const Type& other) const;

    // Factory methods
    static TypePtr getVoid();
    static TypePtr getChar();
    static TypePtr getInt();
    static TypePtr getPointer(TypePtr pointee);
    static TypePtr getArray(TypePtr elementType, size_t numElements);
    static TypePtr getStruct(const std::string& name, const std::vector<StructMember>& members = {});
    static TypePtr getFunction(TypePtr returnType, const std::vector<TypePtr>& paramTypes);

private:
    TypeKind kind;
};

class PrimitiveType : public Type {
public:
    explicit PrimitiveType(TypeKind kind) : Type(kind) {}

    size_t getSize() const override {
        switch (getKind()) {
            case TypeKind::Void: return 0;
            case TypeKind::Char: return 1;
            case TypeKind::Int:  return 4;
            default: return 0;
        }
    }

    size_t getAlignment() const override {
        switch (getKind()) {
            case TypeKind::Void: return 1;
            case TypeKind::Char: return 1;
            case TypeKind::Int:  return 4;
            default: return 1;
        }
    }

    std::string toString() const override {
        switch (getKind()) {
            case TypeKind::Void: return "void";
            case TypeKind::Char: return "char";
            case TypeKind::Int:  return "int";
            default: return "unknown";
        }
    }
};

class PointerType : public Type {
public:
    explicit PointerType(TypePtr pointee)
        : Type(TypeKind::Pointer), pointee(std::move(pointee)) {}

    TypePtr getPointee() const { return pointee; }

    size_t getSize() const override { return 8; }
    size_t getAlignment() const override { return 8; }

    std::string toString() const override {
        return pointee->toString() + "*";
    }

    bool equals(const Type& other) const override;

private:
    TypePtr pointee;
};

class ArrayType : public Type {
public:
    ArrayType(TypePtr elementType, size_t numElements)
        : Type(TypeKind::Array), elementType(std::move(elementType)), numElements(numElements) {}

    TypePtr getElementType() const { return elementType; }
    size_t getNumElements() const { return numElements; }

    size_t getSize() const override {
        return numElements * elementType->getSize();
    }

    size_t getAlignment() const override {
        return elementType->getAlignment();
    }

    std::string toString() const override {
        return elementType->toString() + "[" + std::to_string(numElements) + "]";
    }

    bool equals(const Type& other) const override;

private:
    TypePtr elementType;
    size_t numElements;
};

class StructType : public Type {
public:
    explicit StructType(std::string name, const std::vector<StructMember>& members = {})
        : Type(TypeKind::Struct), name(std::move(name)) {
        if (!members.empty()) {
            setMembers(members);
        }
    }

    const std::string& getName() const { return name; }
    const std::vector<StructMember>& getMembers() const { return members; }
    bool isComplete() const { return complete; }

    const StructMember* findMember(const std::string& memberName) const;
    void setMembers(const std::vector<StructMember>& newMembers);

    size_t getSize() const override { return totalSize; }
    size_t getAlignment() const override { return structAlignment; }

    std::string toString() const override {
        return "struct " + name;
    }

    bool equals(const Type& other) const override;

private:
    std::string name;
    std::vector<StructMember> members;
    size_t totalSize = 0;
    size_t structAlignment = 1;
    bool complete = false;
};

class FunctionType : public Type {
public:
    FunctionType(TypePtr returnType, std::vector<TypePtr> paramTypes)
        : Type(TypeKind::Function), returnType(std::move(returnType)), paramTypes(std::move(paramTypes)) {}

    TypePtr getReturnType() const { return returnType; }
    const std::vector<TypePtr>& getParamTypes() const { return paramTypes; }

    size_t getSize() const override { return 0; }
    size_t getAlignment() const override { return 1; }

    std::string toString() const override;
    bool equals(const Type& other) const override;

private:
    TypePtr returnType;
    std::vector<TypePtr> paramTypes;
};

#endif // TYPE_H
