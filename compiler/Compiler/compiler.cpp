#include "compiler.hpp"
#include "../Core/newParser.hpp"

#include <stdexcept>
#include <exception>
#include <cassert>
#include <iostream>
#include "../Core/Ast.hpp"
#include "../BytecodeVM/vm.hpp"
#include "../Core/errorhandler.hpp"

void Compiler::emit(Opcode op, int operand=-1) {
    currCtx->chunk.code.push_back(Instruction{op, operand});
}

Compiler::Compiler(Module& module)
    : program(module.program)
{
    // Initialize the first function context for the global scope.
    currCtx = nullptr;
}

std::vector<FunctionProto> Compiler::compile(void) {
    std::cout << "Starting compilation..." << std::endl;
    program->accept(*this);
    std::cout << "Done" << std::endl;

     // Emit HALT at the end of global chunk to prevent fallthrough into function chunks.

    // emit(Opcode::HALT);

    return funcProtos;
}


int Compiler::addConstant(const ConstValue& v) {
    // Returns the index of the constant v in the chunk.
    auto& map = currCtx->constantMap;
    auto it = map.find(v);
    // Constant exists.
    if (it != map.end()) {
        return it->second;
    }

    int index = currCtx->chunk.constants.size();
    currCtx->chunk.constants.push_back(v);
    currCtx->constantMap[v] = index;
    return index;
}

// Expressions
void Compiler::visit(Literal& e) {
    // Literal only contains ConstValue
    ConstValue v = std::visit([](auto&& arg) -> ConstValue {
        return ConstValue(arg);
    }, e.value);

    int index = addConstant(v);
    emit(Opcode::PUSH_CONST, index);
}

void Compiler::visit(ArrayLiteral& e) {
    for (auto it = e.elements.rbegin(); it != e.elements.rend(); ++it) {
        (*it)->accept(*this);
    }
    emit(Opcode::NEW_ARRAY, e.elements.size());
}


void Compiler::visit(RecordLiteral& e) {
    for (auto it = e.fields.rbegin(); it != e.fields.rend(); ++it) {
        it->second->accept(*this);
    }

    emit(Opcode::MAKE_RECORD, e.fields.size());
}


void Compiler::visit(Variable& e) {
    /*
    // This will be done in closure analysis stage.
    if (!e.resolved) {
        e.resolved = true;   
        e.resolution = resolveVariable(e.name);
    }
    */
    switch (e.resolution.kind) {
        case ResolvedVar::Kind::LOCAL:
            emit(Opcode::LOAD_LOCAL, e.resolution.index);
            break;

        case ResolvedVar::Kind::UPVALUE:
            emit(Opcode::LOAD_UPVALUE, e.resolution.index);
            break;

        // TODO(): glboals actually DE.
        // 
        case ResolvedVar::Kind::GLOBAL:
            emit(Opcode::LOAD_GLOBAL, e.resolution.index);
            break;
    }
}


static Opcode binaryOpToOpcode(BinaryOp op) {
    switch (op) {
        case BinaryOp::Plus:          return Opcode::ADD;
        case BinaryOp::Minus:         return Opcode::SUB;
        case BinaryOp::Star:          return Opcode::MUL;
        case BinaryOp::Slash:         return Opcode::DIV;
        case BinaryOp::Percent:       return Opcode::MOD;

        case BinaryOp::EqualEqual:    return Opcode::EQUAL;
        case BinaryOp::NotEqual:      return Opcode::NOT_EQUAL;
        case BinaryOp::Less:          return Opcode::LESS;
        case BinaryOp::LessEqual:     return Opcode::LESS_EQUAL;
        case BinaryOp::Greater:       return Opcode::GREATER;
        case BinaryOp::GreaterEqual:  return Opcode::GREATER_EQUAL;

        case BinaryOp::LogicalAnd:    return Opcode::LOGICAL_AND;
        case BinaryOp::LogicalOr:     return Opcode::LOGICAL_OR;

        case BinaryOp::BitAnd:        return Opcode::BIT_AND;
        case BinaryOp::BitOr:         return Opcode::BIT_OR;
        case BinaryOp::BitXor:        return Opcode::BIT_XOR;
        case BinaryOp::LShift:        return Opcode::LEFT_SHIFT;
        case BinaryOp::RShift:        return Opcode::RIGHT_SHIFT;

        default:
            throw std::runtime_error("Unknown BinaryOp");
    }
}

void Compiler::visit(BinaryExpr& e) { 
    e.left->accept(*this);
    e.right->accept(*this);
    emit( binaryOpToOpcode(e.op) );
}


static Opcode unaryOpToOpcode(UnaryOp op) {
    switch (op) {
        case UnaryOp::Plus:
            // Unary plus usually does nothing
            // Parser will ignore UnaryOp::Plus, but if it somehow came here...?
            throw std::runtime_error("Unary plus does not require an opcode");

        case UnaryOp::Minus:
            return Opcode::NEG;

        case UnaryOp::LogicalNot:
            return Opcode::LOGICAL_NOT;

        case UnaryOp::BitNot:
            return Opcode::BIT_NOT;

        default:
            throw std::runtime_error("Unknown UnaryOp");
    }
}

void Compiler::visit(UnaryExpr& e) { 
    if (e.op == UnaryOp::AddressOf || e.op == UnaryOp::Dereference) {
        throw KMYCompileError(
            "The VM backend does not support raw pointer operations; compile this program with `-asm`."
        );
    }

    e.operand->accept(*this);
    emit( unaryOpToOpcode(e.op) );
}

void Compiler::handleAssignment(AssignmentOp op, ExprPtr left, ExprPtr right) {
    // compound assignment
    // Puts the RHS or (LHS op RHS) to the stack according to assignment op.
    if (op != AssignmentOp::Assign) {
        // emit load_local / load_upvalue etc. according to its resolved type.
        left->accept(*this);
        right->accept(*this);
        emit(binaryOpToOpcode(compoundToBinaryOp(op)));
    } else {
        right->accept(*this);
    }
}

// TODO... used once and diff format in thisexpr
void Compiler::emitResolutionResult(ResolvedVar res) {
    switch(res.kind) {
        case ResolvedVar::Kind::LOCAL:
            emit(Opcode::STORE_LOCAL, res.index);
            break;

        case ResolvedVar::Kind::UPVALUE:
            emit(Opcode::STORE_UPVALUE, res.index);
            break;

        case ResolvedVar::Kind::GLOBAL:
            emit(Opcode::STORE_GLOBAL, res.index);
            break;
    }
}
void Compiler::visit(Assignment& e) {

    // Case 1: variable assignment
    assert(e.left->isLValue());

    if (e.left->kind == ExprKind::Variable) {
        // Stack: [...] [ value ]
        
        auto var = std::static_pointer_cast<Variable>(e.left);
        if (!e.isInitialisation && !var->symbol->isMutable) {
            throw KMYCompileError("Assignment to a constant variable \"" + var->name + "\"");
        }
        //assert(var->symbol->isMutable);
        
        handleAssignment(e.op, e.left, e.right);

        emitResolutionResult(var->resolution);
        return;
    }

    // Case 2: array index assignment
    if (e.left->kind == ExprKind::Index) {
        auto index = std::static_pointer_cast<Index>(e.left);
        
        // stack: [ ... value, array, index ]
        handleAssignment(e.op, e.left, e.right);

        index->obj->accept(*this);
        index->index->accept(*this);

        emit(Opcode::SET_INDEX);
        return;
    }

    // Case 3: record field assignment
    if (e.left->kind == ExprKind::Get) {
        auto get = std::static_pointer_cast<Get>(e.left);
        // stack: [ ... value, object ]

        handleAssignment(e.op, e.left, e.right);

        get->obj->accept(*this);
        emit(Opcode::SET_PROPERTY, get->fieldIdx);
        return;
    }

    throw KMYCompileError("Invalid assignment target");
}

void Compiler::visit(Index& e) { 
    // Push array
    e.obj->accept(*this);

    // Push index
    e.index->accept(*this);

    emit(Opcode::GET_INDEX);
}

void Compiler::visit(Call& e) { 
    std::cout << "Compiling function call..." << std::endl;

    // Push function
    if (e.func->kind == ExprKind::Get) {
        // obj.f()
        // ^^^^^callee
        auto callee = std::static_pointer_cast<Get>(e.func);
        // change maybe. hard to track.
        if (callee->resolvedMethod) {
            // Instance object.
            if (callee->obj->type->kind != TypeKind::INSTANCE) {
                throw KMYCompileError("Serious error. not an instance?");
            }
            auto aggType = static_cast<InstanceType*>(callee->obj->type);

            // To be used for vtables.
            int methodIdx = callee->methodIdx;

            emit(Opcode::MAKE_CLOSURE, aggType->methodMap[callee->name]->funcProtoIdx);
        } else {
            e.func->accept(*this);
        }
    } else {
        e.func->accept(*this);
    }

    // Push args
    for(auto it = e.args.rbegin(); it != e.args.rend(); ++it) {
        (*it)->accept(*this);
    }

    emit(Opcode::CALL, e.args.size());
}

void Compiler::visit(Get& e) {
    // Push object
    e.obj->accept(*this);

    emit(Opcode::GET_PROPERTY, e.fieldIdx);
}

void Compiler::visit(ScopeAccessExpr& e) {
    // TODO: Currently only enum...
    int idx = addConstant(e.accessIdx);

    emit(Opcode::PUSH_CONST, idx);
}


int Compiler::allocateFuncProto(const FunctionProto& fnProto) {
    int idx = funcProtos.size();
    funcProtos.push_back(fnProto);
    return idx;
}


void Compiler::visit(FunctionExpr& e) {
    std::cout << "Compiling function..." << std::endl;
    FunctionProto fnProto;
    fnProto.totalParams = e.params.size();
    fnProto.frameSize = e.functionContext->locals.size();

    auto temp = new CodegenFnCtx;
    temp->parent = currCtx;
    currCtx = temp;

    // Allocate params
    for (auto& param : e.params) {
        allocateLocal(param.symbol);
    }

    for (auto& param : e.params) {
        if (param.defaultExists) {
            // Not implemented yet...
        }
    }

    std::cout << "body compile" << std::endl;

    bool isCompilingMethod = compilingMethod;
    compilingMethod = false;

    for (auto& [funcDeclSym, funcDeclExpr] : e.functionContext->funcDecls) {
        funcDeclExpr->accept(*this);
        // 7. Emit closure (NOT MAKE_FUNCTION)
        
            if (!isCompilingMethod) {
                emit(Opcode::MAKE_CLOSURE, funcDeclExpr->fnProtoIdx);
            }
            // Not a top level function.
            // 8. Capture variables
            /*
            std::cout << "Capture" << std::endl;
            for (auto up : e.upvalues) {
                std::cout << up.index << std::endl;
                // CAPTURE opcode creates a runtime heap object UpValueObj
                if (up.isLocal) {
                    // From immediate parent,
                    // take x directly from parent stack frame
                    //emit(Opcode::CAPTURE_LOCAL, up.index);
                } else {
                    // take variable from my parent closure’s upvalue list
                    //emit(Opcode::CAPTURE_UPVALUE, up.index);
                }
            }
            std::cout << "Capture done" << std::endl;
            std::cout << "Delete temp" << std::endl;
            */
        
        allocateLocal(funcDeclSym);

        emit(Opcode::STORE_LOCAL, funcDeclSym->slot);
        emit(Opcode::POP);
    }

    // 3. Body
    e.body->accept(*this);
    compilingMethod = isCompilingMethod;
    
    std::cout << "body compile done" << std::endl;

    // 4. Implicit return
    if (currCtx->parent == nullptr) {
        // Top level function. Emit HALT.
        emit(Opcode::HALT);
    } else if (isConstructor) {
        // Constructor returns "this". (because of "new")
        // TODO: Forbid any random returns in constructors
        emit(Opcode::LOAD_LOCAL, 0);
        emit(Opcode::RETURN_VALUE);
    } else {
        // Not top level. Just return.
        emit(Opcode::RETURN_VOID);
    }
    // 5. Extract compiled context
    fnProto.chunk = std::move(currCtx->chunk);
    fnProto.upvalues = e.functionContext->upvalues;
    fnProto.upValueCnt = e.functionContext->upvalues.size();

    currCtx = currCtx->parent;
    std::cout << "currctx: " << currCtx << std::endl;

    // 6. Register proto
    int fnIndex = allocateFuncProto(fnProto);
    e.fnProtoIdx = fnIndex;
    
    // 7. Emit closure (NOT MAKE_FUNCTION)
    
    if (currCtx) {
        if (!isCompilingMethod) {
            emit(Opcode::MAKE_CLOSURE, fnIndex);
        }
    }
    
    delete temp;
    std::cout << "function compiled" << std::endl;
}


void Compiler::visit(ThisExpr& e) {
    // "this" is already guaranteed to be inside methods.
    // Thus always load the 0th slot.
    // ... ONLY WHEN THEY ARE LOCALS.
    // They can be CAPTURED AS UPVALUES TOO.
    
    assert(e.resolved);
    switch(e.resolution.kind) {
        case ResolvedVar::Kind::LOCAL:
            emit(Opcode::LOAD_LOCAL, e.resolution.index);
            break;

        case ResolvedVar::Kind::UPVALUE:
            emit(Opcode::LOAD_UPVALUE, e.resolution.index);
            break;

        case ResolvedVar::Kind::GLOBAL:
            emit(Opcode::LOAD_GLOBAL, e.resolution.index);
            break;
    }
}

void Compiler::visit(NewExpr& e) { 
    // NewExpr's type is always aggregate... right?
    if (e.type->kind != TypeKind::INSTANCE) {
        throw KMYCompileError("new applied to non aggregate type. but this should not print here.");
    }
    auto aggType = static_cast<InstanceType*>(e.type);
    int argCnt = e.args.size();

    for (auto& constructor : aggType->constructorVec) {
        std::cout << "Checking constructor signature\n";
        if (constructor->type->kind != TypeKind::FUNCTION) {
            throw KMYCompileError("Constuctor is not a function? This is weird.");
        }
        auto constrFuncType = static_cast<FunctionType*>(constructor->type);
        // HACK HACK HACK: due to implicit this u need +1
        if (constrFuncType->paramTypes.size() == argCnt + 1) {
            // HACK
            // TODO: argCnt needs update


            // Calling the user constructor
            // Inside, the field init func will run first.
            emit(Opcode::MAKE_CLOSURE, constructor->funcProtoIdx);
            
            // Basically allocating memory.
            // field init
            emit(Opcode::PUSH_UNINITIALISED, aggType->fieldMap.size());
            emit(Opcode::MAKE_RECORD, aggType->fieldMap.size());
            
            emit(Opcode::DUPLICATE);

            emit(Opcode::MAKE_CLOSURE, fieldInitFuncProtoIdx[aggType]);
            
            // stack: [...] [userConstructor] [emptyRec] [emptyRec] [fieldInitFunc]
            
            emit(Opcode::SWAP);
            
            // stack: [...] [userConstructor] [emptyRec] [fieldInitFunc] [emptyRec]
            
            emit(Opcode::CALL, 1);
            
            // stack: [...] [userConstructor] [filledRec] [GARBAGE]
            
            emit(Opcode::POP); // This is essential. RETURN_VOID pushes garbage.
            
            // stack: [...] [userConstructor] [filledRec]

            for(auto it = e.args.begin(); it != e.args.end(); ++it) {
                (*it)->accept(*this);
            }

            // stack: [...] [userConstructor] [filledRec] [args...]
            emit(Opcode::CALL, argCnt + 1);

            // stack: [...]
            return;
        }
    }
    throw KMYCompileError("Cannot find a constructor matching arg cnt from new expr.");
}


// Statements
void Compiler::visit(Print& s) {
    // Compile the expression to leave its value on the stack
    s.expr->accept(*this);

    // Emit print instruction
    emit(Opcode::PRINT);

    std::cout << "Print statement compiled." << std::endl;
}


int Compiler::emitJump(Opcode op) {
    emit(op, -1);
    // Returns the pos of the "op" instruction.
    return currCtx->chunk.code.size() - 1;
}

void Compiler::patchJump(int pos) {
    currCtx->chunk.code[pos].operand = currCtx->chunk.code.size();
}

void Compiler::visit(If& s) { 
    s.condition->accept(*this);
    // Now the condition evaluation result is on top of stack.
    int falseJumpPos = emitJump(Opcode::JUMP_IF_FALSE);
    s.thenbranch->accept(*this);

    // Jump over ELSE
    int endJump = emitJump(Opcode::JUMP);

    patchJump(falseJumpPos);
    if (s.elsebranch) {
        s.elsebranch->accept(*this);
    }
    patchJump(endJump);
}


void Compiler::visit(While& s) {
    // mark loop start
    int loopStartPos = currCtx->chunk.code.size();

    // compile condition
    s.condition->accept(*this);

    // jump out if false
    int exitJumpPos = emitJump(Opcode::JUMP_IF_FALSE);

    loopStack.push_back(CodegenLoopCtx{loopStartPos, {}});

    // loop body
    s.body->accept(*this);

    auto loopCtx = loopStack.back();
    
    // jump back to loop start
    emit(Opcode::JUMP, loopStartPos);
    
    // patch break jumps to here (loop exit)
    loopStack.pop_back();
    for (int breakPos : loopCtx.breakPositions) {
        patchJump(breakPos);
    }
    // patch exit jump
    patchJump(exitJumpPos);
}


void Compiler::visit(Block& s) { 
    currCtx->scopeDepth++;   // ENTER scope

    for (auto& stmt : s.statements) {
        stmt->accept(*this);
    }

    currCtx->scopeDepth--;   // EXIT scope

    // remove locals declared in this scope
    while (!currCtx->locals.empty() && currCtx->locals.back().scopeDepth > currCtx->scopeDepth) {
        auto& local = currCtx->locals.back();

        if (local.captured) {
            emit(Opcode::CLOSE_UPVALUE, local.slot);
        }

        currCtx->locals.pop_back();
    }
}

void Compiler::visit(Break& s) { 
    if (loopStack.empty()) {
        throw KMYCompileError("Invalid break statement. Not inside a loop.");
    }

    // Emit jump and record its position to patch later.
    int breakJumpPos = emitJump(Opcode::JUMP);

    // Record this break position in the current loop context.
    loopStack.back().breakPositions.push_back(breakJumpPos);
}

void Compiler::visit(Continue& s) {
    if (loopStack.empty()) {
        throw KMYCompileError("Invalid continue statement. Not inside a loop.");
    }

    // Emit jump to loop start
    emit(Opcode::JUMP, loopStack.back().continuePos);
}


void Compiler::allocateLocal(VarSymbol* sym) {
    currCtx->locals.push_back(Local{
        sym,
        sym->slot,
        currCtx->scopeDepth,
        false, // initially not captured.
    });
}

void Compiler::visit(Let& s) { 
    if (s.isFunctionDecl) { return; }
    
    if (s.expr) {
        s.expr->accept(*this);
    } else {
        // emit(Opcode::PUSH_CONST, addConstant(nullptr));
    }
    
    // Function decl hoisted in the function expr.
    allocateLocal(s.symbol);

    if (s.expr) {
        // Only push or pop values when there is an initialiser.
        emit(Opcode::STORE_LOCAL, s.symbol->slot);
        emit(Opcode::POP); // Important: Ensures stack size invariance after statement.
    }
}


void Compiler::visit(Return& s) { 
    if (s.expr) {
        s.expr->accept(*this);
        emit(Opcode::RETURN_VALUE);
    } else {
        emit(Opcode::RETURN_VOID);
    }
}

/*
struct MethodInfo {
    std::string name;
    int protoIdx;
};

struct AggregateInfo {
    int fieldSize = 0;
    int constructorProtoIdx = -1;

    std::vector<MethodInfo> methodInfos; 
};
*/

void Compiler::visit(Aggregate& s) {     
    // We don't generate code for fields.

    // AggregateInfo aggInfo;
    // aggInfo.fieldSize = s.fieldCount;

    compilingMethod = true;
    auto oldAggType = currAggType;
    currAggType = static_cast<InstanceType*>(s.typeSymbol->type);
    for (auto& method : s.methodMembers) {
        method.methodExpr->accept(*this);
        method.symbol->funcProtoIdx = method.methodExpr->fnProtoIdx;

        //aggInfo.methodInfos.push_back({ method.name, funcProtoCnt });

    }

    // This MUST be traversed first (since constructor uses its funcProtoIdx)
    s.fieldInitFunc->accept(*this);
    fieldInitFuncProtoIdx[static_cast<InstanceType*>(s.typeSymbol->type)] 
        = s.fieldInitFunc->fnProtoIdx;

    isConstructor = true;
    for (auto& constructor : s.constructorMembers) {
        constructor.initFuncExpr->accept(*this);
        constructor.symbol->funcProtoIdx = constructor.initFuncExpr->fnProtoIdx;

        //aggInfo.methodInfos.push_back({ method.name, funcProtoCnt });
    }
    isConstructor = false;


    compilingMethod = false;
    currAggType = oldAggType;
}

void Compiler::visit(TypeAlias& s) {}

// Enum has nothing to visit...
void Compiler::visit(Enum& s) {}

void Compiler::visit(ExprStmt& s) { 
    s.expr->accept(*this);

    // Always pop since Expr leaves one value on the stack
    // and Stmt should not leave no value on the stack.
    emit(Opcode::POP);
}
