#pragma once

// All AST nodes forward declared.

void traceAstVisit(const char* node);

template <typename T> inline constexpr const char* astNodeName = "ASTNode";
#define KMY_AST_NAME(T) template <> inline constexpr const char* astNodeName<struct T> = #T
KMY_AST_NAME(Literal);
KMY_AST_NAME(ArrayLiteral);
KMY_AST_NAME(RecordLiteral);
KMY_AST_NAME(Variable);
KMY_AST_NAME(BinaryExpr);
KMY_AST_NAME(UnaryExpr);
KMY_AST_NAME(Assignment);
KMY_AST_NAME(Index);
KMY_AST_NAME(Call);
KMY_AST_NAME(Get);
KMY_AST_NAME(ScopeAccessExpr);
KMY_AST_NAME(FunctionExpr);
KMY_AST_NAME(ThisExpr);
KMY_AST_NAME(NewExpr);
KMY_AST_NAME(Print);
KMY_AST_NAME(If);
KMY_AST_NAME(While);
KMY_AST_NAME(Block);
KMY_AST_NAME(Break);
KMY_AST_NAME(Continue);
KMY_AST_NAME(Let);
KMY_AST_NAME(Return);
KMY_AST_NAME(Aggregate);
KMY_AST_NAME(TypeAlias);
KMY_AST_NAME(Enum);
KMY_AST_NAME(ExprStmt);
#undef KMY_AST_NAME

class Visitor {
public:
    // Expressions
    virtual void visit(struct Literal& e) = 0;
    virtual void visit(struct ArrayLiteral& e) = 0;
    virtual void visit(struct RecordLiteral& e) = 0;
    virtual void visit(struct Variable& e) = 0;
    virtual void visit(struct BinaryExpr& e) = 0;
    virtual void visit(struct UnaryExpr& e) = 0;
    virtual void visit(struct Assignment& e) = 0;
    virtual void visit(struct Index& e) = 0;
    virtual void visit(struct Call& e) = 0;
    virtual void visit(struct Get& e) = 0;
    virtual void visit(struct ScopeAccessExpr& e) = 0;
    virtual void visit(struct FunctionExpr& e) = 0;
    virtual void visit(struct ThisExpr& e) = 0;
    virtual void visit(struct NewExpr& e) = 0;

    // Statements
    virtual void visit(struct Print& s) = 0;
    virtual void visit(struct If& s) = 0;
    virtual void visit(struct While& s) = 0;
    virtual void visit(struct Block& s) = 0;
    virtual void visit(struct Break& s) = 0;
    virtual void visit(struct Continue& s) = 0;
    virtual void visit(struct Let& s) = 0;
    virtual void visit(struct Return& s) = 0;
    virtual void visit(struct Aggregate& s) = 0;
    virtual void visit(struct TypeAlias& s) = 0;
    virtual void visit(struct Enum& s) = 0;
    virtual void visit(struct ExprStmt& s) = 0;

    virtual ~Visitor() = default;
};
