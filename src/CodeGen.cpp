#include "CodeGen.h"
#include "CodeGenARM64.h"
#include "CodeGenX86.h"
#include <stdexcept>

TargetArch getHostTargetArch() {
#if defined(__aarch64__) || defined(__arm64__)
    return TargetArch::ARM64;
#elif defined(__x86_64__) || defined(_M_X64)
    return TargetArch::X86_64;
#else
    return TargetArch::ARM64;
#endif
}

TargetOS getHostTargetOS() {
#if defined(__APPLE__)
    return TargetOS::macOS;
#else
    return TargetOS::Linux;
#endif
}

std::string targetArchToString(TargetArch arch) {
    switch (arch) {
        case TargetArch::ARM64:  return "arm64";
        case TargetArch::X86_64: return "x86_64";
    }
    return "unknown";
}

std::string targetOSToString(TargetOS os) {
    switch (os) {
        case TargetOS::macOS: return "macos";
        case TargetOS::Linux: return "linux";
    }
    return "unknown";
}

std::unique_ptr<CodeGenerator> createCodeGenerator(TargetArch arch, TargetOS os) {
    switch (arch) {
        case TargetArch::ARM64:
            return std::make_unique<ARM64CodeGenerator>(os);
        case TargetArch::X86_64:
            return std::make_unique<X86_64CodeGenerator>(os);
    }
    throw std::runtime_error("Unsupported target architecture");
}
