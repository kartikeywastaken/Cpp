#ifndef CODEGEN_ARM64_H
#define CODEGEN_ARM64_H

#include "CodeGen.h"
#include <map>
#include <vector>

class ARM64CodeGenerator : public CodeGenerator {
public:
    explicit ARM64CodeGenerator(TargetOS os = getHostTargetOS());
    void generate(const IRModule& module, std::ostream& out) override;

private:
    TargetOS targetOS;

    struct FunctionFrame {
        std::map<int, size_t> allocaOffsets;
        size_t totalAllocaBytes = 0;
        size_t totalRegBytes = 0;
        size_t outgoingArgBytes = 0;
        size_t frameSize = 0;
        int maxRegNum = -1;

        size_t getRegOffset(int regNum) const {
            return outgoingArgBytes + 8 * regNum;
        }

        size_t getAllocaOffset(int regNum) const {
            auto it = allocaOffsets.find(regNum);
            if (it != allocaOffsets.end()) {
                return outgoingArgBytes + totalRegBytes + it->second;
            }
            return 0;
        }
    };

    std::string mangledName(const std::string& name) const;
    std::string stringLabel(const std::string& label) const;
    std::string localLabel(const std::string& name) const;
    std::string escapeString(const std::string& str) const;

    void emitMovImm32(std::ostream& out, const std::string& reg, int32_t val);
    void emitMovImm64(std::ostream& out, const std::string& reg, int64_t val);
    void emitLoadReg(std::ostream& out, const std::string& reg, int regNum, const FunctionFrame& frame);
    void emitStoreReg(std::ostream& out, const std::string& reg, int regNum, const FunctionFrame& frame);
    void loadOperand(std::ostream& out, const IROperand& op, const std::string& destReg, const FunctionFrame& frame);

    void emitGlobals(const IRModule& module, std::ostream& out);
    void emitStrings(const IRModule& module, std::ostream& out);
    void emitFunction(const IRFunction& fn, const IRModule& module, std::ostream& out);
    void emitBuiltins(const IRModule& module, std::ostream& out);

    FunctionFrame computeStackLayout(const IRFunction& fn);
};

#endif // CODEGEN_ARM64_H
