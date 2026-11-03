#pragma once

#include <vector>
#include "../Core/Ast.hpp"
#include "../Core/Scope.hpp"
#include "../Core/Symbol.hpp"
#include "../Utils/DefaultVisitor.hpp"
#include "TypeInterner.hpp"

struct Module;

// Pass 2: Name Resolution
// Resolves identifier usages to their declared symbols using the scope tree built in Pass 1.
// Binds variables/functions/classes to Symbol objects and reports undefined references.
// Also checks valid loop break/continue, return statements.
// Also does some type building.

class Resolver : public DefaultVisitor {
public:
    Resolver(Module& module);
    void resolve();

private:
    Module& module;
    // TypeInterner typeInterner;
    Scope* globalScope;
    Scope* currScope;

    // Type related
    Type* expectedType = nullptr;
    //std::unordered_map<TypeKey, Type*> typeCache;

    bool assigning = false;

    int loopDepth = 0;

    VarSymbol* lookupVarSymbol(const std::string& name);

    InstanceType* currentAggregate = nullptr;
    VarSymbol* currentThis = nullptr; 

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
    void visit(Enum& s) override;
    void visit(ExprStmt& s);
};
