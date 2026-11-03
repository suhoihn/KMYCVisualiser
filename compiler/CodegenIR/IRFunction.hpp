#pragma once

#include <vector>
#include "../Core/type.hpp"
#include "BasicBlock.hpp"
#include "../Utils/SymbolPrinter.hpp"


template<typename Instr>
struct IRFunction {
    //VarSymbol* funcSymbol = nullptr;
    int functionId = INVALID_SLOT;
    FunctionType* funcType = nullptr;
    std::vector<BasicBlock<Instr>*> blocks;
    BasicBlock<Instr>* entry = nullptr;

    int lastValueId = INVALID_SLOT;
    bool isEntryFunc = false;
};

template<typename Instr>
inline std::ostream& operator<<(std::ostream& os, const IRFunction<Instr>& fn) {
    os << "Function " << fn.functionId << " (type signature: " << typeToString(fn.funcType) << ")\n";
    os << "--------------------------------\n";

    for (const auto* bb : fn.blocks) {
        if (bb == fn.entry) {
            os << "(entry)\n";
        }
        os << *bb;

        os << "\n";
    }

    return os;
}