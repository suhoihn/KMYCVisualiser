// TODO: WILL BE COMBINED WITH COMPILER.

#pragma once

#include <vector>
#include "../Core/Symbol.hpp" 
#include "../Core/Scope.hpp" 
#include "../Core/Ast.hpp"
#include "../Core/FunctionContext.hpp"
#include "../Utils/DefaultVisitor.hpp"

struct Module;

// Pass 3: Closure Analysis + Slot Allocation
// Determines runtime storage for variables.
// Detects captured variables/upvalues and assigns local stack slots
// and closure indices required for code generation.


// Forward decls.
static int allocateLocal(FunctionContext* fnCtx, VarSymbol* sym);
static int resolveUpvalue(FunctionContext* fnCtx, VarSymbol* sym);
static bool isInsideFunction(VarSymbol* sym, Scope* functionScope);


class ClosureAnalyser : public DefaultVisitor {
public:
    // `nextFunctionId` belongs to the whole native compilation, not one
    // source file: x86 labels must remain unique after modules are combined.
    ClosureAnalyser(Module& module, int& nextFunctionId);
    void analyse();

private:
    Module& module;
    
    FunctionContext* currCtx;
    ResolvedVar resolveVariable(VarSymbol* sym);

    bool insideMethod = false;

    Aggregate* currentAggregate = nullptr;

    // ID given to each function expr.
    int& nextFunctionId;

    // Expressions
    void visit(Variable& e) override;
    void visit(FunctionExpr& e) override;
    void visit(ThisExpr& e) override;

    // Statements
    void visit(Block& s) override;
    void visit(Let& s) override;
    void visit(Aggregate& s) override;
    void visit(Enum& s) override;
};
