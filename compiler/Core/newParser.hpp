#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include "tokens.hpp"
#include "Ast.hpp"
#include "type.hpp"
#include "ParserTrace.hpp"


// Precedence values (higher = tighter binding)
enum BindingPower {
    BP_NONE = 0,

    BP_ASSIGNMENT = 10,     // = += -= ...
    BP_LOGICAL_OR = 20,     // ||
    BP_NULL_COALESCE = 25,  // ??
    BP_LOGICAL_AND = 30,    // &&
    BP_BITWISE_OR = 40,     // |
    BP_BITWISE_XOR = 50,    // ^
    BP_BITWISE_AND = 60,    // &
    BP_EQUALITY = 70,       // == !=
    BP_COMPARISON = 80,     // < > <= >=
    BP_SHIFT = 90,          // << >>
    BP_TERM = 100,          // + -
    BP_FACTOR = 110,        // * / %
    BP_UNARY = 120,         // ! - + ~
    BP_POSTFIX = 130        // (), [], ., !!
};

struct ImportDecl {
    std::string path;
    std::string alias;
};

struct Scope;

struct Module {
    std::vector<ImportDecl> importDecls;
    std::unordered_map<std::string, Module*> imports; // alias to Module*

    // The source-level contents of this file. Semantic passes will migrate to
    // visit these directly, so a module no longer needs to be *represented* as
    // a fake zero-parameter function.
    std::vector<StmtPtr> topLevelStatements;

    // Temporary compatibility bridge during the root-function migration.
    // Existing passes still consume this synthetic wrapper; it will be removed
    // only after scopes, resolution, closure analysis, and codegen use the
    // module's real top-level statements and generated initializer instead.
    FunctionExprPtr program;
    Scope* globalScope = nullptr;
};

class Parser {
public:
    explicit Parser(
        const std::vector<Token>& tokens,
        std::vector<ParserTraceEvent>* trace = nullptr
    );

    // Entry point
    Module parse();

private:
    // =============================
    // Token Helpers
    // =============================

    Token peek() const;
    Token previous() const;
    Token advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    bool is_at_end() const;
    Token consume(TokenType token, const std::string& errMsg);

    // =============================
    // Core Pratt Parsing (Expressions)
    // =============================

    ExprPtr parse_expression(int minBP=BP_ASSIGNMENT);
    ExprPtr parse_functionExpr();
    ExprPtr parse_recordExpr();
    ExprPtr parse_newExpr();
    ExprPtr parse_prefix();

    TypeNodePtr parseTypeToken(Token t);
    TypeNodePtr parse_type(bool parseArraySuffix = true);
    TypeNodePtr parse_functionType();
    TypeNodePtr parse_arraySuffix();
    TypeNodePtr parse_recordType();
    TypeNodePtr parse_scopedType();
    
    ExprPtr finishCall(ExprPtr callee);

    int get_binding_power(TokenType type);
    bool is_right_associative(TokenType type);

    // =============================
    // Parsing statements
    // =============================
    
    void parse_import(); // File header declaration; not a runtime statement.
    StmtPtr parse_block();
    void consumeSemicolon();
    StmtPtr parse_statement();
    // The caller consumes `let` or `var` and tells this shared declaration
    // parser whether the resulting binding may be reassigned.
    StmtPtr parse_let(bool isMutable);
    StmtPtr parse_functionDecl(); // "fun f(a,b) {}" form
    StmtPtr parse_for();
    StmtPtr parse_aggregate(AggregateKind kind);
    StmtPtr parse_typeAlias();
    StmtPtr parse_enum();

    const std::vector<Token> tokens;
    std::vector<ParserTraceEvent>* trace;
    ParserTrace recorder;
    size_t current = 0;

    template<typename T, typename... Args>
    std::shared_ptr<T> makeNode(Args&&... args) {
        auto node = std::make_shared<T>(std::forward<Args>(args)...);
        recorder.created(node, current ? current - 1 : 0);
        return node;
    }

    std::vector<ImportDecl> importDecls;
};
