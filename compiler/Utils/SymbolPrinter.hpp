#pragma once

#include <vector>
#include "../Core/Ast.hpp"

std::string typeToString(Type* type);

class SymbolPrinter : public Visitor {
public:
    SymbolPrinter(FunctionExprPtr program);
    void print();
    
private:
    int depth = 0;
    std::string indent();
    void printSymbol(VarSymbol* sym);

    FunctionExprPtr program;

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
    void visit(ExprStmt& s) override;
};
