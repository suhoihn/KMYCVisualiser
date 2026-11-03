#include "ClosureAnalyser.hpp"
#include "../Core/newParser.hpp"

#include <iostream>
#include "../Core/errorhandler.hpp"
#include "../Utils/SymbolPrinter.hpp"
#include "../Utils/utils.hpp"
#include <assert.h>

ClosureAnalyser::ClosureAnalyser(Module& module, int& nextFunctionId)
    : module(module), nextFunctionId(nextFunctionId)
{}

void ClosureAnalyser::analyse() {
    // Compatibility context only: the module's statements are visited
    // directly, but legacy HIR still needs a FunctionContext while the native
    // module-initializer generation is introduced in a later step.
    FunctionContext* moduleContext = new FunctionContext;
    moduleContext->fnScope = module.globalScope;
    currCtx = moduleContext;

    module.program->functionId = nextFunctionId++;
    for (const StmtPtr& statement : module.topLevelStatements) {
        statement->accept(*this);
    }

    int envSlot = 0;
    for (const Local& local : moduleContext->locals) {
        if (local.captured) {
            moduleContext->envMap[local.sym] = envSlot++;
        }
    }
    for (const UpvalueInfo& upvalue : moduleContext->upvalues) {
        if (upvalue.capturedByChildren) {
            moduleContext->envMap[upvalue.symbol] = envSlot++;
        }
    }
    moduleContext->envSize = envSlot;

    module.program->functionContext = moduleContext;
    currCtx = nullptr;
}

static int allocateLocal(FunctionContext* fnCtx, VarSymbol* sym) {
    assert(fnCtx);
    assert(sym);

    // allocateLocal should not be called for symbol already allocated.
    assert(fnCtx->localMap.find(sym) == fnCtx->localMap.end());

    int slot = fnCtx->nextSlot++;
    fnCtx->localMap[sym] = slot;

    // For debug.
    sym->slot = slot;

    std::cout << sym << std::endl;
    std::cout << "Allocating local variable: " << sym->name << " at slot " << slot << std::endl;
    fnCtx->locals.push_back(Local{
        sym,
        slot,
        fnCtx->scopeDepth,
        false, // initially not captured.
    });

    return slot;
}

static int resolveUpvalue(FunctionContext* fnCtx, VarSymbol* sym) {
    // Returns the slot of upvalue where name belongs in fnCtx's context.
    // TODO: Example will be very helpful.

    if (!sym) {
        throw KMYCompileError("Symbol not resolved? this is stupid.");
    }
    // Returns the slot of upvalue where name belongs in fnCtx's context.
    std::cout << "[DEBUG] Resolving upvalue for symbol: " << sym->name << std::endl;

    auto parentCtx = fnCtx->parent;
    if (!parentCtx) {
        // Parent doesn't exist (woah!)
        return INVALID_SLOT;
    }

    std::cout << "In resolveupvalue part 1\n";
    // Check whether name is already resolved in current context.
    auto itMap = fnCtx->upvalueMap.find(sym);
    if (itMap != fnCtx->upvalueMap.end()) {
        return itMap->second;
    }
    
    std::cout << "In resolveupvalue part 2\n";
    // Firstly, is name in parent's local?
    
    if (isInsideFunction(sym, parentCtx->fnScope)) {
        // Found in parent's local.
        // Check whether it has its slot allocated in parent's context.
        auto it = parentCtx->localMap.find(sym);
        int parentLocalSlot;
        if (it != parentCtx->localMap.end()) {
            // Sym slot already exists in parent's locals.
            parentLocalSlot = it->second;
        } else {
            // Sym hasn't been allocated a slot.
            // Thus allocate it in parent.
            parentLocalSlot = allocateLocal(parentCtx, sym);
        }
        

        // Update current fnCtx's upvalue map.
        int slot = fnCtx->upvalues.size();
        fnCtx->upvalueMap[sym] = slot;

        std::cout << "In resolveupvalue part 2-3\n";
        
        // Also the upvalues vector.
        fnCtx->upvalues.push_back(
            UpvalueInfo {
                true,               // it is in parent's local
                parentLocalSlot,    // return slot of parent's local.
                sym                 // Storing the symbol for the captured value (for IR codegen)
            }
        );

        // Also we tell the local in locals that it is captured.
        // So that VM knows to keep it on heap when destroyed.
        parentCtx->locals[parentLocalSlot].captured = true;
        return slot;
    }

    std::cout << "In resolveupvalue part 3\n";
    // Not found in parent's local.
    // Let's go up and add name in ancestor's upvalues vector.
    // Slot in parent's upvalue vector.
    int parentUpvalueSlot = resolveUpvalue(parentCtx, sym);
    if (parentUpvalueSlot != INVALID_SLOT) {
        // parentCtx->upvalues[parentSlot] stores where name is stored.

        parentCtx->upvalues[parentUpvalueSlot].capturedByChildren = true;
        
        int slot = fnCtx->upvalues.size();
        fnCtx->upvalueMap[sym] = slot;
        fnCtx->upvalues.push_back(
            UpvalueInfo {
                false,              // it is not in parent's local.
                parentUpvalueSlot,  // It is a slot in parent's upvalue (so fnCtx->upvalue[slot] = parentUpvalueSlot) 
                                    // You need to recursively walk in parent's upvalues, not locals.
                sym
            }
        );
        return slot;
    }

    // Cannot find name anywhere. Allocation failed.
    return INVALID_SLOT;
}


static bool isInsideFunction(VarSymbol* sym, Scope* functionScope) {
    Scope* s = sym->definingScope;

    while (s) {
        if (s == functionScope) return true;
        s = s->parent;
    }

    return false;
}

ResolvedVar ClosureAnalyser::resolveVariable(VarSymbol* sym) {
    if (!sym) {
        throw KMYCompileError("Symbol not resolved? this is stupid.");
    }

    std::cout << "In resolveVariable(), first search function's local scope.\n";
    // 1. Local
    if (isInsideFunction(sym, currCtx->fnScope)) {
        // Found in function's local.
        // Check whether it has its slot allocated.
        auto it = currCtx->localMap.find(sym);
        if (it != currCtx->localMap.end()) {
            // Sym slot already exists
            return ResolvedVar {
                ResolvedVar::Kind::LOCAL,
                it->second,
            };
        }
        
        // Sym hasn't been allocated a slot.
        int slot = allocateLocal(currCtx, sym);

        return ResolvedVar {
            ResolvedVar::Kind::LOCAL,
            slot,
        };

    }
    
    std::cout << "In resolveVariable(), next search upvalue\n";
    // 2. Upvalue
    int up = resolveUpvalue(currCtx, sym);
    if (up != INVALID_SLOT) {
        std::cout << "Captured upvalue! " << sym->name << "\n";
        std::cout << "Upvalue slot: " << up << "\n";

        return ResolvedVar {
            ResolvedVar::Kind::UPVALUE,
            up
        };
    }

    throw KMYCompileError("Undefined variable.");
}

void ClosureAnalyser::visit(Variable& e) {
    std::cout << "Resolving variable reference\n";
    std::cout << "var node=" << &e << '\n';
    std::cout << "symbol=" << e.symbol << '\n';
    std::cout << "resolved= " << e.resolved << "\n";
    std::cout << "resolution=" << (int)e.resolution.kind << "\n";
    std::cout << typeToString(e.type) << "\n";
    std::cout << "name=" << e.name << "\n";

    if (e.symbol)
        std::cout << "symbol name=" << e.symbol->name << '\n';

    if (e.symbol->nativeFnPtr) {
        std::cout << "Native function uses global slot\n";
        e.resolved = true;
        // TODO: Really feels like a hack...
        e.resolution = ResolvedVar{ResolvedVar::Kind::GLOBAL, e.symbol->globalSlot};
        return;
    }
        
    if (!e.resolved) {
        e.resolved = true;
        std::cout << "Resolving variable storage\n";
        e.resolution = resolveVariable(e.symbol);
    }
}

void ClosureAnalyser::visit(FunctionExpr& e) {
    std::cout << "Entering function: " << e.params.size() << " params\n";
    std::cout << "it is given an id of " << nextFunctionId << "\n";
    e.functionId = nextFunctionId++;
    
    // 1. Create new context
    FunctionContext* fnCtx = new FunctionContext;
    fnCtx->parent = currCtx;
    fnCtx->fnScope = e.scope;
    currCtx = fnCtx;
    

    // 2. Parameters
    for (auto& param : e.params) {
        if (!param.symbol) {
            throw KMYCompileError("Symbol not resolved in param? this is stupid.");
        }
        // No dedup check since it is guaranteed to be new.
        // ... right?   
        allocateLocal(currCtx, param.symbol);
    }

    std::cout << "Local alloc done. body check." << std::endl;
    // 3. Body
    e.body->accept(*this);
    std::cout << "body check done" << std::endl;

    int envSlot = 0;

    // Env genreation.
    // Very simple principle: if children closures capture any of my locals or upvalues,
    // Give it an envSlot.
    // captured locals
    for (auto& local : currCtx->locals) {
        if (local.captured) {
            currCtx->envMap[local.sym] = envSlot++;
        }
    }

    // re-exported upvalues
    for (auto& up : currCtx->upvalues) {
        if (up.capturedByChildren) {
            currCtx->envMap[up.symbol] = envSlot++;
        }
    }

    currCtx->envSize = envSlot;

    currCtx = fnCtx->parent;
    
    std::cout << "framesize: " << fnCtx->nextSlot << std::endl;
    std::cout << "upvalue cnt: " << fnCtx->upvalues.size() << std::endl;

    e.functionContext = fnCtx;
    
}

void ClosureAnalyser::visit(ThisExpr& e) {
    // Note that "this" can be captured too!
    printLog(LogLevel::DEBUG, "Visiting ThisExpr.");
    if (!e.resolved) {
        e.resolved = true;
        e.resolution = resolveVariable(e.symbol);
    }
    printLog(LogLevel::DEBUG, "ThisExpr visit done.");
}

// Statements
void ClosureAnalyser::visit(Block& s) {
    currCtx->scopeDepth++;   // ENTER scope

    for (auto& stmt : s.statements) {
        stmt->accept(*this);
    }

    currCtx->scopeDepth--;   // EXIT scope

    // remove locals declared in this scope
    return;
    // Why?
    while (!currCtx->locals.empty() && currCtx->locals.back().scopeDepth > currCtx->scopeDepth) {
        auto& local = currCtx->locals.back();

        if (local.captured) {
            //emit(Opcode::CLOSE_UPVALUE, local.slot);
        }

        currCtx->locals.pop_back();
    }
}

void ClosureAnalyser::visit(Let& s) {
    if (s.expr)
        s.expr->accept(*this);
    
    if (s.isFunctionDecl) {
        // H A C K?
        currCtx->funcDecls.push_back({
            s.symbol,
            std::static_pointer_cast<FunctionExpr>(s.expr)
        });
        // Make the declaration's emitted code label available to qualified
        // imported references (`math::triple`) in a different module.
        s.symbol->nativeFunctionId = std::static_pointer_cast<FunctionExpr>(s.expr)->functionId;
    }

    // Hmm, allocateLocal is used all over I think. Some places I think unnecessary.
    int slot;
    auto it = currCtx->localMap.find(s.symbol);
    if (it != currCtx->localMap.end()) {
        // Already allocated.
        slot = it->second;
    } else {
        slot = allocateLocal(currCtx, s.symbol);
    }
    s.symbol->slot = slot;
}

void ClosureAnalyser::visit(Aggregate& s) {
    std::cout << "Entering class: \n";

    int offset = 0;
    for (auto& field : s.fieldMembers) {
        offset++;
        if (field.initialiser) {
            field.initialiser->accept(*this);
        }
    }

    for (auto& method : s.methodMembers) {
        method.methodExpr->accept(*this);
        // The symbol is shared by all importing modules, unlike an IRBuilder
        // instance.  Preserve the emitted native label on that symbol.
        method.symbol->nativeFunctionId = method.methodExpr->functionId;
    }

    for (auto& method : s.constructorMembers) {
        method.initFuncExpr->isConstructor = true;
        method.initFuncExpr->accept(*this);
        method.symbol->nativeFunctionId = method.initFuncExpr->functionId;
    }

    s.fieldInitFunc->accept(*this);
    auto* aggregateType = static_cast<InstanceType*>(s.typeSymbol->type);
    aggregateType->fieldInitializerFunctionId = s.fieldInitFunc->functionId;

    s.fieldCount = offset;
    std::cout << "member check done" << std::endl;
}

void ClosureAnalyser::visit(Enum& s) {
    EnumType* enumType = static_cast<EnumType*>(s.typeSymbol->type);

    int num = 0;
    for (auto& str : s.variants) {
        enumType->variantMap[str] = num;
    }
}
