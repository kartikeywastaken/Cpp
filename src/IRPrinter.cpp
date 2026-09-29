#include "IRPrinter.h"

void IRPrinter::print(const IRModule& module) {
    out << "; --- IR Module ---\n\n";

    // Print string constants
    if (!module.getStrings().empty()) {
        out << "; String Literals:\n";
        for (const auto& s : module.getStrings()) {
            out << "@" << s.label << " = constant c\"";
            for (char c : s.value) {
                if (c == '\n') out << "\\n";
                else if (c == '\t') out << "\\t";
                else if (c == '\r') out << "\\r";
                else if (c == '\\') out << "\\\\";
                else if (c == '\"') out << "\\\"";
                else out << c;
            }
            out << "\\0\"\n";
        }
        out << "\n";
    }

    // Print global variables
    if (!module.getGlobals().empty()) {
        out << "; Global Variables:\n";
        for (const auto& g : module.getGlobals()) {
            out << "@" << g.name << " = global " << irTypeToString(g.type);
            if (g.hasInit) {
                out << " " << g.initValue;
            } else {
                out << " zeroinitializer";
            }
            out << ", size=" << g.sizeBytes << "\n";
        }
        out << "\n";
    }

    // Print functions
    for (const auto& fn : module.getFunctions()) {
        print(*fn);
        out << "\n";
    }
}

void IRPrinter::print(const IRFunction& fn) {
    out << "define " << irTypeToString(fn.getReturnType()) << " @" << fn.getName() << "(";
    const auto& pNames = fn.getParamNames();
    const auto& pTypes = fn.getParamTypes();
    for (size_t i = 0; i < pNames.size(); ++i) {
        if (i > 0) out << ", ";
        out << irTypeToString(pTypes[i]) << " %" << pNames[i];
    }
    out << ") {\n";

    for (const auto& block : fn.getBlocks()) {
        print(*block);
    }

    out << "}\n";
}

void IRPrinter::print(const IRBasicBlock& block) {
    out << block.getName() << ":\n";
    for (const auto& inst : block.getInstructions()) {
        out << "  ";
        print(inst);
    }
}

void IRPrinter::print(const IRInstruction& inst) {
    out << inst.toString() << "\n";
}
