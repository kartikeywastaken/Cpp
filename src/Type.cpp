#include "Type.h"
#include <algorithm>

static TypePtr voidType = std::make_shared<PrimitiveType>(TypeKind::Void);
static TypePtr charType = std::make_shared<PrimitiveType>(TypeKind::Char);
static TypePtr intType = std::make_shared<PrimitiveType>(TypeKind::Int);

TypePtr Type::getVoid() { return voidType; }
TypePtr Type::getChar() { return charType; }
TypePtr Type::getInt()  { return intType; }

TypePtr Type::getPointer(TypePtr pointee) {
    return std::make_shared<PointerType>(std::move(pointee));
}

TypePtr Type::getArray(TypePtr elementType, size_t numElements) {
    return std::make_shared<ArrayType>(std::move(elementType), numElements);
}

TypePtr Type::getStruct(const std::string& name, const std::vector<StructMember>& members) {
    return std::make_shared<StructType>(name, members);
}

TypePtr Type::getFunction(TypePtr returnType, const std::vector<TypePtr>& paramTypes) {
    return std::make_shared<FunctionType>(std::move(returnType), paramTypes);
}

bool Type::equals(const Type& other) const {
    return kind == other.kind;
}

bool PointerType::equals(const Type& other) const {
    if (other.getKind() != TypeKind::Pointer) return false;
    const auto& otherPtr = static_cast<const PointerType&>(other);
    return pointee->equals(*otherPtr.pointee);
}

bool ArrayType::equals(const Type& other) const {
    if (other.getKind() != TypeKind::Array) return false;
    const auto& otherArr = static_cast<const ArrayType&>(other);
    return numElements == otherArr.numElements && elementType->equals(*otherArr.elementType);
}

const StructMember* StructType::findMember(const std::string& memberName) const {
    for (const auto& member : members) {
        if (member.name == memberName) {
            return &member;
        }
    }
    return nullptr;
}

void StructType::setMembers(const std::vector<StructMember>& newMembers) {
    members = newMembers;
    size_t currentOffset = 0;
    size_t maxAlign = 1;

    for (auto& m : members) {
        size_t align = m.type ? m.type->getAlignment() : 1;
        if (align == 0) align = 1;
        if (align > maxAlign) maxAlign = align;

        // Pad to alignment boundary
        if (currentOffset % align != 0) {
            currentOffset += align - (currentOffset % align);
        }
        m.offset = currentOffset;
        currentOffset += m.type ? m.type->getSize() : 0;
    }

    if (maxAlign > 0 && currentOffset % maxAlign != 0) {
        currentOffset += maxAlign - (currentOffset % maxAlign);
    }
    structAlignment = maxAlign;
    totalSize = currentOffset;
    complete = true;
}

bool StructType::equals(const Type& other) const {
    if (other.getKind() != TypeKind::Struct) return false;
    const auto& otherStruct = static_cast<const StructType&>(other);
    return name == otherStruct.name;
}

std::string FunctionType::toString() const {
    std::string str = returnType->toString() + " (";
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (i > 0) str += ", ";
        str += paramTypes[i]->toString();
    }
    str += ")";
    return str;
}

bool FunctionType::equals(const Type& other) const {
    if (other.getKind() != TypeKind::Function) return false;
    const auto& otherFn = static_cast<const FunctionType&>(other);
    if (!returnType->equals(*otherFn.returnType)) return false;
    if (paramTypes.size() != otherFn.paramTypes.size()) return false;
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (!paramTypes[i]->equals(*otherFn.paramTypes[i])) return false;
    }
    return true;
}
