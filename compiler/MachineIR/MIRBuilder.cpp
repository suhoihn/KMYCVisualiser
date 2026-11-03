#include "MIRBuilder.hpp"

#include <unordered_map>
#include "../Core/errorhandler.hpp"
#include "TypeLayout.hpp"
#include <assert.h>

MIRBuilder::MIRBuilder(std::vector<HIRFunction*> irFunctions)
    : irFunctions(irFunctions) 
{}

IRValue MIRBuilder::makeValue(Type* type) {
    return IRValue{.id = lastValueId++, .type = type};
}

static std::vector<IRValue> convertHIRargs(const std::vector<HIROperand>& HIRargs) {
    std::vector<IRValue> res;
    for (const auto& arg : HIRargs) {
        if (!std::holds_alternative<IRValue>(arg)) {
            throw KMYCompileError("Untransformed VarRef(" + std::get<VarRef>(arg).sym->name + ")");
        }
        res.push_back(std::get<IRValue>(arg));
    }

    return res;
}

std::vector<MIRInstr> MIRBuilder::lowerHIRInstr(const IRInstr& instr) {

    // SERIOUS TODO: This assumes 1:1 mapping, which is obviously false.
    // This is a temporary solution. We will need to handle cases where one IRInstr maps to multiple MIRInstrs.
    auto make = [&](MIROp op) {
        MIRInstr out;

        out.op = op;
        out.dst = instr.dst;
        out.args = convertHIRargs(instr.args);
        out.imm = instr.imm;

        return out;
    };


    switch (instr.op) {

        // =====================
        // Constants
        // =====================
        case IROp::CONST_INT:
            return { make(MIROp::CONST) };

        case IROp::CONST_STRING:
            return { make(MIROp::CONST_STRING) };

        case IROp::STRING_EQUAL: {
            MIRInstr call;
            call.op = MIROp::RUNTIME_CALL;
            call.dst = instr.dst;
            call.args = convertHIRargs(instr.args);
            call.imm = 2;
            return { call };
        }

        case IROp::STRING_CONCAT: {
            MIRInstr call;
            call.op = MIROp::RUNTIME_CALL;
            call.dst = instr.dst;
            call.args = convertHIRargs(instr.args);
            call.imm = 3;
            return { call };
        }

        case IROp::STRING_LENGTH: {
            MIRInstr call;
            call.op = MIROp::RUNTIME_CALL;
            call.dst = instr.dst;
            call.args = convertHIRargs(instr.args);
            call.imm = 4;
            return { call };
        }

        case IROp::STRING_BYTE_AT: {
            MIRInstr call;
            call.op = MIROp::RUNTIME_CALL;
            call.dst = instr.dst;
            call.args = convertHIRargs(instr.args);
            call.imm = 5;
            return { call };
        }

        case IROp::STRING_FROM_BYTE: {
            MIRInstr call;
            call.op = MIROp::RUNTIME_CALL;
            call.dst = instr.dst;
            call.args = convertHIRargs(instr.args);
            call.imm = 8;
            return { call };
        }

        case IROp::FORCE_UNWRAP:
            return { make(MIROp::FORCE_UNWRAP) };

        case IROp::FILE_READ: {
            MIRInstr call;
            call.op = MIROp::RUNTIME_CALL;
            call.dst = instr.dst;
            call.args = convertHIRargs(instr.args);
            call.imm = 6;
            return { call };
        }

        case IROp::FILE_WRITE: {
            MIRInstr call;
            call.op = MIROp::RUNTIME_CALL;
            call.dst = instr.dst;
            call.args = convertHIRargs(instr.args);
            call.imm = 7;
            return { call };
        }

        case IROp::CONST_NULL: {
            MIRInstr constant = make(MIROp::CONST);
            constant.imm = 0;
            return { constant };
        }


        // =====================
        // Arithmetic
        // =====================
        case IROp::ADD:
            return { make(MIROp::ADD) };

        case IROp::SUB:
            return { make(MIROp::SUB) };

        case IROp::MUL:
            return { make(MIROp::MUL) };

        case IROp::DIV:
            return { make(MIROp::DIV) };

        case IROp::MOD:
            return { make(MIROp::MOD) };


        // =====================
        // Comparison
        // =====================
        case IROp::EQUAL:
            return { make(MIROp::CMP_EQ) };

        case IROp::NOT_EQUAL:
            return { make(MIROp::CMP_NE) };

        case IROp::LESS:
            return { make(MIROp::CMP_LT) };

        case IROp::LESS_EQUAL:
            return { make(MIROp::CMP_LE) };

        case IROp::GREATER:
            return { make(MIROp::CMP_GT) };

        case IROp::GREATER_EQUAL:
            return { make(MIROp::CMP_GE) };


        // =====================
        // Bitwise
        // =====================
        case IROp::BIT_AND:
            return { make(MIROp::AND) };

        case IROp::BIT_OR:
            return { make(MIROp::OR) };

        case IROp::BIT_XOR:
            return { make(MIROp::XOR) };

        case IROp::LEFT_SHIFT:
            return { make(MIROp::SHL) };

        case IROp::RIGHT_SHIFT:
            return { make(MIROp::SHR) };


        // =====================
        // Unary
        // =====================
        case IROp::NEG:
            return { make(MIROp::NEG) };

        case IROp::BIT_NOT:
            return { make(MIROp::NOT) };


        // =====================
        // Memory
        // =====================
        case IROp::LOAD_GLOBAL:
            return { make(MIROp::LOAD_GLOBAL) };

        case IROp::STORE_GLOBAL:
            return { make(MIROp::STORE_GLOBAL) };

        case IROp::GET_GLOBAL_ADDR:
            return { make(MIROp::GLOBAL_ADDR) };

        case IROp::LOAD_CELL:
            // Load from a cell (dereference)
            // arg0: cell pointer
            // dst: value loaded from cell
            return { 
                MIRInstr {
                    .op = MIROp::LOAD,
                    .dst = instr.dst,
                    .args = convertHIRargs(instr.args), // [cell_ptr]
                    .imm = 0,
                }
            };

        case IROp::STORE_CELL:
            // Store to a cell (dereference)
            // arg0: value to store
            // arg1: cell pointer
            return { 
                MIRInstr {
                    .op = MIROp::STORE,
                    .args = convertHIRargs(instr.args), // [cell_ptr, value_to_store]
                    .imm = 0,
                }
            };


        case IROp::GET_ADDR:
            return { make(MIROp::LEA) };

        case IROp::LOAD_INDIRECT:
            return {
                MIRInstr {
                    .op = MIROp::LOAD_INDIRECT,
                    .dst = instr.dst,
                    .args = convertHIRargs(instr.args), // [pointer]
                    .imm = 0,
                }
            };

        case IROp::STORE_INDIRECT:
            return {
                MIRInstr {
                    .op = MIROp::STORE_INDIRECT,
                    .args = convertHIRargs(instr.args), // [pointer, value]
                    .imm = 0,
                }
            };

        case IROp::ALLOC_HEAP: {
            MIRInstr alloc;
            alloc.op = MIROp::ALLOC;
            alloc.dst = instr.dst;
            alloc.imm = instr.imm.value();
            return { alloc };
        }

        case IROp::ALLOC_SHARED:
            return { make(MIROp::ALLOC_SHARED) };

        case IROp::RETAIN:
            return { make(MIROp::RETAIN) };

        case IROp::RELEASE:
            return { make(MIROp::RELEASE) };

        case IROp::ALLOC_ARRAY: {
            MIRInstr alloc;
            alloc.op = MIROp::ALLOC;
            alloc.dst = instr.dst;
            alloc.imm = instr.imm.value();
            return { alloc };
        }

        case IROp::MALLOC_BYTES:
            return { make(MIROp::MALLOC_BYTES) };

        case IROp::FREE:
            return { make(MIROp::FREE) };

        case IROp::ALLOC_ARRAY_DYNAMIC: {
            MIRInstr alloc;
            alloc.op = MIROp::ALLOC_DYNAMIC;
            alloc.dst = instr.dst;
            alloc.args = convertHIRargs(instr.args); // [element_count]
            alloc.imm = instr.imm.value(); // element size
            return { alloc };
        }

        case IROp::LOAD_FIELD:
            return {
                MIRInstr {
                    .op = MIROp::LOAD,
                    .dst = instr.dst,
                    .args = convertHIRargs(instr.args), // [object_ptr]
                    .imm = instr.imm.value()
                }
            };

        case IROp::STORE_FIELD:
            return {
                MIRInstr {
                    .op = MIROp::STORE,
                    .args = convertHIRargs(instr.args), // [object_ptr, value]
                    .imm = instr.imm.value()
                }
            };

        case IROp::STORE_ARRAY:
            return {
                MIRInstr {
                    .op = MIROp::STORE,
                    .args = convertHIRargs(instr.args), // [array_data_ptr, value]
                    .imm = instr.imm.value()
                }
            };

        case IROp::LOAD_ARRAY_INDEX:
            return {
                MIRInstr {
                    .op = MIROp::LOAD_INDEX,
                    .dst = instr.dst,
                    .args = convertHIRargs(instr.args), // [array_data_ptr, index]
                    .imm = instr.imm.value()
                }
            };

        case IROp::STORE_ARRAY_INDEX:
            return {
                MIRInstr {
                    .op = MIROp::STORE_INDEX,
                    .args = convertHIRargs(instr.args), // [array_data_ptr, index, value]
                    .imm = instr.imm.value()
                }
            };


        // =====================
        // Calls
        // =====================
        case IROp::CALL: 
            return { make(MIROp::CALL) };


        case IROp::PARAM:
            return { 
                MIRInstr {
                    .op = MIROp::PARAM,
                    .dst = instr.dst,
                    .args = convertHIRargs(instr.args),
                    .imm = instr.imm.value() + 1, // +1 because MIR PARAM's 0th idx is for closure pointer for now. TODO
                }
            };


        case IROp::RETURN:
            return { make(MIROp::RET) };


        // =====================
        // Runtime
        // =====================
        case IROp::PRINT:
        {
            MIRInstr call;
            call.op = MIROp::RUNTIME_CALL;
            call.args = convertHIRargs(instr.args);
            call.imm = instr.args[0].index() == 0 &&
                       std::get<IRValue>(instr.args[0]).type == &Types::STRING_TYPE
                       ? 1 : 0;

            return {
                call
            };
        }


        // =====================
        // Closures
        // =====================
        case IROp::ALLOC_CELL_INIT: {
            // arg0: value to store in cell
            // dst: cell pointer

            // Allocate a cell.
            MIRInstr alloc;
            alloc.op = MIROp::ALLOC;
            alloc.dst = instr.dst;
            alloc.imm = 8;

            // Populate the cell with the value.
            MIRInstr store;
            store.op = MIROp::STORE;
            store.args = {
                instr.dst.value(),
                std::get<IRValue>(instr.args[0]) // Should be IRValue... Trust SSABuilder.
            };
            store.imm = 0;

            return {
                alloc,
                store
            };
        }


        // These should disappear before MIR
        case IROp::LOGICAL_AND:
        case IROp::LOGICAL_OR:
        case IROp::LOGICAL_NOT:
            throw KMYCompileError(
                "Logical operation survived into MIR lowering"
            );

        case IROp::FUNC_LABEL: {
            // Arg 0: env pointer (if any)
            // Imm: function id

            // code pointer
            MIRInstr code;
            code.op = MIROp::LABEL;
            code.dst = makeValue(instr.dst->type);
            code.imm = instr.imm.value();


            // allocate closure object
            MIRInstr alloc;
            alloc.op = MIROp::ALLOC;
            alloc.dst = instr.dst;
            alloc.imm = 16; // 2 pointers


            // store code pointer
            MIRInstr storeCode;
            storeCode.op = MIROp::STORE;
            storeCode.args = {
                instr.dst.value(),
                code.dst.value()
            };
            storeCode.imm = 0;


            if (instr.args.size() > 0) {
                MIRInstr storeEnv;
                storeEnv.op = MIROp::STORE;
                storeEnv.args = {
                    instr.dst.value(), // Base address of closure object
                    std::get<IRValue>(instr.args[0]) // Again, have faith.
                };
                storeEnv.imm = 8;
                return {
                    code,
                    alloc,
                    storeCode,
                    storeEnv
                };
            } 

            // TODO: THIS WILL NEVER HAPPEN!
            throw KMYCompileError("This should have been handled in HIR.");
            // No env pointer.
            // i.e., no env exists on my function. (Children captures nothing)
            IRValue nullValue = makeValue(&Types::INT_TYPE);
            MIRInstr nullValueInstr {
                .op = MIROp::CONST,
                .dst = nullValue,
                .imm = 0
            };

            MIRInstr storeEnv {
                .op = MIROp::STORE,
                .args = {
                    instr.dst.value(), // Base address of closure object
                    nullValue
                },
                .imm = 8
            };
            
            return {
                code,
                alloc,
                storeCode,
                nullValueInstr,
                storeEnv
            };
        }


        case IROp::ALLOC_ENV: {
            MIRInstr alloc;
            alloc.op = MIROp::ALLOC;
            alloc.dst = instr.dst;
            alloc.imm = instr.imm.value() * 8; // Assuming the cell is a pointer to the base type

            return {
                alloc
            };
        }
        case IROp::INTRODUCE_ENV: {
            // Convention. Env pointer is the first argument of the function.
            return { 
                MIRInstr {
                    .op = MIROp::PARAM,
                    .dst = instr.dst,
                    .imm = 0,
                }
            };
        }
        case IROp::SET_ENV: {
            // Arg 0: env pointer value
            // Arg 1: value to set
            // Imm: env slot index

            // format: STORE base offset value
            MIRInstr store {
                .op = MIROp::STORE,
                .args = convertHIRargs(instr.args), // [env_ptr, value_to_set]
                .imm = instr.imm.value() * 8 // Assuming 64-bit architecture, each cell is 8 bytes
            };
            return { store };
        }
        case IROp::GET_ENV: {
            // Arg 0: env pointer value
            // Imm: env slot index

            // format: LOAD base offset
            MIRInstr load {
                .op = MIROp::LOAD,
                .dst = instr.dst,
                .args = convertHIRargs(instr.args), // [env_ptr]
                .imm = instr.imm.value() * 8 // Assuming 64-bit architecture, each cell is 8 bytes
            };
            return { load };
        }

        case IROp::PHI: {
            // Phi on original block is gone.
            return {};
        }

        case IROp::BIND:
            // SSA has already propagated this alias into later operands.
            return {};


        case IROp::GARBAGE:
        default:
            throw KMYCompileError(
                std::string("Unhandled HIR instruction: ") + toString(instr.op)
            );
    }
}

static MIRTerm lowerTerm(
    const HIRTerm& term,
    const std::unordered_map<HIRBlock*, MIRBlock*>& blockMap
) {
    return std::visit([&](const auto& t) -> MIRTerm {

        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, JumpTerm<IRInstr>>) {

            return JumpTerm<MIRInstr>{
                blockMap.at(t.target)
            };

        } else if constexpr (std::is_same_v<T, BranchTerm<IRInstr>>) {

            return BranchTerm<MIRInstr>{
                t.cond,
                blockMap.at(t.trueTarget),
                blockMap.at(t.falseTarget)
            };

        } else {
            return t;
        }
    }, term);
}

MIRFunction* MIRBuilder::lowerHIRFunc(HIRFunction* hirFunc) {
    auto* mirFunc = new MIRFunction;
    mirFunc->isEntryFunc = hirFunc->isEntryFunc;
    mirFunc->functionId = hirFunc->functionId;

    // 1. Create all blocks (for map)
    std::unordered_map<HIRBlock*, MIRBlock*> blockMap;
    for (const auto& block : hirFunc->blocks) {
        auto* mirBlock = new MIRBlock;
        mirBlock->id = block->id;
        blockMap[block] = mirBlock;
        mirFunc->blocks.push_back(mirBlock);
    }

    mirFunc->entry = blockMap[hirFunc->entry];
    
    this->lastValueId = hirFunc->lastValueId;
    // 2. Fill them

    // Parellel copies for each block
    /*
        v2 = phi(v0: B0, v1: B1)
        v3 = phi(v5: B1, v6: B2)

        parallelCopies[B0] = {
            {v2 <- v0}
        }

        parallelCopies[B1] = {
            {v2 <- v1},
            {v3 <- v5}
        }

        parallelCopies[B2] = {
            {v3 <- v5}
        }
    */
    std::unordered_map<HIRBlock*, std::vector<std::pair<IRValue, IRValue>>> parallelCopies;
    for (const auto& block : hirFunc->blocks) {
        auto* mirBlock = blockMap[block];
        for (const auto& instr : block->code) {
            auto loweredInstrs = lowerHIRInstr(instr);
            for (const auto& loweredInstr : loweredInstrs) {
                mirBlock->code.push_back(loweredInstr);
            }

            if (instr.op == IROp::PHI) {
                for (const auto& phi : instr.phis) {
                    assert(std::holds_alternative<IRValue>(phi.value));
                    
                    parallelCopies[phi.pred].push_back(
                        { instr.dst.value(), std::get<IRValue>(phi.value) } 
                    );
                }
            }
        }
        if (block->term.has_value()) {
            mirBlock->term = lowerTerm(block->term.value(), blockMap);
        }

        for (const auto& succ : block->succs) {
            mirBlock->succs.push_back(blockMap[succ]);
        }
        for (const auto& pred : block->preds) {
            mirBlock->preds.push_back(blockMap[pred]);
        }
    }

    // 3. Lower phis

    // Check for cycles and resolve it.
    /*
        Problematic case:

        v2 = phi(v1: B0, v3: B1)
        v1 = phi(v2: B0, v2: B1)

        parallelCopies[B0] = {
            {v2 <- v1},
            {v1 <- v2},
        }

        parallelCopies[B1] = {
            {v2 <- v3},
            {v1 <- v2}
        }

        // parallelCopies in every block is problematic.

        Algorithm:
        For each pair {D <- S},
        if D is used as a source (S) somewhere (except itself), that's problematic.
        Thus, introduce a temp to preserve it.

        For B0 example,
        both v2 and v1 will be marked problematic.
        Say v2 <- v1 (since this is preceding) is our target to resolve.
        Make tmp <- v1, and replace all pair where S is v1 to tmp.
        Then re-run the check.
    */
    for (auto& [blockWithCopies, originalCopies] : parallelCopies) {

        // Copies on this block
        auto copies = originalCopies;

        // TODO: This version can be improved (maybe?) with hashmap...? (i think)
        // Currently O(n^3) (eww)

        // Drain every copies vector.
        while (!copies.empty()) {
            bool foundSafeCopy = false;

            for (size_t i = 0; i < copies.size(); ++i) {
                const auto& [D, S] = copies[i];

                bool isThisCopySafe = true;
                for (size_t j = 0; j < copies.size(); ++j) {
                    // Skip myself.
                    if (i == j) continue;
                    
                    const auto &[otherD, otherS] = copies[j];

                    // D's old value is still needed.
                    if (D.id == otherS.id) {
                        isThisCopySafe = false;
                        break;
                    }
                }

                // This copy is not safe.
                // Not safe to erase from copies vector directly (need temp)
                // Make someone to resolve this for me!
                if (!isThisCopySafe) continue;

                blockMap[blockWithCopies]->code.push_back(
                    MIRInstr{
                        .op = MIROp::MOVE,
                        .dst = D,
                        .args = {S},
                    }
                );

                copies.erase(
                    copies.begin() + i
                );

                foundSafeCopy = true;
                break;
            }

            // Re-examine the whole thing after emitting safe copy.
            // Since the safety relationship may have changed.
            // e.g. A <- B, C <- A
            // C <- A is safe, so emitted
            // Re-examine and A <- B is safe.
            if (foundSafeCopy) continue;

            // No safe copy exists AT ALL.
            // The remaining copies contain a cycle.
            // e.g., A <- B, B <- A

            const auto& [D, S] = copies[0];

            // We need temp.
            IRValue temp = makeValue(S.type);

            // Preserve the old value of S.
            blockMap[blockWithCopies]->code.push_back(
                MIRInstr{
                    .op = MIROp::MOVE,
                    .dst = temp,
                    .args = {S},
                }
            );

            // Any pending copy that needed old S
            // now gets it from the temporary.
            for (auto &[pendingD, pendingS] : copies) {
                if (pendingS.id == S.id) {
                    pendingS = temp;
                }
            }

            // After this, we re-examine the whole thing.
        }
    }
    // Update MIR func's value cnt (since it may have increased).
    // Used for stack offset.
    mirFunc->lastValueId = this->lastValueId;
    return mirFunc;
}

void MIRBuilder::insertMoves(
    const IRInstr& instr,
    std::unordered_map<HIRBlock*, MIRBlock*>& blockMap, 
    const std::vector<IncomingPhi>& phis
) {
    for (const auto& phi : phis) {
        blockMap[phi.pred]->code.push_back(
            MIRInstr{
                .op = MIROp::MOVE,
                .dst = instr.dst,
                .args = { std::get<IRValue>(phi.value) },
            }
        );
    }
}

std::vector<MIRFunction*> MIRBuilder::lower() {
    std::vector<MIRFunction*> loweredFunctions;

    for (auto* irFunc : irFunctions) {
        // Lower each IRFunction to MIRFunction
        loweredFunctions.push_back(lowerHIRFunc(irFunc));
    }
    return loweredFunctions;
}

