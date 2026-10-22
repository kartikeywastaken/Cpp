#ifndef CODEGEN_H
#define CODEGEN_H

#include "IR.h"
#include <ostream>
#include <memory>
#include <string>

enum class TargetArch {
    ARM64,
    X86_64
};

enum class TargetOS {
    macOS,
    Linux
};

TargetArch getHostTargetArch();
TargetOS getHostTargetOS();
std::string targetArchToString(TargetArch arch);
std::string targetOSToString(TargetOS os);

class CodeGenerator {
public:
    virtual ~CodeGenerator() = default;
    virtual void generate(const IRModule& module, std::ostream& out) = 0;
};

std::unique_ptr<CodeGenerator> createCodeGenerator(TargetArch arch = getHostTargetArch(),
                                                   TargetOS os = getHostTargetOS());

#endif // CODEGEN_H
