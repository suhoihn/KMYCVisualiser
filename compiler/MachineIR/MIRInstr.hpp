#pragma once

#include <ostream>
#include <vector>
#include <optional>
#include "../CodegenIR/IRValue.hpp"
#include "MIROp.hpp"


struct MIRInstr {

    MIROp op;

    // Most instructions produce a value:
    //
    // v1 = ADD v2 v3
    //
    std::optional<IRValue> dst;


    // Input values:
    //
    // ADD v1 v2
    //
    std::vector<IRValue> args;


    // Immediate integer:
    //
    // v1 = MOV_IMM 42
    //
    std::optional<int> imm;
};


inline std::ostream& operator<<(std::ostream& os, const MIRInstr& instr)
{
    if (instr.dst.has_value()) {
        os << instr.dst.value() << " = ";
    }

    os << instr.op;


    if (instr.imm.has_value()) {
        os << " " << instr.imm.value();
    }

    for (auto& arg : instr.args) {
        os << " " << arg;
    }

    return os;
}