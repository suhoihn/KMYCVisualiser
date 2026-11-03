#include "parser.hpp"
#include "errorhandler.hpp"
#include "utils.hpp"

Parser::Parser(const vector<Token>& tokens) : tokens(tokens) {}

// program → statement* EOF
vector<StmtPtr> Parser::parse() {
    vector<StmtPtr> statements;
    while (!is_at_end()) {
        statements.push_back(parse_statement());
    }
    return statements;
}

Token Parser::peek() const {
    return tokens[current];
}

Token Parser::previous() const {
    if (current - 1 < 0) {
        return Token{TokenType::Unknown, "", peek().line, peek().column, peek().startIdx, peek().endIdx};
    }
    return tokens[current - 1];
}

Token Parser::advance() {
    if (!is_at_end()) {
        ++current;
    }
    return tokens[current - 1];
}

// IMPORTANT: match consumes the token if it matches!
bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    return peek().type == type;
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::EndOfFile;
}

Token Parser::consume(TokenType token, const string& errMsg) {
    if (!check(token)) {
        throw KMYParseError(errMsg);
    }
    return advance();
}

void Parser::consumeSemicolon() {
    consume(TokenType::Semicolon, "Expected ';' after statement.");
}

// block → "{" statement* "}"
StmtPtr Parser::parse_block() {
    vector<StmtPtr> statements;
    
    consume(TokenType::LeftBrace, "Expected '{'");

    while (!check(TokenType::RightBrace) && !is_at_end()) {
        statements.push_back(parse_statement());
    }
    
    consume(TokenType::RightBrace, "Expected '}'");
    
    return make_shared<Block>(
        move(statements)
    );
}

/*
statement → (
    print_stmt |
    if_stmt |
    while_stmt | 
    block |
    break_stmt |
    continue_stmt |
    let_stmt |
    funtionDecl |
    for_stmt |
    class_stmt |
    expression (wrapped in ExprStmt) ";"
)

print_stmt → "print" '(' expression ')' ';'
// You can have statement instead of blocks, which is more complex.
if_stmt → "if" '(' expression ')' block else_clause?
else_clause -> "else" (block | if_stmt)
while_stmt → "while" '(' expression ')' block
break_stmt -> "break" ';'
continue_stmt -> "continue" ';'
let_stmt -> "let" "const"? IDENTIFIER ( '=' expression )? ';'
*/ 
StmtPtr Parser::parse_statement() {
    if (check(TokenType::LeftBrace)) {
        // Do not consume the '{' here since parse_block expects to see it and will consume it.
        return parse_block();

    } else if (match(TokenType::KeywordPrint)) {
        consume(TokenType::LeftParen, "Expected '(' after print statement.");
        ExprPtr expr = parse_expression();
        consume(TokenType::RightParen, "Expected ')' after expression.");
        consumeSemicolon();

        return make_shared<Print>(move(expr));

    } else if (match(TokenType::KeywordIf)) {
        consume(TokenType::LeftParen, "Expected '(' after if statement.");
        ExprPtr condition = parse_expression();
        consume(TokenType::RightParen, "Expected ')' after condition expression.");

        StmtPtr thenbranch = parse_block();
        StmtPtr elsebranch = nullptr;
        
        if (match(TokenType::KeywordElse)) {
            elsebranch = parse_statement();
        }

        return make_shared<If>(
            move(condition),
            move(thenbranch),
            move(elsebranch)
        );

    } else if (match(TokenType::KeywordWhile)) {
        consume(TokenType::LeftParen, "Expected '(' after while statement.");
        ExprPtr condition = parse_expression();
        consume(TokenType::RightParen, "Expected ')' after condition expression.");
        StmtPtr body = parse_block();

        return make_shared<While>(
            move(condition),
            move(body)
        );

    } else if (match(TokenType::KeywordFor)) {
        return parse_for();

    } else if (match(TokenType::KeywordBreak)) {
        consumeSemicolon();
        return make_shared<Break>();

    } else if (match(TokenType::KeywordContinue)) {
        consumeSemicolon();
        return make_shared<Continue>();
    
    } else if (match(TokenType::KeywordLet)) {
        return parse_let();
    } else if (match(TokenType::KeywordFun)) {
        return parse_functionDecl();
    } else if (match(TokenType::KeywordReturn)) {
        ExprPtr expr = parse_expression();
        consumeSemicolon();
        return make_shared<Return>(move(expr));
    } else if (match(TokenType::KeywordClass)) {
        return parse_class();
    } else {
        ExprPtr expr = parse_expression();
        consumeSemicolon();
        return make_shared<ExprStmt>(move(expr));
    }
}

StmtPtr Parser::parse_let() {
    // consume(TokenType::KeywordLet, "Expected Let keyword. If this error is thrown in normal var decl, contact KMY.");

    bool isMutable = !match(TokenType::KeywordConst);

    consume(TokenType::Identifier, "Expected an identifier.");
    
    Token varToken = previous();
    ExprPtr initialiser = nullptr;

    if (match(TokenType::Assign)) {
        // let x = (smth) form.
        initialiser = parse_expression();
    }
    // "let x;" form
    consumeSemicolon();
    return make_shared<Let>(
        varToken.lexeme,
        move(initialiser),
        isMutable
    );
}

// functionDecl → "fun" IDENTIFIER (funtionExpr without initial "fun")
StmtPtr Parser::parse_functionDecl() {
    // consume(TokenType::KeywordFun, "Expected fun keyword. If this error is thrown in normal func decl, contact KMY.");

    consume(TokenType::Identifier, "Expected an identifier.");
    Token name = previous(); // Gets the previous identifier token.

    ExprPtr fnExpr = parse_functionExpr();

    // desugar: 
    return make_shared<Let>(
        name.lexeme,
        fnExpr,
        false // Function declarations are immutable for default.
    );
}

// for_stmt → "for" '(' initialiser? ';' condition? ';' increment? ')' block
StmtPtr Parser::parse_for() {
    consume(TokenType::LeftParen, "Expected '(' at the start of a for loop.");

    StmtPtr initialiser = nullptr;
    ExprPtr condition = nullptr;
    ExprPtr increment = nullptr;

    if (match(TokenType::Semicolon)) {
        initialiser = nullptr;
    } else if (check(TokenType::KeywordLet)) {
        // Kinda cheating, but only allowing let statements.
        initialiser = parse_statement(); // This consumes ';'
    } else {
        initialiser = make_shared<ExprStmt>(parse_expression());
        consumeSemicolon();
    }
        
    if (!match(TokenType::Semicolon)) {
        // If there is something...
        condition = parse_expression();
        consumeSemicolon();
    }

    // If there is something...
    if (!check(TokenType::RightParen)) {
        increment = parse_expression();
    }
    
    consume(TokenType::RightParen, "Expected ')' at the end of a for loop.");

    StmtPtr body = parse_block();

    // add increment at end of body
    if (increment != nullptr) {
        body = make_shared<Block>(
            vector<StmtPtr>{
                body,
                make_shared<ExprStmt>(increment)
            }
        );
    }

    // default condition = true
    if (condition == nullptr) {
        condition = make_shared<Literal>(Value(true));
    }

    // while loop
    body = make_shared<While>(move(condition), move(body));

    // wrap with initializer
    if (initialiser != nullptr) {
        body = make_shared<Block>(
            vector<StmtPtr>{
                initialiser,
                body
            }
        );
    }

    return body;
}

/*
class_stmt   → "class" IDENTIFIER '{' class_member* '}' ';'
class_member → functionDecl | let_stmt
*/
StmtPtr Parser::parse_class() {
    //consume(TokenType::KeywordClass, "Expected class keyword.");

    consume(TokenType::Identifier, "Expected an identifier.");
    Token name = previous(); // Gets the previous identifier token.

    consume(TokenType::LeftBrace, "Expected '{'");
    vector<FieldMember> fieldMembers;
    vector<MethodMember> methodMembers;
    while (!match(TokenType::RightBrace)) {
        if (match(TokenType::KeywordLet)) {
            shared_ptr<Let> letStmt = static_pointer_cast<Let>(parse_let());
            fieldMembers.push_back(
                FieldMember(
                    move(letStmt->name),
                    move(letStmt->expr),
                    letStmt->isMutable
                )
            );
        } else if (match(TokenType::KeywordFun)) {
            // Desugared form: let [functionName] = [functionExpr]
            shared_ptr<Let> letStmt = static_pointer_cast<Let>(parse_functionDecl());
            methodMembers.push_back(
                MethodMember(
                    move(letStmt->name),
                    // This must be FunctionExpr.
                    static_pointer_cast<FunctionExpr>(move(letStmt->expr))
                )
            );
        } else {
            throw KMYParseError("Illegal statement detected.");
        }
    }

    consumeSemicolon();

    return make_shared<Class>(
        move(name.lexeme),
        move(fieldMembers),
        move(methodMembers)
    );
}



// TODO: combine parsing (args?) form to one func.

// expression just delegates to assignment
ExprPtr Parser::parse_expression() {
    return parse_assignment();
}


// Note that '=' can be any assignment operator.
// assignment → (postfix '=' assignment) | postfix
// But the code will perform:
// assignment → or '=' assignment
// The above structure allows chaining like a = b[0] += c = 3
// The code calls parse_or(), so weird things like (a || true) = 3 are allowed.
// (a + 3) = 3 is syntactically valid, but semantically invalid.
// but the interpreter will reject it.

// This structure allows for right-associative assignment operators and
// ensures that they have lower precedence than all other operators.
// Note that parser doesnt handle semantics.
ExprPtr Parser::parse_assignment() {
    ExprPtr left = parse_or();
    if (
        match(TokenType::Assign) ||
        match(TokenType::PlusAssign) ||
        match(TokenType::MinusAssign) ||
        match(TokenType::StarAssign) ||
        match(TokenType::SlashAssign) ||
        match(TokenType::PercentAssign)
    ) {
        Token assignToken = previous();
        // The assignment operator token we just matched
        ExprPtr right = parse_assignment();
        // a = b = c
        return make_shared<Assignment>(
            assignToken.type,
            move(left),
            move(right)
        );
    }
    return left;
}

// or -> and ("||" and)*
ExprPtr Parser::parse_or() {
    ExprPtr left = parse_and();
    while (
        match(TokenType::OrOr)
    ) {
        Token prevOpToken = previous(); // The operator token we just matched
        ExprPtr right = parse_and();
        left = make_shared<BinaryOp>(
            prevOpToken.type,
            move(left),
            move(right)
        );
        prevOpToken = peek();
    }
    return left;
}

// and -> equality ("&&" equality)*
ExprPtr Parser::parse_and() {
    ExprPtr left = parse_equality();
    while (
        match(TokenType::AndAnd)
    ) {
        Token prevOpToken = previous(); // The operator token we just matched
        ExprPtr right = parse_equality();
        left = make_shared<BinaryOp>(
            prevOpToken.type,
            move(left),
            move(right)
        );
    }
    return left;
}

// equality → comparison (("==" | "!=") comparison)*
ExprPtr Parser::parse_equality() {
    ExprPtr left = parse_comparison();
    while (
        match(TokenType::EqualEqual) ||
        match(TokenType::NotEqual)
    ) {
        Token prevOpToken = previous(); // The operator token we just matched
        ExprPtr right = parse_comparison();
        left = make_shared<BinaryOp>(
            prevOpToken.type,
            move(left),
            move(right)
        );
    }
    return left;
}

// comparison → term ((">" | ">=" | "<" | "<=") term)*
ExprPtr Parser::parse_comparison() {
    ExprPtr left = parse_term();
    while (
        match(TokenType::Greater) ||
        match(TokenType::GreaterEqual) ||
        match(TokenType::Less) ||
        match(TokenType::LessEqual)
    ) {
        Token prevOpToken = previous(); // The operator token we just matched
        ExprPtr right = parse_term();
        left = make_shared<BinaryOp>(
            prevOpToken.type,
            move(left),
            move(right)
        );
    }
    return left;
}

// term → factor (("+" | "-") factor)*
ExprPtr Parser::parse_term() {
    ExprPtr left = parse_factor();
    while (
        match(TokenType::Plus) ||
        match(TokenType::Minus)
    ) {
        Token prevOpToken = previous(); // The operator token we just matched
        ExprPtr right = parse_factor();
        left = make_shared<BinaryOp>(
            prevOpToken.type,
            move(left),
            move(right)
        );
    }
    return left;
}

// factor → unary (("*" | "/" | "%") unary)*
ExprPtr Parser::parse_factor() {
    ExprPtr left = parse_unary();
    while (
        match(TokenType::Star) ||
        match(TokenType::Slash) ||
        match(TokenType::Percent)
    ) {
        Token prevOpToken = previous(); // The operator token we just matched
        ExprPtr right = parse_unary();
        left = make_shared<BinaryOp>(
            prevOpToken.type,
            move(left),
            move(right)
        );
    }
    return left;   
}

// unary → (("!" | "-" | "+") unary) | postfix
ExprPtr Parser::parse_unary() {
    if (match(TokenType::Bang) ||
        match(TokenType::Minus) ||
        match(TokenType::Plus)) {
        
        Token opToken = previous(); // The operator token we just matched

        // recursion since unary operators are right associative
        ExprPtr operand = parse_unary();

        return make_shared<UnaryOp>(
            opToken.type,
            // unique_ptrs are move-only, so we need to use std::move to transfer ownership.
            move(operand)
        );
    }

    return parse_postfix();
}

// postfix → primary ( "[" expression "]" | "(" args? ")" | "." identifier)*
ExprPtr Parser::parse_postfix() {
    ExprPtr expr = parse_primary();
    while (true) {
        // Indexing
        if (match(TokenType::LeftBracket)) {
            ExprPtr index = parse_expression();
            consume(TokenType::RightBracket, "Expected ']' after indexing.");

            expr = make_shared<Index>(
                move(expr),
                move(index)
            );
        }

        // Function calls.
        else if(match(TokenType::LeftParen)) {
            vector<ExprPtr> args;
            if (!check(TokenType::RightParen)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenType::Comma));
            }

            consume(TokenType::RightParen, "Expected ')' after call.");


            expr = make_shared<Call>(
                move(expr),
                move(args)
            );
        }

        // Get expression.
        else if (match(TokenType::Dot)) {
            consume(TokenType::Identifier, "Expected an identifier in a dot expression.");
            Token name = previous();
            expr = make_shared<Get>(
                move(expr),
                name.lexeme
            );
        } 
        
        else {
            break;
        }
    }
    return expr;
}

/*
array_elements → expression ("," expression)*
primary → 
    LITERAL |
    '(' expression ')'
    '[' array_elements? ']' |
    object |
    fnExpr |
    newExpr
*/
ExprPtr Parser::parse_primary() {
    Token prevToken = peek();
    if (match(TokenType::Int)) {
        // Arguments are forwarded to the constructor.
        return make_shared<Literal>( Value(stoi(prevToken.lexeme)) );
    } else if (match(TokenType::Double)) {
        return make_shared<Literal>( Value(stod(prevToken.lexeme)) );
    } else if (match(TokenType::StringLiteral)) {
        return make_shared<Literal>( Value(prevToken.lexeme) );
    } else if (match(TokenType::KeywordNull)) {
        return make_shared<Literal>( Value(nullptr) );
    } else if (match(TokenType::KeywordTrue)) {
        return make_shared<Literal>( Value(true) );
    } else if (match(TokenType::KeywordFalse)) {
        return make_shared<Literal>( Value(false) );
    } else if (match(TokenType::Identifier)) {
        return make_shared<Variable>( prevToken.lexeme );
    } else if (match(TokenType::LeftParen)) {
        ExprPtr expr = parse_expression();
        consume(TokenType::RightParen, "Expected ')' after expression.");
        return expr;
    } else if (match(TokenType::LeftBracket)) {
        vector<ExprPtr> elements;
        if (!check(TokenType::RightBracket)) {
            do {
                elements.push_back(parse_expression());
            } while(match(TokenType::Comma));
        }
        
        consume(TokenType::RightBracket, "Expected ']' after array literal.");

        return make_shared<ArrayLiteral>(
            move(elements)
        );

    } else if (match(TokenType::KeywordFun)) {
        return parse_functionExpr();
    } else if (match(TokenType::LeftBrace)) {
        // '{' in EXPRESSION handles objects, not blocks!
        return parse_objectExpr();
    } else if (match(TokenType::KeywordThis)) {
        return make_shared<ThisExpr>();
    } else if (match(TokenType::KeywordNew)) {
        return parse_newExpr();
    } 
    else {
        // Handle error: unexpected token
        //cout << "Unexpected token: " << tokenTypeToString(peek().type) << " with lexeme \"" << peek().lexeme << "\" at line " << peek().line << ", column " << peek().column << endl;
        throw KMYParseError("Unexpected token.");
    }
}


/*
fnExpr → "fun" '(' params? ')' block
params → param (',' param)*
param  → "const"? ( ( IDENTIFIER ('=' expression)? ) | "..." IDENTIFIER )

fnExpr is more like a lambda function!
*/
ExprPtr Parser::parse_functionExpr() {
    consume(TokenType::LeftParen, "Expected '(' after function declaration.");

    vector<Parameter> params;
    bool defaultSeen = false;
    if (!check(TokenType::RightParen)) {
        do {
            bool isMutable = !match(TokenType::KeywordConst);
            if (match(TokenType::Ellipsis)) {

                Token name = consume(TokenType::Identifier, "Expected an identifier.");
                
                if (!check(TokenType::RightParen)) {
                    throw KMYParseError("Variadic parameter should come at the end of the parameter list. Didn't see ')'");
                }
                params.push_back(
                    Parameter(Type(TypeKind::ANY), name.lexeme, true, isMutable, false, nullptr) 
                );
                break;
            }

            Token name = consume(TokenType::Identifier, "Expected an identifier.");
            Type type = Type(TypeKind::ANY);
            if (match(TokenType::Colon)) {
                // Type signature exists.
                type = parse_type();
            }
            
            ExprPtr defaultValue = nullptr;
            bool isDefault;
            if (isDefault = match(TokenType::Assign)) {
                defaultSeen = true;
                defaultValue = parse_expression();
            } else if (defaultSeen) {
                throw KMYParseError("Non-default parameter detected after a default parameter.");
            }
            
            params.push_back(
                Parameter(type, name.lexeme, false, isMutable, isDefault, defaultValue) 
            );
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "Expected ')' after function parameters.");

    StmtPtr body = parse_block();

    return make_shared<FunctionExpr>(
        move(params),
        move(body)
    );
}


/*
object → '{' (pair (',' pair)*)? '}'
pair   → IDENTIFIER ':' expression
*/
ExprPtr Parser::parse_objectExpr() {
    unordered_map<string, ExprPtr> fields;

    if (check(TokenType::RightBrace)) {
        advance();
        return make_shared<ObjectLiteral>(
            move(fields)
        );
    }

    do {
        if (!match(TokenType::Identifier)) {
            throw KMYParseError("Expected an identifier.");
        }
        Token name = previous();
        if (!match(TokenType::Colon)) {
            throw KMYParseError("Expected ':'");
        }
        ExprPtr expr = parse_expression();
        fields[name.lexeme] = expr;

    } while (match(TokenType::Comma));
    
    if (!match(TokenType::RightBrace)) {
        throw KMYParseError("Expected '}' after object declaration.");
    }

    return make_shared<ObjectLiteral>(
        move(fields)
    ); 
}  


/*
newExpr → "new" IDENTIFIER '(' args? ')'
args    → expression ("," expression)*
*/
ExprPtr Parser::parse_newExpr() {
    consume(TokenType::Identifier, "Expected an identifier.");
    Token name = previous();

    consume(TokenType::LeftParen, "Expected '(' after class name.");
    vector<ExprPtr> args;

    if (!check(TokenType::RightParen)) {
        do {
            args.push_back(parse_expression());
        } while (match(TokenType::Comma));
    }
    
    consume(TokenType::RightParen, "Expected ')' after class constructor args.");

    return make_shared<NewExpr>(
        name.lexeme,
        move(args)
    );
}

/*
parseExpression() // handles operator precedence and associativity
 └── parseAssignment() // =, +=, -=, *=, /=
      └── parseOr() // ||
           └── parseAnd() // &&
                └── parseEquality() // ==, !=
                     └── parseComparison() // >, <, >=, <=
                          └── parseTerm() // +, -
                               └── parseFactor() // *, /, %
                                    └── parseUnary() // !, -, +
                                        └── parsePostfix() // a[0], f(2), obj.property etc.
                                            └── parsePrimary() // NUMBER, IDENTIFIER, "(", STRING, NULL, BOOLEAN, ARRAY
*/

/*
I saw this and ditched this recursive descent parsing method.
parseExpression()
 └── parseAssignment() // =, +=, ...
      └── parseOr() // ||
           └── parseAnd() // &&
                └── parseBitwiseOr() // |
                     └── parseBitwiseXor() // ^
                          └── parseBitwiseAnd() // &
                               └── parseEquality() // ==, !=
                                    └── parseComparison() // <, >, <=, >=
                                         └── parseShift() // << >>
                                              └── parseTerm() // +, -
                                                   └── parseFactor() // *, /, %
                                                        └── parseUnary() // !, -, +, ~
                                                             └── parsePostfix() // a[0], f(2), obj.property etc.
                                                                  └── parsePrimary() // NUMBER, IDENTIFIER, "(", STRING, NULL, BOOLEAN, ARRAY
*/