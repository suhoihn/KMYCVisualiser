#pragma once

#include <vector>
#include "../Core/Ast.hpp"
#include "../Core/value.hpp"
#include "../Core/CodegenInfo.hpp"
#include "../Core/visitor.hpp"
#include "../BytecodeVM/vm.hpp"
#include "../Semantics/Resolver.hpp"

struct Module;

struct CodegenFnCtx {
    int scopeDepth = 0;
    std::vector<Local> locals;
    std::vector<UpvalueInfo> upvalues;
    std::unordered_map<ConstValue, int> constantMap; // Constants are accessed via chunk.constants
    CodegenFnCtx* parent = nullptr;
    Chunk chunk;
};

struct CodegenLoopCtx {
    int continuePos;
    std::vector<int> breakPositions;
};

class Compiler : public Visitor {
public:
    Compiler(Module& module);
    std::vector<FunctionProto> compile(void);
private:
    const FunctionExprPtr program; // AST (read-only)

    void emit(Opcode op, int operand);
    void emitResolutionResult(ResolvedVar res);
    void handleAssignment(AssignmentOp op, ExprPtr left, ExprPtr right);
    
    // Tracking current function context.
    CodegenFnCtx* currCtx;

    // Function Prototypes (immutable function codes)
    int allocateFuncProto(const FunctionProto& fnProto);
    std::vector<FunctionProto> funcProtos;
    int funcProtoCnt = 0;

    // Globals
    std::unordered_map<std::string, int> globals;

    // For locals    
    void allocateLocal(VarSymbol* sym);

    // For constants
    int addConstant(const ConstValue& v);

    // For branches
    int emitJump(Opcode op);
    void patchJump(int pos);

    // For loops
    std::vector<CodegenLoopCtx> loopStack;

    // For aggregates
    bool compilingMethod = false;
    bool isConstructor = false;
    InstanceType* currAggType = nullptr; // HACK: rly feals like hack.
    std::unordered_map<InstanceType*, int> fieldInitFuncProtoIdx;

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
