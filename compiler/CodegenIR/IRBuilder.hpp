#pragma once

#include <vector>
#include "BasicBlock.hpp"
#include "IRInstr.hpp"
#include "IRValue.hpp"
#include "IRFunction.hpp"
#include "../Core/visitor.hpp"
#include "../Core/Ast.hpp"
#include "CommonDef.hpp"
#include "StringPool.hpp"

struct Module;

struct LoopContext {
    BasicBlock<IRInstr>* continueTarget;
    BasicBlock<IRInstr>* breakTarget;
};

enum class StorageType {
    SSA,
    STACK,
    CELL,
};

struct IRLocalInfo {
    IRValue value;
    bool isCell = false; // If this local is a cell.
    
    StorageType storageType = StorageType::SSA; // TODO: Later consider timing when to promote to cell.
};

// This is temporary! Only lived in this file.
struct IRCodegenFnCtx {
    IRCodegenFnCtx* parent = nullptr;
    int nextId = 0;

    // Locals
    HIROperand lastValue;
    std::unordered_map<VarSymbol*, IRLocalInfo> locals;
    
    // Upvalues
    std::unordered_map<VarSymbol*, IRValue> upvalues;
    
    // Loops
    std::vector<LoopContext> loopStack;
    
    // Blocks
    int nextBlockId = 0;
    BasicBlock<IRInstr>* currBlock = nullptr;

    // Env
    std::optional<IRValue> env; // The env this function is making for children (if any)
    std::optional<IRValue> incomingEnv; // The env this function is taking from parent (if any)


    FunctionContext* fnCtx;
};

class IRBuilder : public Visitor {    
public:
    // All per-module builders share this pool because one assembly file emits
    // every module's code and therefore has one string-data section.
    IRBuilder(Module& module, StringPool& stringPool, std::vector<int> initializerIds = {});

    std::vector<IRFunction<IRInstr>*> compile();

    const StringPool& getStringPool() const { return stringPool; }

private:
    // Readonly AST
    const FunctionExprPtr program;
    StringPool& stringPool;
    std::vector<int> initializerIds;
    FunctionExpr* moduleInitializer = nullptr;

    void emit(const IRInstr& instr);

    // Current function context
    IRCodegenFnCtx* currCtx = nullptr;

    bool compilingAggregateMember = false;

    // Blocks
    HIRBlock* makeBlock();
    void connectBlock(HIRBlock* from, HIRBlock* to);

    // Code
    IRValue makeValue(Type* type);

    inline HIROperand getLastValue();
    inline void setLastValue(HIROperand value);

    // Functions
    IRFunction<IRInstr>* currFunc = nullptr;
    std::vector<IRFunction<IRInstr>*> functions;

    // Helper
    void bindLocalDefinition(
        VarSymbol* sym,
        const HIROperand& value
    );

    // Expressions
    void visit(Literal& e) override;
    void visit(ArrayLiteral& e) override;
    void visit(RecordLiteral& e) override;
    void visit(Variable& e) override;
    void visit(BinaryExpr& e) override;
    void visit(UnaryExpr& e) override;
    void visit(Assignment& e) override;
    void visit(Index& e) override;
    void visit(Call& e) override;
    void visit(Get& e) override;
    void visit(ScopeAccessExpr& e) override;
    void visit(FunctionExpr& e) override;
    void visit(ThisExpr& e) override;
    void visit(NewExpr& e) override;

    // Statements
    void visit(Print& s) override;
    void visit(If& s) override;
    void visit(While& s) override;
    void visit(Block& s) override;
    void visit(Break& s) override;
    void visit(Continue& s) override;
    void visit(Let& s) override;
    void visit(Return& s) override;
    void visit(Aggregate& s) override;
    void visit(TypeAlias& s) override;
    void visit(ExprStmt& s) override;
    void visit(Enum& s) override;    
};
