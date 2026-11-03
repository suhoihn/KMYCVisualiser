#pragma once
#include <string>
#include <vector>
#include "Ast.hpp"
#include "tokens.hpp"

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    std::vector<StmtPtr> parse();
    
private:

    /*
    template<typename T, typename Func>
    std::vector<T> Parser::readSeparatedList(
        Token separator,
        Func applyFunc
    );
    */

    const std::vector<Token> tokens;
    int current = 0;

    Token peek() const;
    Token previous() const;
    Token advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    bool is_at_end() const;
    Token consume(TokenType token, const std::string& errMsg);

    // Parsing statements
    StmtPtr parse_block();
    void consumeSemicolon();
    StmtPtr parse_statement();
    StmtPtr parse_let();
    StmtPtr parse_functionDecl(); // "fun f(a,b) {}" form
    StmtPtr parse_for();
    StmtPtr parse_class();

    // Parsing expressions with operator precedence (lowest to highest)
    ExprPtr parse_expression();
    ExprPtr parse_assignment();
    ExprPtr parse_or();
    ExprPtr parse_and();
    ExprPtr parse_equality();
    ExprPtr parse_comparison();
    ExprPtr parse_term();
    ExprPtr parse_factor();
    ExprPtr parse_unary();
    ExprPtr parse_postfix(); // Function calls "f()" or array indexing "a[0][1]" or member access "obj.x"
    ExprPtr parse_primary();

    ExprPtr parse_functionExpr(); // "fun(a,b) {}" form.
    ExprPtr parse_objectExpr();
    ExprPtr parse_newExpr(); // "new A(1,2)" form.
};
