#include "IRBuilder.hpp"
#include "../Core/newParser.hpp"

#include <assert.h>
#include "../Core/type.hpp"
#include "../Core/errorhandler.hpp"
#include "IRFunction.hpp"
#include <iostream>
#include "../Semantics/TypeInterner.hpp"
#include "../MachineIR/TypeLayout.hpp"


static inline bool hasTerminator(HIRBlock* bb) {
    return bb->term.has_value();
}

IRBuilder::IRBuilder(Module& module, StringPool& stringPool, std::vector<int> initializerIds)
    : program(module.program), stringPool(stringPool), initializerIds(std::move(initializerIds))
{}

void IRBuilder::emit(const IRInstr& instr) {
    assert(currCtx && "No current function context");
    assert(currCtx->currBlock && "No current block");

    currCtx->currBlock->code.push_back(instr);
}


static IRLocalInfo* getValueFromLocal(IRCodegenFnCtx* fnCtx, VarSymbol* sym) {
    auto& localMap = fnCtx->locals;

    auto it = localMap.find(sym);
    if (it != localMap.end()) {
        return &it->second;
    }
    std::cout << "Local value lookup missed: " << sym->name << '\n';
    return nullptr;
}

std::vector<HIRFunction*> IRBuilder::compile() {
    assert(program);

    // Native codegen no longer compiles the parser's synthetic root node.
    // This compiler-generated function is the module initializer; it owns the
    // same top-level statement pointers and compatibility closure metadata.
    FunctionExpr initializer(
        std::vector<Parameter>{},
        std::make_shared<Block>(std::static_pointer_cast<Block>(program->body)->statements),
        nullptr,
        program->isEntry
    );
    initializer.functionContext = program->functionContext;
    initializer.functionId = program->functionId;
    initializer.type = TypeInterner::getFunctionType({}, &Types::VOID_TYPE);
    moduleInitializer = &initializer;
    initializer.accept(*this);
    moduleInitializer = nullptr;

    if (!hasTerminator(currCtx->currBlock)) {
        currCtx->currBlock->term = HaltTerm{};
    }

    return functions;
}

inline HIROperand IRBuilder::getLastValue() { return currCtx->lastValue; }
inline void IRBuilder::setLastValue(HIROperand value) { currCtx->lastValue = value; }

IRValue IRBuilder::makeValue(Type* type) {
    return IRValue{currCtx->nextId++, type};
}

HIRBlock* IRBuilder::makeBlock() {
    auto* newBlock = new HIRBlock{currCtx->nextBlockId++};
    currFunc->blocks.push_back(newBlock);
    return newBlock;
}

void IRBuilder::connectBlock(HIRBlock* from, HIRBlock* to) {
    from->succs.push_back(to);
    to->preds.push_back(from);
}

// ======================================================
// Expressions
// ======================================================

void IRBuilder::visit(Literal& e) {
    if (std::holds_alternative<std::nullptr_t>(e.value)) {
        IRValue dst = makeValue(e.type);
        emit({ .op = IROp::CONST_NULL, .dst = dst });
        setLastValue(dst);
        return;
    }

    ConstValue v = std::visit([](auto&& arg) -> ConstValue {
        return ConstValue(arg);
    }, e.value);

    if (std::holds_alternative<std::string>(e.value)) {
        IRValue dst = makeValue(e.type);
        StringId stringId = stringPool.intern(std::get<std::string>(e.value));
        emit({
            .op = IROp::CONST_STRING,
            .dst = dst,
            .imm = static_cast<int64_t>(stringId)
        });
        setLastValue(dst);
        return;
    }

    if (
        !std::holds_alternative<int>(e.value) &&
        !std::holds_alternative<bool>(e.value)
    ) {
        throw KMYCompileError("Only string, bool and int yet sorry.");
    }

    int val;
    if (std::holds_alternative<int>(e.value)) {
        val = std::get<int>(e.value);
    } else {
        val = (int)std::get<bool>(e.value);
    }

    IRValue dst = makeValue(e.type);
    IRInstr instr{
        .op = IROp::CONST_INT, 
        .dst = dst,
        .imm = val
    };
    emit(instr);

    setLastValue(dst);
}

void IRBuilder::visit(ArrayLiteral& e) {
    auto* arrayType = static_cast<ArrayType*>(e.type);
    if (!arrayType->fixedLength.has_value()) {
        throw KMYCompileError("Native arrays must have a fixed length.");
    }

    size_t elementSize = TypeLayout::sizeOf(arrayType->elementType);
    if (elementSize != PTR_SIZE) {
        throw KMYCompileError("Native array elements must fit in one machine word.");
    }

    IRValue array = makeValue(e.type);
    emit({
        .op = IROp::ALLOC_ARRAY,
        .dst = array,
        .imm = static_cast<int64_t>(*arrayType->fixedLength * elementSize)
    });

    // Store each element into the array
    for (size_t i = 0; i < e.elements.size(); ++i) {
        e.elements[i]->accept(*this);
        emit({
            .op = IROp::STORE_ARRAY,
            .args = { array, getLastValue() },
            .imm = static_cast<int64_t>(i * elementSize)
        });
    }

    setLastValue(array);
}

void IRBuilder::visit(RecordLiteral& e) {
    throw KMYCompileError("rec Not yet");
}

void IRBuilder::visit(Variable& e) {
    if (e.symbol->isModuleGlobal) {
        IRValue value = makeValue(e.type);
        emit({ .op = IROp::LOAD_GLOBAL, .dst = value, .imm = e.symbol->moduleGlobalSlot });
        setLastValue(value);
        return;
    }
    auto upvalueIt = currCtx->upvalues.find(e.symbol);
    if (upvalueIt != currCtx->upvalues.end()) {
        IRValue cell = upvalueIt->second;

        IRValue loaded = makeValue(e.type);

        emit({
            .op = IROp::LOAD_CELL,
            .dst = loaded,
            .args = {cell}
        });

        setLastValue(loaded);
        return;
    }

    // Get from local
    IRLocalInfo* localInfo = getValueFromLocal(currCtx, e.symbol);

    if (localInfo && localInfo->isCell) {

        IRValue loaded = makeValue(e.type);

        emit({
            .op = IROp::LOAD_CELL,
            .dst = loaded,
            .args = {localInfo->value}
        });

        setLastValue(loaded);

    } else {
        setLastValue(VarRef{e.symbol});
    }

    // assert(false && "Variable not found in locals or upvalues");
}

static IROp binaryOpToIROp(BinaryOp op) {
    switch (op) {
        case BinaryOp::Plus:          return IROp::ADD;
        case BinaryOp::Minus:         return IROp::SUB;
        case BinaryOp::Star:          return IROp::MUL;
        case BinaryOp::Slash:         return IROp::DIV;
        case BinaryOp::Percent:       return IROp::MOD;

        case BinaryOp::EqualEqual:    return IROp::EQUAL;
        case BinaryOp::NotEqual:      return IROp::NOT_EQUAL;
        case BinaryOp::Less:          return IROp::LESS;
        case BinaryOp::LessEqual:     return IROp::LESS_EQUAL;
        case BinaryOp::Greater:       return IROp::GREATER;
        case BinaryOp::GreaterEqual:  return IROp::GREATER_EQUAL;

        case BinaryOp::LogicalAnd:    return IROp::LOGICAL_AND;
        case BinaryOp::LogicalOr:     return IROp::LOGICAL_OR;
        // Null-coalescing is lowered by visit(BinaryExpr) into CFG blocks;
        // this mapper is only for single-instruction binary operations.
        case BinaryOp::NullCoalesce:  return IROp::GARBAGE;

        case BinaryOp::BitAnd:        return IROp::BIT_AND;
        case BinaryOp::BitOr:         return IROp::BIT_OR;
        case BinaryOp::BitXor:        return IROp::BIT_XOR;
        case BinaryOp::LShift:        return IROp::LEFT_SHIFT;
        case BinaryOp::RShift:        return IROp::RIGHT_SHIFT;
    }

    throw KMYCompileError("Unknown BinaryOp");
}

static IROp unaryOpToIROp(UnaryOp op) {
    switch (op) {
        case UnaryOp::Minus:      return IROp::NEG;
        case UnaryOp::BitNot:     return IROp::BIT_NOT;
        case UnaryOp::LogicalNot: return IROp::LOGICAL_NOT;
        // Force-unwrapping is emitted directly by visit(UnaryExpr) so it can
        // retain its explicit null-trap machine instruction.
        case UnaryOp::ForceUnwrap: return IROp::FORCE_UNWRAP;
    }

    throw KMYCompileError("Unknown UnaryOp");
}

void IRBuilder::visit(BinaryExpr& e) {
    if (e.op != BinaryOp::LogicalAnd && e.op != BinaryOp::LogicalOr &&
        e.op != BinaryOp::NullCoalesce) {
        // Simple case
        e.left->accept(*this);
        HIROperand lhs = getLastValue();
        
        e.right->accept(*this);
        HIROperand rhs = getLastValue();
    
        IRValue dst = makeValue(e.type);
        IRInstr instr{
            .op = binaryOpToIROp(e.op),
            .dst = dst,
            .args = { lhs, rhs }
        };
        emit(instr);
    
        setLastValue(dst);
        return;
    }

    // e.op is && or ||
    if (e.op == BinaryOp::LogicalAnd) {
        /*
        Format for && (simple RHS)
        -----------
        This compact diagram assumes the RHS fits in one block. A nested
        short-circuit RHS may create additional blocks; in that case the
        outer PHI predecessor is the nested RHS's final merge block.

        B0(currBlock):
            ...
            cond = <lhs eval>
            branch cond B1 B2
        
        B1(true):
            ; Using property that true && A == A
            result_B1 = <rhs eval>
            jump B3
        
        B2(false):
            ; Using property that false && A == false
            result_B2 = <false const> 
            jump B3
    
        B3(merge):
            result = phi(result_B1, result_B2)
            ...
        */

        // && short-circuiting
        e.left->accept(*this);
        HIROperand lhs = getLastValue();

        HIRBlock* trueBlock = makeBlock();
        HIRBlock* falseBlock = makeBlock();
        HIRBlock* mergeBlock = makeBlock();

        currCtx->currBlock->term = BranchTerm<IRInstr>{
            .cond = lhs,
            .trueTarget = trueBlock,
            .falseTarget = falseBlock
        };
        connectBlock(currCtx->currBlock, trueBlock);
        connectBlock(currCtx->currBlock, falseBlock);
        
        // True block
        currCtx->currBlock = trueBlock;
        e.right->accept(*this);
        HIROperand rhs = getLastValue();
        /*
        Nested short-circuit example:

            a && (b || c)

        The RHS does not necessarily finish in trueBlock. The inner `||`
        creates its own true/false blocks and a merge block. Therefore:

            WRONG: rhs predecessor = trueBlock
            RIGHT: rhs predecessor = the actual currBlock after RHS lowering

        The PHI must use the actual final RHS block, or it can read the result
        before the nested expression has assigned it.
        */
        HIRBlock* rhsEndBlock = currCtx->currBlock;
        
        rhsEndBlock->term = JumpTerm<IRInstr>{mergeBlock};
        connectBlock(rhsEndBlock, mergeBlock);
        
        // False block
        currCtx->currBlock = falseBlock;
        IRValue falseConst = makeValue(&Types::BOOL_TYPE);
        emit({
            .op = IROp::CONST_INT,
            .dst = falseConst,
            .imm = 0
        });
        
        currCtx->currBlock->term = JumpTerm<IRInstr>{mergeBlock};
        connectBlock(falseBlock, mergeBlock);

        // Merge block
        currCtx->currBlock = mergeBlock;

        IRValue result = makeValue(e.type);
        emit({
            .op = IROp::PHI,
            .dst = result,
            .phis = {
                {rhsEndBlock, rhs},
                {falseBlock, falseConst}
            }
        });

        setLastValue(result);
    } else if (e.op == BinaryOp::LogicalOr || e.op == BinaryOp::NullCoalesce) {
        // `??` is not rewritten into an If AST node. It is represented
        // directly as CFG blocks: branch on the left value, evaluate the
        // fallback only on the null edge, then merge with a PHI.
        /*
        Format for || and ?? (simple RHS)
        -----------
        This compact diagram assumes the RHS fits in one block. Nested
        short-circuit expressions can add blocks between B1/B2 and B3; the
        implementation records those actual final blocks before forming PHI.

        B0(currBlock):
            ...
            cond = <lhs eval>
            branch cond B1 B2
        
        B1(true/non-null):
            ; For ||: true || A == true, so result_B1 = true
            ; For ??: non-null A ?? B == A, so result_B1 = lhs
            result_B1 = true (||) or lhs (??)
            jump B3
        
        B2(false/null):
            ; For ||: false || A == A, so result_B2 = <rhs eval>
            ; For ??: null ?? B == B, so result_B2 = <rhs eval>
            result_B2 = <rhs eval>
            ; Note that RHS may end in a different block (Say BX)
            ; So we ensure we connect BX back to B3(merge)
            jump B3 ; So this only occurs when <rhs eval> doesnt make a new block.

        B3(merge):
            result = phi(result_B1, result_B2)
            ...
        */

        // || short-circuiting
        e.left->accept(*this);
        HIROperand lhs = getLastValue();

        HIRBlock* trueBlock = makeBlock();
        HIRBlock* falseBlock = makeBlock();
        HIRBlock* mergeBlock = makeBlock();

        currCtx->currBlock->term = BranchTerm<IRInstr>{
            .cond = lhs,
            .trueTarget = trueBlock,
            .falseTarget = falseBlock
        };
        connectBlock(currCtx->currBlock, trueBlock);
        connectBlock(currCtx->currBlock, falseBlock);

        // True block
        currCtx->currBlock = trueBlock;

        // This logic is used with ?? short circuiting.
        // A (??, ||) B -> eval A first (lhs)
        // For ||, use true as the value for phi (trueValue = trueConst)
        // For ??, use that A as the value for phi before evaluating B (trueValue = lhs)
        HIROperand trueValue = lhs;
        if (e.op == BinaryOp::LogicalOr) {
            IRValue trueConst = makeValue(&Types::BOOL_TYPE);
            emit({
                .op = IROp::CONST_INT,
                .dst = trueConst,
                .imm = 1
            });
            trueValue = trueConst;
        }

        // Since the block is just a statement, there is only this terminator.
        currCtx->currBlock->term = JumpTerm<IRInstr>{mergeBlock};
        connectBlock(trueBlock, mergeBlock);

        // False block
        currCtx->currBlock = falseBlock;
        e.right->accept(*this);
        HIROperand rhs = getLastValue();
        /*
        For `a || (b && c)` and `a ?? (b && c)`, the RHS may finish in a
        nested merge block rather than falseBlock. Its jump and PHI incoming
        edge must use that actual final block.
        */
        HIRBlock* rhsEndBlock = currCtx->currBlock;

        // Since the block is just a statement, there is only this terminator.
        rhsEndBlock->term = JumpTerm<IRInstr>{mergeBlock};
        connectBlock(rhsEndBlock, mergeBlock);

        // Merge block
        currCtx->currBlock = mergeBlock;
        IRValue result = makeValue(e.type);
        emit({
            .op = IROp::PHI,
            .dst = result,
            .phis = {
                {trueBlock, trueValue},
                {rhsEndBlock, rhs}
            }
        });

        setLastValue(result);
    }
}

void IRBuilder::visit(UnaryExpr& e) {
    // Increment/decrement are read-modify-write expressions.  The lvalue
    // address/identity must be evaluated once: `values[nextIndex()]++` must
    // call nextIndex() once, then load and store that same array element.
    auto lowerIntMutation = [&](IROp arithmetic, bool returnsOldValue) {
        if (!e.operand->isLValue()) {
            throw KMYCompileError("Increment/decrement operator requires an lvalue operand.");
        }

        auto makeUpdatedValue = [&](HIROperand oldValue) {
            IRValue one = makeValue(&Types::INT_TYPE);
            emit({ .op = IROp::CONST_INT, .dst = one, .imm = 1 });

            IRValue updated = makeValue(e.type);
            emit({ .op = arithmetic, .dst = updated, .args = { oldValue, one } });
            return updated;
        };

        // Variable destination: the storage can be an SSA local, heap cell,
        // upvalue cell, or persistent module-global slot.
        if (e.operand->kind == ExprKind::Variable) {
            auto variable = std::static_pointer_cast<Variable>(e.operand);
            if (!variable->symbol->isMutable) {
                throw KMYCompileError("Increment/decrement of constant variable \"" + variable->name + "\"");
            }

            variable->accept(*this);
            HIROperand oldValue = getLastValue();
            IRValue updated = makeUpdatedValue(oldValue);

            if (variable->symbol->isModuleGlobal) {
                emit({
                    .op = IROp::STORE_GLOBAL,
                    .args = { updated },
                    .imm = variable->symbol->moduleGlobalSlot
                });
            } else if (auto upvalue = currCtx->upvalues.find(variable->symbol);
                       upvalue != currCtx->upvalues.end()) {
                emit({ .op = IROp::STORE_CELL, .args = { upvalue->second, updated } });
            } else {
                // Ordinary locals are represented by SSA definitions, not by
                // currCtx->locals storage. Do not use that map to decide how
                // to write: rebinding is the local write operation.
                bindLocalDefinition(variable->symbol, updated);
            }

            setLastValue(returnsOldValue ? oldValue : HIROperand{updated});
            return;
        }

        // Field destination: retain the evaluated receiver before loading,
        // updating, and storing its field at the known byte offset.
        if (e.operand->kind == ExprKind::Get) {
            auto get = std::static_pointer_cast<Get>(e.operand);
            if (get->resolvedMethod || get->obj->type->kind != TypeKind::INSTANCE) {
                throw KMYCompileError("Increment/decrement requires a mutable instance field.");
            }
            auto* instanceType = static_cast<InstanceType*>(get->obj->type);
            auto field = instanceType->fieldMap.find(get->name);
            if (field == instanceType->fieldMap.end() || !field->second->isMutable) {
                throw KMYCompileError("Increment/decrement of constant or unknown field \"" + get->name + "\"");
            }

            get->obj->accept(*this);
            HIROperand object = getLastValue();
            IRValue oldValue = makeValue(e.type);
            emit({ .op = IROp::LOAD_FIELD, .dst = oldValue, .args = { object },
                   .imm = static_cast<int64_t>(get->fieldIdx) * 8 });
            IRValue updated = makeUpdatedValue(oldValue);
            emit({ .op = IROp::STORE_FIELD, .args = { object, updated },
                   .imm = static_cast<int64_t>(get->fieldIdx) * 8 });
            setLastValue(returnsOldValue ? HIROperand{oldValue} : HIROperand{updated});
            return;
        }

        // Indexed destination: retain both array and index before the load so
        // a side-effecting index expression is never evaluated a second time.
        if (e.operand->kind == ExprKind::Index) {
            auto indexExpr = std::static_pointer_cast<Index>(e.operand);
            auto* arrayType = static_cast<ArrayType*>(indexExpr->obj->type);
            size_t elementSize = TypeLayout::sizeOf(arrayType->elementType);
            if (elementSize != PTR_SIZE) {
                throw KMYCompileError("Native array elements must fit in one machine word.");
            }

            indexExpr->obj->accept(*this);
            HIROperand array = getLastValue();
            indexExpr->index->accept(*this);
            HIROperand index = getLastValue();
            IRValue oldValue = makeValue(e.type);
            emit({ .op = IROp::LOAD_ARRAY_INDEX, .dst = oldValue, .args = { array, index },
                   .imm = static_cast<int64_t>(elementSize) });
            IRValue updated = makeUpdatedValue(oldValue);
            emit({ .op = IROp::STORE_ARRAY_INDEX, .args = { array, index, updated },
                   .imm = static_cast<int64_t>(elementSize) });
            setLastValue(returnsOldValue ? HIROperand{oldValue} : HIROperand{updated});
            return;
        }

        // Dereferenced destination: evaluate the pointer once, then perform
        // the read-modify-write sequence through that same pointer.
        if (e.operand->kind == ExprKind::UnaryExpr) {
            auto dereference = std::static_pointer_cast<UnaryExpr>(e.operand);
            if (dereference->op == UnaryOp::Dereference) {
                dereference->operand->accept(*this);
                HIROperand pointer = getLastValue();
                IRValue oldValue = makeValue(e.type);
                emit({ .op = IROp::LOAD_INDIRECT, .dst = oldValue, .args = { pointer } });
                IRValue updated = makeUpdatedValue(oldValue);
                emit({ .op = IROp::STORE_INDIRECT, .args = { pointer, updated } });
                setLastValue(returnsOldValue ? HIROperand{oldValue} : HIROperand{updated});
                return;
            }
        }

        throw KMYCompileError("Unsupported increment/decrement lvalue.");
    };

    if (e.op == UnaryOp::PreIncrement) {
        // ++x: mutate x, then the expression yields the new value.
        lowerIntMutation(IROp::ADD, false);
        return;
    } else if (e.op == UnaryOp::PreDecrement) {
        // --x: mutate x, then the expression yields the new value.
        lowerIntMutation(IROp::SUB, false);
        return;
    } else if (e.op == UnaryOp::PostIncrement) {
        // x++: mutate x, but the expression yields its old value.
        lowerIntMutation(IROp::ADD, true);
        return;
    } else if (e.op == UnaryOp::PostDecrement) {
        // x--: mutate x, but the expression yields its old value.
        lowerIntMutation(IROp::SUB, true);
        return;
    } else if (e.op == UnaryOp::AddressOf) {
        // Taking the address must not first load the lvalue's value.
        if (e.operand->kind == ExprKind::Variable) {
            auto var = std::static_pointer_cast<Variable>(e.operand);

            if (var->symbol->isModuleGlobal) {
                // Module globals live in kmy_globals rather than an SSA local
                // stack slot. Keep that storage class explicit so SSA never
                // tries to find a nonexistent local definition for &global.
                IRValue dst = makeValue(e.type);
                emit({
                    .op = IROp::GET_GLOBAL_ADDR,
                    .dst = dst,
                    .imm = var->symbol->moduleGlobalSlot
                });
                setLastValue(dst);
                return;
            }

            auto upvalueIt = currCtx->upvalues.find(var->symbol);
            if (upvalueIt != currCtx->upvalues.end()) {
                // Captured variables already live in heap cells; the cell
                // pointer is the address that '&' should expose.
                setLastValue(upvalueIt->second);
                return;
            }

            // Ordinary locals are intentionally represented as VarRef until
            // SSA renaming. Keep the symbol explicit here instead of asking
            // the transient local-value map for a definition that may not
            // exist yet.
            auto localIt = currCtx->locals.find(var->symbol);
            if (localIt != currCtx->locals.end() && localIt->second.isCell) {
                // A local promoted to a cell is already represented by its
                // cell pointer, so no extra address instruction is needed.
                setLastValue(localIt->second.value);
                return;
            }

            IRValue dst = makeValue(e.type);
            emit({
                .op = IROp::GET_ADDR,
                .dst = dst,
                .args = { VarRef{var->symbol} }
            });
            setLastValue(dst);
            return;
        }

        if (e.operand->kind == ExprKind::Get) {
            auto get = std::static_pointer_cast<Get>(e.operand);
            if (get->resolvedMethod || get->obj->type->kind != TypeKind::INSTANCE) {
                throw KMYCompileError("Address-of requires a native field lvalue");
            }

            get->obj->accept(*this);
            IRValue dst = makeValue(e.type);
            emit({
                .op = IROp::GET_ADDR,
                .dst = dst,
                .args = { getLastValue() },
                .imm = static_cast<int64_t>(get->fieldIdx) * 8
            });
            setLastValue(dst);
            return;
        }

        if (e.operand->kind == ExprKind::Index) {
            auto indexExpr = std::static_pointer_cast<Index>(e.operand);
            if (indexExpr->obj->type->kind != TypeKind::ARRAY) {
                throw KMYCompileError("Address-of requires an array lvalue");
            }

            auto* arrayType = static_cast<ArrayType*>(indexExpr->obj->type);
            size_t elementSize = TypeLayout::sizeOf(arrayType->elementType);
            if (elementSize != PTR_SIZE) {
                throw KMYCompileError("Native array elements must fit in one machine word.");
            }

            indexExpr->obj->accept(*this);
            HIROperand array = getLastValue();
            indexExpr->index->accept(*this);
            HIROperand index = getLastValue();

            IRValue dst = makeValue(e.type);
            emit({
                .op = IROp::GET_ADDR,
                .dst = dst,
                .args = { array, index },
                .imm = static_cast<int64_t>(elementSize)
            });
            setLastValue(dst);
            return;
        }

        if (e.operand->kind == ExprKind::UnaryExpr) {
            auto nested = std::static_pointer_cast<UnaryExpr>(e.operand);
            if (nested->op == UnaryOp::Dereference) {
                // &*p is the original pointer; no memory access is needed.
                nested->operand->accept(*this);
                return;
            }
        }

        throw KMYCompileError("Address-of requires an addressable expression");
    }

    e.operand->accept(*this);
    HIROperand v = getLastValue();

    if (e.op == UnaryOp::ForceUnwrap) {
        IRValue dst = makeValue(e.type);
        emit({ .op = IROp::FORCE_UNWRAP, .dst = dst, .args = { v } });
        setLastValue(dst);
        return;
    }

    if (e.op == UnaryOp::Dereference) {
        IRValue dst = makeValue(e.type);
        emit({ .op = IROp::LOAD_INDIRECT, .dst = dst, .args = { v } });
        setLastValue(dst);
        return;
    }

    IRValue dst = makeValue(e.type);

    IRInstr instr{
        .op = unaryOpToIROp(e.op),
        .dst = dst,
        .args = { v }
    };
    emit(instr);

    setLastValue(dst);
}

void IRBuilder::visit(Assignment& e) {
    // Case 1: variable assignment
    assert(e.left->isLValue());

    // Pointer dereference assignment. The dereference expression is already
    // an lvalue, so its operand evaluates to the destination address.
    if (e.left->kind == ExprKind::UnaryExpr) {
        auto unary = std::static_pointer_cast<UnaryExpr>(e.left);
        if (unary->op != UnaryOp::Dereference) {
            throw KMYCompileError("Invalid unary assignment target");
        }

        unary->operand->accept(*this);
        HIROperand pointer = getLastValue();

        HIROperand value;
        if (e.op == AssignmentOp::Assign) {
            e.right->accept(*this);
            value = getLastValue();
        } else {
            IRValue oldValue = makeValue(e.type);
            emit({
                .op = IROp::LOAD_INDIRECT,
                .dst = oldValue,
                .args = { pointer }
            });

            e.right->accept(*this);
            IRValue newValue = makeValue(e.type);
            emit({
                .op = binaryOpToIROp(compoundToBinaryOp(e.op)),
                .dst = newValue,
                .args = { oldValue, getLastValue() }
            });
            value = newValue;
            setLastValue(newValue);
        }

        emit({
            .op = IROp::STORE_INDIRECT,
            .args = { pointer, value }
        });
        return;
    }

    if (e.left->kind == ExprKind::Variable) {
        auto var = std::static_pointer_cast<Variable>(e.left);
        if (!var->symbol->isMutable) {
            throw KMYCompileError("Assignment to a constant variable \"" + var->name + "\"");
        }
        
        if (e.op != AssignmentOp::Assign) {
            e.left->accept(*this);
            HIROperand lhs = getLastValue();
            e.right->accept(*this);
            HIROperand rhs = getLastValue();
            
            IRValue dst = makeValue(e.type);
            IRInstr instr{
                .op = binaryOpToIROp(compoundToBinaryOp(e.op)),
                .dst = dst,
                .args = { lhs, rhs }
            };
            emit(instr);

            setLastValue(dst);
        } else {
            e.right->accept(*this);
            // last value isnt updated.
            // In a = 42, the last value is RHS.
        }

        if (var->symbol->isModuleGlobal) {
            emit({
                .op = IROp::STORE_GLOBAL,
                .args = { getLastValue() },
                .imm = var->symbol->moduleGlobalSlot
            });
            return;
        }

        // The locals must already contain the symbol.
        // Though this may never happen.

        // UNLESS it is an upvalue!
        // Emit SET_ENV for mutable upvalues (like x = 3 where x is not local.)
        std::cout << "[assignment] check: " << var->symbol << std::endl;
        if (currCtx->locals.count(var->symbol)) {
            std::cout << "[assignment] check: " << var->symbol << " exists in locals. this is an local cell mutation.\n";
            IRLocalInfo* varInfo = getValueFromLocal(currCtx, var->symbol);
            if (varInfo && varInfo->isCell) {
                emit({
                    .op = IROp::STORE_CELL,
                    .args = {
                        varInfo->value,
                        getLastValue()
                    }
                });
                return;
            }
            // Normal SSA local. (????????)
            bindLocalDefinition(
                var->symbol,
                getLastValue()
            );

            return;
        } else if (currCtx->upvalues.count(var->symbol) == 0) {
            // non cell mutation.
            /*
            HIROperand v = getLastValue();
            //currCtx->locals[var->symbol].value = v;
            currCtx->currBlock->defs.push_back({var->symbol, v});
            return;
            */

            // Normal SSA-backed local mutation.
            HIROperand v = getLastValue();

            bindLocalDefinition(var->symbol, v);

            return;
        }
        
        // Upvalue mutation
        assert(currCtx->incomingEnv.has_value());
        
        // Should have a parent that provides addr to upvalue.
        std::cout << var->name << std::endl;
        std::cout << "= " << getLastValue() << std::endl;
        assert(currCtx->parent);

        IRValue cell = currCtx->upvalues[var->symbol];

        emit({
            .op = IROp::STORE_CELL,
            .args = {
                cell,
                getLastValue()
            }
        });

        return;
    }

    // Case 2: aggregate field assignment
    if (e.left->kind == ExprKind::Get) {
        auto get = std::static_pointer_cast<Get>(e.left);
        if (get->resolvedMethod) {
            throw KMYCompileError("Cannot assign to a method.");
        }

        if (get->obj->type->kind != TypeKind::INSTANCE) {
            throw KMYCompileError("Native field assignment currently requires a class/record instance.");
        }

        auto* instanceType = static_cast<InstanceType*>(get->obj->type);
        auto fieldIt = instanceType->fieldMap.find(get->name);
        if (!e.isInitialisation && fieldIt != instanceType->fieldMap.end() &&
            !fieldIt->second->isMutable) {
            throw KMYCompileError("Assignment to a constant field \"" + get->name + "\"");
        }

        get->obj->accept(*this);
        HIROperand object = getLastValue();

        HIROperand value;
        if (e.op == AssignmentOp::Assign) {
            e.right->accept(*this);
            value = getLastValue();
        } else {
            IRValue oldValue = makeValue(get->type);
            emit({
                .op = IROp::LOAD_FIELD,
                .dst = oldValue,
                .args = { object },
                .imm = static_cast<int64_t>(get->fieldIdx) * 8
            });

            e.right->accept(*this);
            IRValue newValue = makeValue(get->type);
            emit({
                .op = binaryOpToIROp(compoundToBinaryOp(e.op)),
                .dst = newValue,
                .args = { oldValue, getLastValue() }
            });
            value = newValue;
            setLastValue(newValue);
        }

        emit({
            .op = IROp::STORE_FIELD,
            .args = { object, value },
            .imm = static_cast<int64_t>(get->fieldIdx) * 8
        });
        return;
    }

    // Case 3: fixed-array element assignment
    if (e.left->kind == ExprKind::Index) {
        auto indexExpr = std::static_pointer_cast<Index>(e.left);
        if (indexExpr->obj->type->kind != TypeKind::ARRAY) {
            throw KMYCompileError("Native indexing requires an array.");
        }

        auto* arrayType = static_cast<ArrayType*>(indexExpr->obj->type);
        size_t elementSize = TypeLayout::sizeOf(arrayType->elementType);
        if (elementSize != PTR_SIZE) {
            throw KMYCompileError("Native array elements must fit in one machine word.");
        }

        indexExpr->obj->accept(*this);
        HIROperand array = getLastValue();
        indexExpr->index->accept(*this);
        HIROperand index = getLastValue();

        HIROperand value;
        if (e.op == AssignmentOp::Assign) {
            e.right->accept(*this);
            value = getLastValue();
        } else {
            IRValue oldValue = makeValue(e.type);
            emit({
                .op = IROp::LOAD_ARRAY_INDEX,
                .dst = oldValue,
                .args = { array, index },
                .imm = static_cast<int64_t>(elementSize)
            });
            e.right->accept(*this);
            IRValue newValue = makeValue(e.type);
            emit({
                .op = binaryOpToIROp(compoundToBinaryOp(e.op)),
                .dst = newValue,
                .args = { oldValue, getLastValue() }
            });
            value = newValue;
            setLastValue(newValue);
        }

        emit({
            .op = IROp::STORE_ARRAY_INDEX,
            .args = { array, index, value },
            .imm = static_cast<int64_t>(elementSize)
        });
        return;
    }

    throw KMYCompileError("Invalid assignment target");
}

void IRBuilder::visit(Index& e) {
    if (e.obj->type->kind != TypeKind::ARRAY) {
        throw KMYCompileError("Native indexing requires an array.");
    }

    auto* arrayType = static_cast<ArrayType*>(e.obj->type);
    size_t elementSize = TypeLayout::sizeOf(arrayType->elementType);
    if (elementSize != PTR_SIZE) {
        throw KMYCompileError("Native array elements must fit in one machine word.");
    }

    e.obj->accept(*this);
    HIROperand array = getLastValue();
    e.index->accept(*this);
    HIROperand index = getLastValue();

    IRValue value = makeValue(e.type);
    emit({
        .op = IROp::LOAD_ARRAY_INDEX,
        .dst = value,
        .args = { array, index },
        .imm = static_cast<int64_t>(elementSize)
    });
    setLastValue(value);
}

void IRBuilder::visit(Call& e) {
    if (e.func->kind == ExprKind::Variable) {
        auto builtin = std::static_pointer_cast<Variable>(e.func);
        // Lower only the actual registered builtin symbol. A user-defined
        // function that shadows a builtin name must remain an ordinary call.
        if (builtin->symbol && builtin->symbol->nativeFnPtr &&
            (builtin->name == "streq" || builtin->name == "strconcat" ||
            builtin->name == "strlen" || builtin->name == "strByteAt" ||
            builtin->name == "strFromByte" || builtin->name == "readFile" ||
            builtin->name == "writeFile" || builtin->name == "malloc" ||
            builtin->name == "free")) {
            std::vector<HIROperand> args;
            for (const auto& arg : e.args) {
                arg->accept(*this);
                args.push_back(getLastValue());
            }

            IROp op = IROp::STRING_EQUAL;
            if (builtin->name == "strconcat") op = IROp::STRING_CONCAT;
            if (builtin->name == "strlen") op = IROp::STRING_LENGTH;
            if (builtin->name == "strByteAt") op = IROp::STRING_BYTE_AT;
            if (builtin->name == "strFromByte") op = IROp::STRING_FROM_BYTE;
            if (builtin->name == "readFile") op = IROp::FILE_READ;
            if (builtin->name == "writeFile") op = IROp::FILE_WRITE;
            if (builtin->name == "malloc") op = IROp::MALLOC_BYTES;
            if (builtin->name == "free") op = IROp::FREE;

            // free returns void, so emitting a destination would create an
            // invalid SSA value for an expression that produces no value.
            if (op == IROp::FREE) {
                emit({ .op = op, .args = std::move(args) });
                return;
            }

            IRValue result = makeValue(e.type);
            emit({
                .op = op,
                .dst = result,
                .args = std::move(args)
            });
            setLastValue(result);
            return;
        }
    }

    std::optional<IRValue> v = std::nullopt;
    e.func->accept(*this);

    std::vector<HIROperand> args = { getLastValue() }; // First is always the function value.
    for (const auto& arg : e.args) {
        arg->accept(*this);
        args.push_back( getLastValue() );
    }

    // MethodLower appends the implicit receiver after the explicit arguments
    // (the bytecode compiler consumes arguments in reverse order).  Native x64
    // calls pass arguments left-to-right in registers, so move that receiver
    // directly after the closure/env operand before lowering to MIR.
    if (e.func->kind == ExprKind::Get) {
        auto callee = std::static_pointer_cast<Get>(e.func);
        if (callee->resolvedMethod && args.size() > 1) {
            HIROperand receiver = args.back();
            args.pop_back();
            args.insert(args.begin() + 1, receiver);
        }
    }

    if (e.type != &Types::VOID_TYPE) {
        v = makeValue(e.type);
    }

    IRInstr instr{
        .op = IROp::CALL,
        .dst = v,
        .args = std::move(args)
    };
    emit(instr);
    
    // NOTE: In prev semantic passes, it is guaranteed that void isn't stored anywhere.
    if (e.type != &Types::VOID_TYPE) {
        setLastValue(v.value());
    }
}

void IRBuilder::visit(Get& e) {
    if (e.obj->type->kind != TypeKind::INSTANCE) {
        throw KMYCompileError("Native field access currently requires a class/record instance.");
    }

    if (e.resolvedMethod) {
        auto* instanceType = static_cast<InstanceType*>(e.obj->type);
        auto methodIt = instanceType->methodMap.find(e.name);
        if (methodIt == instanceType->methodMap.end()) {
            throw KMYCompileError("Method metadata missing for native codegen.");
        }
        if (methodIt->second->nativeFunctionId == INVALID_SLOT) {
            throw KMYCompileError("Method native function label missing for native codegen.");
        }

        IRValue closure = makeValue(e.type);
        HIROperand env;
        if (currCtx->env.has_value()) {
            env = currCtx->env.value();
        } else {
            // Null env (no captured value)
            // TODO to like const_null
            IRValue nullEnv = makeValue(new PointerType(&Types::VOID_TYPE));
            emit({ .op = IROp::CONST_NULL, .dst = nullEnv });
            env = nullEnv;
        }
        emit({
            .op = IROp::FUNC_LABEL,
            .dst = closure,
            .args = { env },
            .imm = methodIt->second->nativeFunctionId
        });
        setLastValue(closure);
        return;
    }

    e.obj->accept(*this);
    HIROperand object = getLastValue();
    IRValue value = makeValue(e.type);

    emit({
        .op = IROp::LOAD_FIELD,
        .dst = value,
        .args = { object },
        .imm = static_cast<int64_t>(e.fieldIdx) * 8
    });
    setLastValue(value);
}

void IRBuilder::visit(ScopeAccessExpr& e) {
    // A qualified imported function is compiled like a normal function value:
    // make a closure whose code pointer is the globally unique function label.
    // Module globals are not implemented, so imported functions must not rely
    // on captured module state yet; their environment is therefore null.
    if (e.symbol) {
        IRValue value = makeValue(e.type);
        emit({ .op = IROp::LOAD_GLOBAL, .dst = value, .imm = e.symbol->moduleGlobalSlot });
        setLastValue(value);
        return;
    }

    // Enum variants are compile-time ordinal constants (RED = 0, GREEN = 1,
    // ...).  Keep the enum type on the IR value while using the native integer
    // representation for MIR/x86 storage and comparisons.
    IRValue value = makeValue(e.type);
    emit({
        .op = IROp::CONST_INT,
        .dst = value,
        .imm = e.accessIdx
    });
    setLastValue(value);
}

static void printFunctionContext(FunctionContext* fnCtx) {
    if (!fnCtx) {
        std::cout << "<null function context>\n";
        return;
    }

    std::cout << "========================================\n";
    std::cout << "FunctionContext @" << fnCtx << "\n";

    //
    // Locals
    //
    std::cout << "\nLocals (" << fnCtx->locals.size() << ")\n";
    std::cout << "----------------------------------------\n";

    for (const auto& local : fnCtx->locals) {
        std::cout
            << "  "
            << local.sym->name
            << "  slot=" << local.slot
            << "  depth=" << local.scopeDepth
            << "  captured=" << std::boolalpha << local.captured
            << "\n";
    }

    //
    // Upvalues
    //
    std::cout << "\nUpvalues (" << fnCtx->upvalues.size() << ")\n";
    std::cout << "----------------------------------------\n";

    for (int i = 0; i < fnCtx->upvalues.size(); ++i) {
        const auto& up = fnCtx->upvalues[i];

        std::cout
            << "  ["
            << i
            << "] "
            << up.symbol->name
            << "  parentSlot=" << up.index
            << "  isLocal=" << std::boolalpha << up.isLocal
            << "  capturedByChildren="
            << up.capturedByChildren
            << "\n";
    }

    //
    // Environment layout
    //
    std::cout << "\nEnvironment (" << fnCtx->envMap.size()
              << " slots, size=" << fnCtx->envSize << ")\n";
    std::cout << "----------------------------------------\n";

    for (const auto& [sym, slot] : fnCtx->envMap) {
        std::cout
            << "  slot " << slot
            << " -> " << sym->name
            << "\n";
    }

    //
    // Local map
    //
    std::cout << "\nLocalMap\n";
    std::cout << "----------------------------------------\n";

    for (const auto& [sym, slot] : fnCtx->localMap) {
        std::cout
            << "  "
            << sym->name
            << " -> " << slot
            << "\n";
    }

    //
    // Upvalue map
    //
    std::cout << "\nUpvalueMap\n";
    std::cout << "----------------------------------------\n";

    for (const auto& [sym, slot] : fnCtx->upvalueMap) {
        std::cout
            << "  "
            << sym->name
            << " -> " << slot
            << "\n";
    }

    std::cout << "========================================\n";
}

void IRBuilder::visit(FunctionExpr& e) {
    std::cout << "NEW FUNC " << e.functionId << "\n";
    printFunctionContext(e.functionContext);

    // Context switch to a new function.
    auto oldCtx = currCtx;
    HIRFunction* oldFn = currFunc;

    // Named function declarations already have a closure emitted by the
    // enclosing function's hoisting pass. Function expressions, however,
    // must materialize a closure at their evaluation site so their value can
    // be returned, stored, or called.
    bool isHoistedDeclaration = false;
    std::optional<IRValue> expressionValue;
    if (oldCtx) {
        for (const auto& [symbol, declaration] : oldCtx->fnCtx->funcDecls) {
            (void)symbol;
            if (declaration.get() == &e) {
                isHoistedDeclaration = true;
                break;
            }
        }

        if (!isHoistedDeclaration && !compilingAggregateMember) {
            IRValue fnValue = makeValue(e.type);
            std::vector<HIROperand> args;

            if (oldCtx->env.has_value()) {
                args.push_back(oldCtx->env.value());
            } else {
                IRValue nullEnv = makeValue(new PointerType(&Types::VOID_TYPE));
                emit({
                    .op = IROp::CONST_NULL,
                    .dst = nullEnv
                });
                args.push_back(nullEnv);
            }

            emit({
                .op = IROp::FUNC_LABEL,
                .dst = fnValue,
                .args = std::move(args),
                .imm = e.functionId
            });

            expressionValue = fnValue;
        }
    }

    currCtx = new IRCodegenFnCtx{};
    currCtx->parent = oldCtx;   
    currCtx->fnCtx = e.functionContext;

    currFunc = new HIRFunction{ 
        .functionId = e.functionId, 
        .funcType = static_cast<FunctionType*>(e.type),
        .isEntryFunc = e.isEntry,
    };

    HIRBlock* entry = makeBlock();
    // entry has no preds!

    currFunc->entry = entry;
    currCtx->currBlock = entry;

    // The entry module's legacy root is its initializer/main for now. Run all
    // dependency initializers before evaluating its own top-level statements.
    if (&e == moduleInitializer) {
        for (int initializerId : initializerIds) {
            IRValue nullEnv = makeValue(new PointerType(&Types::VOID_TYPE));
            emit({ .op = IROp::CONST_NULL, .dst = nullEnv });
            IRValue closure = makeValue(new PointerType(&Types::VOID_TYPE));
            emit({ .op = IROp::FUNC_LABEL, .dst = closure, .args = { nullEnv }, .imm = initializerId });
            emit({ .op = IROp::CALL, .args = { closure } });
        }
    }

    // Incoming env from parent
    // Only introduce env if there is a free variable in the function
    // i.e., if upvalues are present.
    std::optional<IRValue> incomingEnv;
    if (!e.functionContext->upvalues.empty()) {
        incomingEnv = makeValue(new PointerType(&Types::VOID_TYPE));
        currCtx->incomingEnv = incomingEnv; // Unused?

        IRInstr instr{
            .op  = IROp::INTRODUCE_ENV,
            .dst = incomingEnv.value()
        };

        emit(instr);
    }


    // Introduce params
    // NOTE: This must happen at the very beginning of the function.

    struct CapturedParamInfo {
        VarSymbol* sym;
        IRValue v;
        int envSlot;
    };

    std::vector<CapturedParamInfo> capturedParams;

    for (int i = 0; i < e.params.size(); i++) {
        IRValue v = makeValue(e.params[i].symbol->type);
        // Allow locals (function body) to see this param.
        
        //currCtx->locals[e.params[i].symbol] = IRLocalInfo{ .value = v, .isCell = false };


        // Check whether the parameter is captured.
        // (Same logic as Let stmt visit)
        auto it = currCtx->fnCtx->envMap.find(e.params[i].symbol);
        if (it != currCtx->fnCtx->envMap.end()) {
            // This param needs to be SET_ENV'ed!
            // Since currCtx->env is not present here, we delegate it.
            capturedParams.push_back({e.params[i].symbol, v, it->second});
        }

        // Instructing the function to use the param
        /*
        main
        -------
        B0:
            v0 = 1
            v1 = 2
            v2 = CALL v0, v1 

        add
        -------
        B0:
            v0 = PARAM 0
            v1 = PARAM 1
            v2 = ADD v0, v1
            PRINT v2
        */
        IRInstr instr {
            .op = IROp::PARAM,
            .dst = v,
            .imm = i,
            .defSym = e.params[i].symbol
        };
        emit(instr);
    }

    // Allocate my env (for children) if children capture my locals or upvalues
    if (e.functionContext->envSize > 0) {
        IRValue env = makeValue(new PointerType(&Types::VOID_TYPE));

        currCtx->env = env;

        IRInstr instr{
            .op  = IROp::ALLOC_ENV,
            .dst = env,
            .imm = e.functionContext->envSize
        };

        emit(instr);
    }

    // Now SET_ENV the captured params
    if (currCtx->env) {
        for (auto [sym, v, envSlot] : capturedParams) {
    
            IRValue cell = makeValue(
                new CellType(sym->type)
            );
    
            emit({
                .op = IROp::ALLOC_CELL_INIT,
                .dst = cell,
                .args = { v }
            });
    
            emit({
                .op = IROp::SET_ENV,
                .args = { currCtx->env.value(), cell },
                .imm = envSlot
            });
    
            // IMPORTANT! Overwrite local(param) reads with cell reads.
            currCtx->locals[sym] = IRLocalInfo{ .value = cell, .isCell = true };
        }            
    }

    // At this point, all params must have been assigned as locals
    // either as a cell or just a normal local.
    for (const auto& param: e.params) {
        IRLocalInfo& paramInfo = currCtx->locals[param.symbol];
        if (!paramInfo.isCell) {
            currCtx->currBlock->defs.push_back({param.symbol, paramInfo.value});
        }
    }


    // Introduce upvalues (They are accessed via incoming env with given env slot.)
    // They are assigned on function entry from its env.
    for (int i = 0; i < e.functionContext->upvalues.size(); i++) {
        auto& up = e.functionContext->upvalues[i];
        IRValue v = makeValue(new CellType(up.symbol->type));

        currCtx->upvalues[up.symbol] = v; // Needed for varaible upvalue lookup.

        // Having upvalue means incoming env and parent exists!
        assert(e.functionContext->parent != nullptr); 
        assert(e.functionContext->parent->envMap.count(up.symbol) > 0);
        IRInstr instr {
            .op = IROp::GET_ENV,    
            .dst = v,
            .args = { incomingEnv.value() },
            .imm = e.functionContext->parent->envMap.at(up.symbol) 
        };

        emit(instr);

        if (up.capturedByChildren) {
            std::cout << "Re-exporting upvalue captured by child closure\n";
            // Children captures MY upvalue
            // We need to put it in env so that the children can see my upvalues.
            
            assert(e.functionContext->envMap.count(up.symbol) > 0);
            
            // Here, v is already a pointer from parent's env. so a copy should occur.
            // This is essentially flattening environment.
            IRInstr instr {
                .op = IROp::SET_ENV,    
                .args = { currCtx->env.value(), v },
                .imm = e.functionContext->envMap.at(up.symbol)
            };

            emit(instr);
        }
    }

    // Hoist function declarations (in opposite order?)
    for (auto& [funcDeclSym, funcDeclExpr] : e.functionContext->funcDecls) {
        IRValue fnValue = makeValue(funcDeclExpr->type);
        //assert(currCtx->env.has_value());

        std::vector<HIROperand> arg = {};
        if (currCtx->env.has_value()) { arg.push_back(currCtx->env.value()); }
        else { 
            IRValue nullValue = makeValue(new PointerType(&Types::VOID_TYPE));
            emit({
                .op = IROp::CONST_NULL,
                .dst = nullValue
            });
            arg.push_back(nullValue); 
        }

        IRInstr instr {
            .op = IROp::FUNC_LABEL,
            .dst = fnValue, 
            .args = arg, // Env to use
            .imm = funcDeclExpr->functionId, // TODO: Don't store the whole expr...
            .defSym = funcDeclSym // TODO: Remove hoisting...
        };
        emit(instr);

        if (funcDeclSym->isModuleGlobal) {
            emit({ .op = IROp::STORE_GLOBAL, .args = { fnValue }, .imm = funcDeclSym->moduleGlobalSlot });
            continue;
        }
        
        //funcDeclExpr->accept(*this);

        // Quirky! TODO!!!
        bool uncapturedLocal = false;
        if (currCtx->locals.find(funcDeclSym) == currCtx->locals.end()) {
            //??????
            //currCtx->locals[funcDeclSym] = IRLocalInfo{ .value = fnValue, .isCell = false };
            uncapturedLocal = true;
        } else {
            std::cout << "Duplicate write prevented which is not very desired.\n";
        }

        auto it = currCtx->fnCtx->envMap.find(funcDeclSym);
        
        // If the hoisted function (closure) itself is needed from children closure,
        // Set it on env (func/closure is also a pointer, so copy occurs.)
        if (it != currCtx->fnCtx->envMap.end()) {
            assert(currCtx->env.has_value());
            std::cout << "Hoisted function " << funcDeclSym->name << " is captured by children. Setting it on env.\n";
            
            IRValue cell = makeValue(
                new CellType(e.type)
            );
    
            emit({
                .op = IROp::ALLOC_CELL_INIT,
                .dst = cell,
                .args = { fnValue }
            });
    
            emit({
                .op = IROp::SET_ENV,
                .args = { currCtx->env.value(), cell },
                .imm = it->second
            });
            uncapturedLocal = false;
        }

        if (uncapturedLocal) {
            currCtx->currBlock->defs.push_back({funcDeclSym, fnValue});
        }
    }

    std::cout << "env\n";
    for (auto& [sym, slot] : e.functionContext->envMap) {
        std::cout << sym->name << " -> " << slot << "\n";
    }
    std::cout << "locals so far\n";
    for (auto& [sym, v] : currCtx->locals) {
        std::cout << sym->name << " -> " << v.value << " (isCell: " << v.isCell << ")\n";
    }

    
    // body
    e.body->accept(*this);    

    // implicit return
    if (!hasTerminator(currCtx->currBlock)) {
        if (e.isConstructor && !e.params.empty()) {
            currCtx->currBlock->term = ReturnTerm{
                currCtx->locals.at(e.params[0].symbol).value
            };
        } else {
            currCtx->currBlock->term = ReturnTerm{};
        }
    }

    currFunc->lastValueId = currCtx->nextId;
    std::cout << "This function " << currFunc->functionId << " lastVId: " << currFunc->lastValueId << "\n";
    functions.push_back(currFunc);

    // Prevent currCtx from being nullptr (top level entry func)
    if (oldCtx) {
        currFunc = oldFn;
        currCtx = oldCtx;
        if (expressionValue.has_value()) {
            setLastValue(expressionValue.value());
        }
    }
}

void IRBuilder::visit(ThisExpr& e) {
    if (auto it = currCtx->upvalues.find(e.symbol); it != currCtx->upvalues.end()) {
        IRValue loaded = makeValue(e.type);
        emit({
            .op = IROp::LOAD_CELL,
            .dst = loaded,
            .args = { it->second }
        });
        setLastValue(loaded);
        return;
    }

    auto it = currCtx->locals.find(e.symbol);
    if (it == currCtx->locals.end()) {
        throw KMYCompileError("Unable to resolve 'this' in native codegen");
    }

    if (it->second.isCell) {
        IRValue loaded = makeValue(e.type);
        emit({
            .op = IROp::LOAD_CELL,
            .dst = loaded,
            .args = { it->second.value }
        });
        setLastValue(loaded);
    } else {
        setLastValue(it->second.value);
    }
}

void IRBuilder::visit(NewExpr& e) {
    if (e.arrayType) {
        auto* arrayType = static_cast<ArrayType*>(e.type);
        size_t elementSize = TypeLayout::sizeOf(arrayType->elementType);
        if (elementSize != PTR_SIZE) {
            throw KMYCompileError("Native array elements must fit in one machine word.");
        }

        IRValue array = makeValue(e.type);
        if (e.arraySize) {
            e.arraySize->accept(*this);
            emit({
                .op = IROp::ALLOC_ARRAY_DYNAMIC,
                .dst = array,
                .args = { getLastValue() },
                .imm = static_cast<int64_t>(elementSize)
            });
            setLastValue(array);
            return;
        }
        if (!arrayType->fixedLength.has_value()) {
            throw KMYCompileError("Native array allocation requires a size.");
        }
        emit({
            .op = IROp::ALLOC_ARRAY,
            .dst = array,
            .imm = static_cast<int64_t>(*arrayType->fixedLength * elementSize)
        });
        setLastValue(array);
        return;
    }

    if (e.type->kind != TypeKind::INSTANCE) {
        throw KMYCompileError("new applied to a non-instance type");
    }

    auto* instanceType = static_cast<InstanceType*>(e.type);
    size_t fieldCount = instanceType->fieldMap.size();
    size_t objectSize = std::max<size_t>(PTR_SIZE, fieldCount * PTR_SIZE);

    IRValue object = makeValue(e.type);
    emit({
        .op = IROp::ALLOC_HEAP,
        .dst = object,
        .imm = static_cast<int64_t>(objectSize)
    });

    // Zero fields before running source-level initializers.
    // for (auto& [name, field] : instanceType->fieldMap) {
    //     (void)name;
    //     IRValue zero = makeValue(&Types::INT_TYPE);
    //     emit({ .op = IROp::CONST_INT, .dst = zero, .imm = 0 });
    //     emit({
    //         .op = IROp::STORE_FIELD,
    //         .args = { object, zero },
    //         .imm = static_cast<int64_t>(field->fieldOffset) * 8
    //     });
    // }

    auto emitClosure = [&](Type* functionType, int functionId) {
        if (functionId == INVALID_SLOT) {
            throw KMYCompileError("Native function label missing for aggregate construction.");
        }
        IRValue closure = makeValue(functionType);
        HIROperand env;
        if (currCtx->env.has_value()) {
            env = currCtx->env.value();
        } else {
            IRValue nullEnv = makeValue(new PointerType(&Types::VOID_TYPE));
            emit({ .op = IROp::CONST_NULL, .dst = nullEnv });
            env = nullEnv;
        }
        emit({
            .op = IROp::FUNC_LABEL,
            .dst = closure,
            .args = { env },
            .imm = functionId
        });
        return closure;
    };

    if (!instanceType->fieldInitializerType ||
        instanceType->fieldInitializerFunctionId == INVALID_SLOT) {
        throw KMYCompileError("Field initializer prototype missing for native codegen.");
    }
    IRValue fieldInit = emitClosure(
        instanceType->fieldInitializerType,
        instanceType->fieldInitializerFunctionId
    );
    IRValue fieldInitResult = makeValue(&Types::VOID_TYPE);
    emit({
        .op = IROp::CALL,
        .dst = fieldInitResult,
        .args = { fieldInit, object }
    });

    VarSymbol* constructor = nullptr;
    for (auto* constructorSymbol : instanceType->constructorVec) {
        auto* fnType = static_cast<FunctionType*>(constructorSymbol->type);
        if (fnType->paramTypes.size() == e.args.size() + 1) {
            if (constructorSymbol->nativeFunctionId != INVALID_SLOT) {
                constructor = constructorSymbol;
                break;
            }
        }
    }
    if (!constructor) {
        throw KMYCompileError("Cannot find a constructor matching native new expression.");
    }

    IRValue constructorClosure = emitClosure(
        constructor->type,
        constructor->nativeFunctionId
    );
    std::vector<HIROperand> callArgs = { constructorClosure, object };
    for (auto& arg : e.args) {
        arg->accept(*this);
        callArgs.push_back(getLastValue());
    }

    IRValue result = makeValue(e.type);
    emit({ .op = IROp::CALL, .dst = result, .args = std::move(callArgs) });
    setLastValue(result);
}

// ======================================================
// Statements
// ======================================================

void IRBuilder::visit(Print& s) {
    s.expr->accept(*this);

    IRInstr instr{
        .op = IROp::PRINT,
        .args = {getLastValue()}
    };
    emit(instr);
}

void IRBuilder::visit(If& s) {
    /*
    Format
    -----------
    B0(currBlock):
        <condition>
        branch cond B1 B2
    
    B1(then):
        <thenBranch>
        jump B3
    
    B2(else): ; This may not exist!
        <elseBranch>
        jump B3
    
    B3(merge):
        ...
    */
    
    // Note that order matters!!
    HIRBlock* thenBB  = makeBlock();
    HIRBlock* mergeBB = makeBlock();

    // Note that else block is just final block if else branch doesn't exist.
    HIRBlock* elseBB = mergeBB;

    if (s.elsebranch) {
        elseBB = makeBlock();
    }

    // Condition lives in current block.
    s.condition->accept(*this);
    HIROperand cond = getLastValue();

    currCtx->currBlock->term = BranchTerm<IRInstr>{
        cond,
        thenBB,
        elseBB
    };

    connectBlock(currCtx->currBlock, thenBB);
    connectBlock(currCtx->currBlock, elseBB);

    //
    // THEN
    //
    currCtx->currBlock = thenBB;
    s.thenbranch->accept(*this);
    // After thenBranch, the terminator MAY exist (e.g. return)
    // so we should be careful not to overwrite it.
    if (!hasTerminator(currCtx->currBlock)) {
        currCtx->currBlock->term = JumpTerm<IRInstr>{mergeBB};
        connectBlock(currCtx->currBlock, mergeBB);
    }

    //
    // ELSE
    //
    if (s.elsebranch) {
        currCtx->currBlock = elseBB;
        s.elsebranch->accept(*this);

        // Similar logic to thenBranch.
        if (!hasTerminator(currCtx->currBlock)) {
            currCtx->currBlock->term = JumpTerm<IRInstr>{mergeBB};
            connectBlock(currCtx->currBlock, mergeBB);
        }
    }

    currCtx->currBlock = mergeBB;
}

void IRBuilder::visit(While& s) {
    /*
    Format
    -----------
    B0(currBlock):
        <condition>
        jump B1
    
    B1(cond):
        <condition>
        branch cond B2 B3

    B2(body):
        ...
        <condition>
        jump B1
    
    B3(exit):
        ...
    */
    HIRBlock* condBB = makeBlock();
    HIRBlock* bodyBB = makeBlock();
    HIRBlock* exitBB = makeBlock();

    currCtx->currBlock->term = JumpTerm<IRInstr>{condBB};

    connectBlock(currCtx->currBlock, condBB);

    // Condition lives in condBB
    currCtx->currBlock = condBB;

    s.condition->accept(*this);
    HIROperand cond = getLastValue();

    /*
    While-condition example:

        while (x < n && (isLetter(ch) || isDigit(ch))) { ... }

    Short-circuit lowering can leave currBlock at a nested merge block, not
    condBB. The loop branch and its successor edges must start at that actual
    final condition block; otherwise SSA records stale predecessors/values.
    */
    HIRBlock* conditionEndBB = currCtx->currBlock;

    conditionEndBB->term =
        BranchTerm<IRInstr>{cond, bodyBB, exitBB};

    connectBlock(conditionEndBB, bodyBB);
    connectBlock(conditionEndBB, exitBB);

    currCtx->loopStack.push_back({condBB, exitBB});

    currCtx->currBlock = bodyBB;
    s.body->accept(*this);

    if (!hasTerminator(currCtx->currBlock)) {
        currCtx->currBlock->term = JumpTerm<IRInstr>{condBB};
        connectBlock(currCtx->currBlock, condBB);
    }

    currCtx->loopStack.pop_back();

    currCtx->currBlock = exitBB;
}

void IRBuilder::visit(Block& s) {
    for (auto& stmt : s.statements) {
        if (hasTerminator(currCtx->currBlock)) {
            // Dead code after terminator
            break;
        }
        stmt->accept(*this);
    }
}

void IRBuilder::visit(Break& s) {
    if (currCtx->loopStack.empty()) {
        throw KMYCompileError("break outside loop");
    }

    auto& loop = currCtx->loopStack.back();

    currCtx->currBlock->term = JumpTerm<IRInstr>{
        loop.breakTarget
    };

    connectBlock(currCtx->currBlock, loop.breakTarget);

    // Return to the upper block
    //currCtx->currBlock = loop.breakTarget;

    // This(^) is not needed because the break target will be the next block to execute after the loop, 
    // and the current block is effectively terminated by the jump instruction.

    // Simply, currBlock is done after break/continue,
    // And the parent AST will contigure where to emit code afterwards.
    // Since break/continue only happens in loops, While& will handle that.
}

void IRBuilder::visit(Continue& s) {
    if (currCtx->loopStack.empty()) {
        throw KMYCompileError("continue outside loop");
    }

    auto& loop = currCtx->loopStack.back();

    currCtx->currBlock->term = JumpTerm<IRInstr>{
        loop.continueTarget
    };

    connectBlock(currCtx->currBlock, loop.continueTarget);

    // current block is dead after continue
    //currCtx->currBlock = loop.continueTarget;
}

// Connect a source-level `let` binding to the HIR value it defines.
//
// The definition is recorded for SSA renaming/phi placement.  Aliases such as
// `let y = x` use BIND, while ordinary values are attached to the instruction
// that produced them.  The producer may be earlier than the last instruction
// when side-effecting stores follow it (for example, array initialization), so
// search backward instead of assuming the immediately preceding instruction is
// the value's producer.
void IRBuilder::bindLocalDefinition(
    VarSymbol* sym,
    const HIROperand& value
) {
    // Keep this for phi placement.
    currCtx->currBlock->defs.push_back({
        sym,
        value
    });

    // Alias: y = x
    if (std::holds_alternative<VarRef>(value)) {
        emit({
            .op = IROp::BIND,
            .args = { value },
            .defSym = sym
        });

        return;
    }

    // Normal value-producing expression.
    IRValue rhs = std::get<IRValue>(value);

    assert(!currCtx->currBlock->code.empty());

    IRInstr& producer = currCtx->currBlock->code.back();

    if (producer.dst.has_value()) {
        assert(producer.dst->id == rhs.id);
        producer.defSym = sym;
    } else {
        // The value may have been produced earlier, followed by side-effecting
        // stores (for example, array literal element initialization). Attach
        // the definition to that earlier producer so BIND does not survive to
        // MIR lowering.
        for (auto it = currCtx->currBlock->code.rbegin();
             it != currCtx->currBlock->code.rend();
             ++it) {
            if (it->dst.has_value() && it->dst->id == rhs.id) {
                it->defSym = sym;
                return;
            }
        }
        throw KMYCompileError("Unable to find array value producer for local binding.");
    }
}

void IRBuilder::visit(Let& s) {
    if (s.isFunctionDecl) {
        // Do not traverse when it is hoisted.
        // It is traversed in the function expr.
        //return;   
    }

    if (s.expr) {
        s.expr->accept(*this);
    } else {
        throw KMYCompileError("Uninit expr not yet.");
    }

    // Don't allocate function decl again (already hoisted in func expr visit)
    if (s.isFunctionDecl) { return; }

    HIROperand value = getLastValue();

    if (s.symbol->isModuleGlobal) {
        emit({ .op = IROp::STORE_GLOBAL, .args = { value }, .imm = s.symbol->moduleGlobalSlot });
        return;
    }
    
    bool uncapturedLocal = true;
    if (currCtx->locals.find(s.symbol) == currCtx->locals.end()) {
        std::cout << "Let has no existing local binding: " << s.symbol->name << '\n';
        uncapturedLocal = true;
       // currCtx->locals[s.symbol] = IRLocalInfo{ .value = value, .isCell = false };
    } else {
        std::cout << "Duplicate write prevented which is not very desired.\n";
    }

    // Set env right away.
    // This decl is captured from children closure.
    // This time, we need to copy the POINTER of this value.
    auto it = currCtx->fnCtx->envMap.find(s.symbol);

    IRValue cell;
    std::cout << "[let] currctx->env: " << currCtx->env.has_value() << "\n";
    
    if (currCtx->env && it != currCtx->fnCtx->envMap.end()) {
        uncapturedLocal = false;
        std::cout << "[let] " << s.symbol->name << " exists in env\n";

        Type* type = nullptr;
        if (std::holds_alternative<IRValue>(value)) {
            type = std::get<IRValue>(value).type;
        } else if (std::holds_alternative<VarRef>(value)) {
            type = std::get<VarRef>(value).sym->type;
        } 

        cell = makeValue(
            new CellType(type)
        );

        emit({
            .op = IROp::ALLOC_CELL_INIT,
            .dst = cell,
            .args = { value }
        });

        emit({
            .op = IROp::SET_ENV,
            .args = { currCtx->env.value(), cell },
            .imm = it->second
        });

        // IMPORTANT! Overwrite local reads with cell reads.
        currCtx->locals[s.symbol] = IRLocalInfo{ .value = cell, .isCell = true };
    }

    if (uncapturedLocal) {
        bindLocalDefinition(s.symbol, value);
    } else {
        //currCtx->currBlock->defs.push_back({s.symbol, cell});
    }
}

void IRBuilder::visit(Return& s) {
    s.expr->accept(*this);

    currCtx->currBlock->term = ReturnTerm{ getLastValue() };
}

void IRBuilder::visit(Aggregate& s) {
    bool oldMemberMode = compilingAggregateMember;
    compilingAggregateMember = true;

    for (auto& method : s.methodMembers) {
        method.methodExpr->accept(*this);
    }
    s.fieldInitFunc->accept(*this);
    for (auto& constructor : s.constructorMembers) {
        constructor.initFuncExpr->accept(*this);
    }

    compilingAggregateMember = oldMemberMode;
}

void IRBuilder::visit(TypeAlias& s) {}
void IRBuilder::visit(Enum& s) {}

void IRBuilder::visit(ExprStmt& s) {
    s.expr->accept(*this);
}
