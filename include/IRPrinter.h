#ifndef IR_PRINTER_H
#define IR_PRINTER_H

#include <iostream>
#include "IR.h"

class IRPrinter {
public:
    explicit IRPrinter(std::ostream& out = std::cout) : out(out) {}

    void print(const IRModule& module);
    void print(const IRFunction& fn);
    void print(const IRBasicBlock& block);
    void print(const IRInstruction& inst);

private:
    std::ostream& out;
};

#endif // IR_PRINTER_H
