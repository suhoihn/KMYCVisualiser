#pragma once

#include <vector>
#include <variant>
#include <optional>
#include <ostream>
#include "IRValue.hpp"

struct VarRef {
    VarSymbol* sym;
};

using HIROperand = std::variant<IRValue, VarRef>;

inline std::ostream& operator<<(std::ostream& os, const HIROperand& operand) {
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, IRValue>) {
            os << value;
        } else if constexpr (std::is_same_v<T, VarRef>) {
            os << "VarRef(" << value.sym->name << ")";
        }
    }, operand);

    return os;
}

// Forward decl.    
template<typename Instr>
struct BasicBlock;

template<typename Instr>
struct JumpTerm {
    BasicBlock<Instr>* target;
};

template<typename Instr>
struct BranchTerm {
    HIROperand cond;
    BasicBlock<Instr>* trueTarget;
    BasicBlock<Instr>* falseTarget;
};

struct ReturnTerm {
    std::optional<HIROperand> value;
};

struct HaltTerm {};

template<typename Instr>
using Terminator =
    std::variant<
        JumpTerm<Instr>,
        BranchTerm<Instr>,
        ReturnTerm,
        HaltTerm
>;


// Created on variable decl site.
struct VarDef {
    VarSymbol* sym;
    HIROperand value;
};

// Created on variable use site.
struct VarUse {
    VarSymbol* sym;
    int operandIdx;
    int instrIdx;
};

template<typename Instr>
struct BasicBlock {
    int id;
    std::optional<Terminator<Instr>> term = std::nullopt;

    std::vector<Instr> code;    
    
    std::vector<BasicBlock<Instr>*> preds; // Optional graph
    std::vector<BasicBlock<Instr>*> succs; // Optional graph

    // CONTINUE FROM HERE: Populate those defs/uses
    // Also make another pass that fills them
    // Before that, just mark how many args and output like ADD ??? ???
    // Or better, like LLVM, mark it as "use x (varsymbol's name)"
    
    std::vector<VarDef> defs;
    std::vector<VarUse> uses;
};

template<typename Instr>
inline std::ostream& operator<<(std::ostream& os, const JumpTerm<Instr>& t) {
    os << "jump B" << t.target->id;
    return os;
}

template<typename Instr>
inline std::ostream& operator<<(std::ostream& os, const BranchTerm<Instr>& t) {
    os << "branch " << t.cond
       << ", B" << t.trueTarget->id
       << ", B" << t.falseTarget->id;
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const ReturnTerm& t) {
    os << "return ";

    if (t.value.has_value()) {
        os << t.value.value();
    }

    return os;
}

inline std::ostream& operator<<(std::ostream& os, const HaltTerm& t) {
    os << "HALT";
    return os;
}

template<typename Instr>
inline std::ostream& operator<<(std::ostream& os, const Terminator<Instr>& term) {
    std::visit([&](const auto& t) {
        os << t;
    }, term);

    return os;
}

template<typename Instr>
inline std::ostream& operator<<(std::ostream& os, const BasicBlock<Instr>& bb) {
    os << "B" << bb.id << ":\n";

    for (const auto& instr : bb.code) {
        os << "    " << instr << '\n';
    }

    os << "    ";

    if (bb.term.has_value()) {
        os << bb.term.value();
    } else {
        os << "<NO TERM! This is an error!>";
    }

    os << '\n';
    
    // Print var defs here
    os << "DEFs here\n";
    if (bb.defs.size() == 0) {
        os << "<empty>\n";
        return os;
    }

    for (const auto& def : bb.defs) {
        os << "    - " << def.sym->name << " : " << def.value;
        os << '\n';
    }

    os << '\n';

    return os;
}