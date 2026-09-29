#ifndef IR_INTERPRETER_H
#define IR_INTERPRETER_H

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <iostream>
#include <sstream>
#include "IR.h"

class IRInterpreter {
public:
    explicit IRInterpreter(const IRModule& module, std::ostream& out = std::cout);

    int64_t run(const std::string& entryFunc = "main", const std::vector<int64_t>& args = {});

    std::string getCapturedOutput() const { return capturedOutput.str(); }
    void enableCapture(bool enable) { captureMode = enable; }

private:
    struct StackFrame {
        std::string funcName;
        std::unordered_map<int, int64_t> registers;
        int64_t returnValue = 0;
        bool hasReturned = false;
    };

    int64_t evalOperand(const IROperand& op, StackFrame& frame);
    void setOperand(const IROperand& op, int64_t value, StackFrame& frame);

    int64_t executeFunction(const IRFunction& fn, const std::vector<int64_t>& args);
    bool executeBuiltin(const std::string& name, const std::vector<int64_t>& args, int64_t& outResult);

    // Memory access
    uint64_t allocateMemory(size_t size);
    void writeMemory(uint64_t addr, int64_t value, IRType type);
    int64_t readMemory(uint64_t addr, IRType type);
    const char* getMemoryPointer(uint64_t addr);

    void initGlobalsAndStrings();

    const IRModule& module;
    std::ostream& outStream;
    std::ostringstream capturedOutput;
    bool captureMode = false;

    std::vector<uint8_t> memoryPool;
    uint64_t nextAddr = 0x1000;
    std::unordered_map<std::string, uint64_t> globalAddresses;
    std::unordered_map<std::string, uint64_t> stringAddresses;

    bool exited = false;
    int64_t exitCode = 0;
};

#endif // IR_INTERPRETER_H
