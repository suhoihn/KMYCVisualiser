#pragma once

#include "../Core/Ast.hpp"
#include "../Core/Scope.hpp"
#include "../Core/Symbol.hpp"
#include "../Utils/DefaultVisitor.hpp"

struct Module;

// Pass 1: Scope & Symbol Construction
// Builds scope hierarchy and registers all "order-independent" declarations
// - function DECL, aggregate itself's definition, type alias are order-independent
//      - fun main() {}
//      - class X {};
//      - typealias Y = X;
//      - Aggregate's member decls except fields
// - variables, including function EXPR are order-dependent (they are handled in resolver.)
//      - let x = y;
//      - let f = fun() {};
//      - Aggregate's field member decls
// Creates symbols and stores them in the correct scope.
// Symbols are semantic identities of a variable, function (which are variables here)

// A Symbol represents a semantic binding/declaration in the program.
//
// It is NOT just a name string.
//
// A symbol uniquely identifies a declared entity such as:
// - variable
// - parameter
// - function
// - field
// - method
// - class/record
//
// Multiple declarations with the same textual name
// produce different symbols due to lexical scoping.
//
// Example:
//
//     let x = 1;
//     {
//         let x = 2;
//     }
//
// The two `x` declarations are different symbols.
//
// Symbols store semantic/compiler information such as:
// - declared name
// - mutability
// - resolved type
// - scope ownership
// - stack/local slot
// - closure capture info
// - runtime metadata
//
// AST identifier nodes resolve to symbols during semantic analysis.

class SymbolScopeBuilder : public DefaultVisitor {
public:
    SymbolScopeBuilder(Module& module);
    Scope* analyse();

private:
    Module& module;

    Scope* globalScope;
    Scope* currScope;

    void enterScope();
    void exitScope();

    VarSymbol* declareVar(const std::string& name, bool isMutable);
    TypeSymbol* declareType(const std::string& name, bool isMutable);

    // Expressions
    void visit(RecordLiteral& e) override;
    void visit(FunctionExpr& e) override;

    // Statements
    void visit(Block& s) override;
    void visit(Let& s) override;
    void visit(Aggregate& s) override;
    void visit(TypeAlias& s) override;
    void visit(Enum& s) override;
};
