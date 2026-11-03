#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <variant>
#include "IROp.hpp"
#include "IRValue.hpp"
#include "BasicBlock.hpp"

struct IRInstr;
using HIRBlock = BasicBlock<IRInstr>;


struct IncomingPhi {
    HIRBlock* pred;
    HIROperand value;
};

struct IRInstr {
    IROp op;

    std::optional<IRValue> dst;

    std::vector<HIROperand> args;
    //std::optional<IRValue> env;

    ConstValue literal; // For actual literals

    std::optional<int64_t> imm; // For arguments of ints (e.g, env slot, func id, param id)

    std::vector<IncomingPhi> phis;

    VarSymbol* defSym = nullptr; // The symbol it defines (if any)
    VarSymbol* phiVar = nullptr; // Used for SSA renaming
};

inline void printDefSym(std::ostream& os, const IRInstr& instr) {
    if (instr.defSym != nullptr) {
        os << " [def " << instr.defSym->name << "]";
    }
}

inline std::ostream& operator<<(
    std::ostream& os,
    const IRInstr& instr
) {
    switch (instr.op) {

        case IROp::CONST_INT: {
            os << instr.dst.value();
            printDefSym(os, instr);

            os << " = const "
               << instr.imm.value();

            break;
        }

        case IROp::CONST_STRING: {
            os << instr.dst.value();
            printDefSym(os, instr);

            os << " = const_string #"
               << instr.imm.value();

            break;
        }

        case IROp::PHI: {
            os << instr.dst.value();
            printDefSym(os, instr);

            os << " = phi ";

            for (size_t i = 0; i < instr.phis.size(); ++i) {
                os << instr.phis[i].value
                   << "(B"
                   << instr.phis[i].pred->id
                   << ")";

                if (i + 1 < instr.phis.size()) {
                    os << ", ";
                }
            }

            break;
        }

        case IROp::BIND: {
            os << "bind";

            if (instr.defSym != nullptr) {
                os << " [def " << instr.defSym->name << "]";
            }

            if (!instr.args.empty()) {
                os << " " << instr.args[0];
            }

            break;
        }

        case IROp::PRINT: {
            os << "print ";

            if (!instr.args.empty()) {
                os << instr.args[0];
            }

            break;
        }

        case IROp::FUNC_LABEL: {
            os << instr.dst.value();
            printDefSym(os, instr);

            os << " = func_label (func id: "
               << instr.imm.value()
               << ")";

            if (!instr.args.empty()) {
                os << " ";

                for (size_t i = 0; i < instr.args.size(); ++i) {
                    os << instr.args[i];

                    if (i + 1 < instr.args.size()) {
                        os << ", ";
                    }
                }
            }

            break;
        }

        case IROp::PARAM: {
            os << instr.dst.value();
            printDefSym(os, instr);

            os << " = param "
               << instr.imm.value();

            break;
        }

        case IROp::UPVALUE: {
            os << instr.dst.value();
            printDefSym(os, instr);

            os << " = upvalue "
               << instr.imm.value();

            break;
        }

        case IROp::ALLOC_ENV: {
            os << instr.dst.value();
            printDefSym(os, instr);

            os << " = alloc_env of size "
               << instr.imm.value();

            break;
        }

        case IROp::SET_ENV: {
            os << toString(instr.op)
               << " "
               << instr.args[0]
               << ", "
               << instr.imm.value()
               << ", "
               << instr.args[1];

            break;
        }

        case IROp::GET_ENV: {
            os << instr.dst.value();
            printDefSym(os, instr);

            os << " = "
               << toString(instr.op)
               << " "
               << instr.args[0]
               << ", "
               << instr.imm.value();

            break;
        }

        case IROp::ALLOC_CELL_INIT: {
            os << instr.dst.value();
            printDefSym(os, instr);

            os << " = "
               << toString(instr.op)
               << " "
               << instr.args[0];

            break;
        }

        case IROp::STORE_CELL: {
            os << toString(instr.op)
               << " "
               << instr.args[0]
               << ", "
               << instr.args[1];

            break;
        }

        case IROp::LOAD_CELL: {
            os << instr.dst.value();
            printDefSym(os, instr);

            os << " = "
               << toString(instr.op)
               << " "
               << instr.args[0];

            break;
        }

        case IROp::LOAD_GLOBAL: {
            os << instr.dst.value();
            printDefSym(os, instr);
            os << " = load_global " << instr.imm.value();
            break;
        }

        case IROp::STORE_GLOBAL: {
            os << "store_global " << instr.imm.value() << ", " << instr.args[0];
            break;
        }

        case IROp::ALLOC_SHARED: {
            os << instr.dst.value();
            printDefSym(os, instr);
            os << " = alloc_shared " << instr.imm.value() << " bytes";
            break;
        }

        case IROp::RETAIN:
        case IROp::RELEASE: {
            os << toString(instr.op) << " " << instr.args[0];
            break;
        }

        case IROp::RETURN: {
            os << "return";

            if (!instr.args.empty()) {
                os << " "
                   << instr.args[0];
            }

            break;
        }

        default: {
            if (instr.dst.has_value()) {
                os << instr.dst.value();
                printDefSym(os, instr);

                os << " = "
                   << toString(instr.op);
            } else {
                os << toString(instr.op);
            }

            if (!instr.args.empty()) {
                os << " ";

                for (size_t i = 0; i < instr.args.size(); ++i) {
                    os << instr.args[i];

                    if (i + 1 < instr.args.size()) {
                        os << ", ";
                    }
                }
            }

            break;
        }
    }

    return os;
}
