#include "newParser.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include "errorhandler.hpp"
#include "type.hpp"
#include "operators.hpp"

Parser::Parser(
    const std::vector<Token>& tokens,
    std::vector<ParserTraceEvent>* trace
) : tokens(tokens), trace(trace), recorder(trace) {}

// program → statement* EOF
Module Parser::parse() {
    auto traceFrame = recorder.scope(__func__, current);
    std::vector<StmtPtr> statements;
    parse_import();
    while (!is_at_end()) {
        statements.push_back(parse_statement());
    }
    // Keep the source file's statements on Module. The duplicate vector holds
    // shared AST nodes, not copied statements, and lets legacy passes continue
    // to use `program` while they are migrated one by one.
    std::vector<StmtPtr> topLevelStatements = statements;

    auto program = makeNode<FunctionExpr>(
        std::vector<Parameter>{}, // No params for global scope
        makeNode<Block>(move(statements)),
        nullptr, // No return annotation for global scope
        true // Entry function.
    );

    return Module {
        .importDecls = importDecls,
        .topLevelStatements = std::move(topLevelStatements),
        .program = program
    };
}

/*
Format---------- Compile-time imports

module     → importDecl* statement* EOF
importDecl → "import" STRING "as" IDENTIFIER ";"

The string names a source file relative to the importing file's directory.
The alias names that module in this file (Alias::Name). Imports belong before
all statements, and their metadata will be kept separate from runtime AST
statements. "import", "as", and "from" are lexer keywords; "from" is not
used by this proposed whole-module import grammar.
*/
void Parser::parse_import() {
    auto traceFrame = recorder.scope(__func__, current);
    while (match(TokenType::KeywordImport)) {
        // It is a string literal, not an identifier
        Token path = consume(TokenType::StringLiteral, "Expected a string literal for the import path");

        // TODO: From not implemented yet.
        Token alias;
        if (match(TokenType::KeywordAs)){
            alias = consume(TokenType::Identifier, "Expected an identifier for the import alias");
        } else {
            // Error without as for now
            throw KMYCompileError("You need 'as' for imports.");
        }
        consumeSemicolon();

        importDecls.push_back(ImportDecl{path.lexeme, alias.lexeme});
    }
}

Token Parser::peek() const {
    return tokens[current];
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::EndOfFile;
}

// Unused (previous()).

Token Parser::previous() const {
    if (current - 1 < 0) {
        return Token{TokenType::Unknown, "", peek().line, peek().column, peek().startIdx, peek().endIdx};
    }
    return tokens[current - 1];
}

// Advances current and returns the previous token.
Token Parser::advance() {
    if (!is_at_end()) {
        recorder.emit("consume", current);
        ++current;
    }
    return tokens[current - 1];
}

// Compares peek token's type and the given tokentype
bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    return peek().type == type;
}

// Returns whether the peek token's type is the given tokentype.
// IMPORTANT: match consumes the token if it matches!
bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

// Consumes the peek token if it matches the given type. 
// If matched, Returns the consumed token.
// Otherwise, throws given error message.
Token Parser::consume(TokenType token, const std::string& errMsg) {
    if (!check(token)) {
        Token top = previous();
        throw KMYParseError(errMsg, top.line, top.startIdx, top.endIdx);
    }
    return advance();
}

void Parser::consumeSemicolon() {
    consume(TokenType::Semicolon, "Expected ';' after statement");
}

// block → '{' statement* '}'
StmtPtr Parser::parse_block() {
    auto traceFrame = recorder.scope(__func__, current);
    std::vector<StmtPtr> statements;
    
    consume(TokenType::LeftBrace, "Expected '{'");

    while (!check(TokenType::RightBrace) && !is_at_end()) {
        statements.push_back(parse_statement());
    }
    
    consume(TokenType::RightBrace, "Expected '}'");
    
    return makeNode<Block>( move(statements) );
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
let_stmt -> "let" "const"? IDENTIFIER (':' type)? ( '=' expression )? ';'
*/ 
StmtPtr Parser::parse_statement() {
    auto traceFrame = recorder.scope(__func__, current);
    if (check(TokenType::LeftBrace)) {
        // Do not consume the '{' here since parse_block expects to see it and will consume it.
        return parse_block();

    } else if (match(TokenType::KeywordPrint)) {
        consume(TokenType::LeftParen, "Expected '(' after print statement");
        ExprPtr expr = parse_expression();
        consume(TokenType::RightParen, "Expected ')' after expression");
        consumeSemicolon();

        return makeNode<Print>(move(expr));

    } else if (match(TokenType::KeywordIf)) {
        consume(TokenType::LeftParen, "Expected '(' after if statement");
        ExprPtr condition = parse_expression();
        consume(TokenType::RightParen, "Expected ')' after condition expression");

        StmtPtr thenbranch = parse_block();
        StmtPtr elsebranch = nullptr;
        
        if (match(TokenType::KeywordElse)) {
            elsebranch = parse_statement(); // TODO: statement or block?
        }

        return makeNode<If>(
            move(condition),
            move(thenbranch),
            move(elsebranch)
        );

    } else if (match(TokenType::KeywordWhile)) {
        consume(TokenType::LeftParen, "Expected '(' after while statement");
        ExprPtr condition = parse_expression();
        consume(TokenType::RightParen, "Expected ')' after condition expression");
        StmtPtr body = parse_block();

        return makeNode<While>(
            move(condition),
            move(body)
        );

    } else if (match(TokenType::KeywordFor)) {
        return parse_for();

    } else if (match(TokenType::KeywordBreak)) {
        consumeSemicolon();
        return makeNode<Break>();

    } else if (match(TokenType::KeywordContinue)) {
        consumeSemicolon();
        return makeNode<Continue>();
    
    } else if (match(TokenType::KeywordLet)) {
        return parse_let(false);
    } else if (match(TokenType::KeywordVar)) {
        return parse_let(true);
    } else if (match(TokenType::KeywordFun)) {
        return parse_functionDecl();
    } else if (match(TokenType::KeywordReturn)) {
        if (check(TokenType::Semicolon)) {
            // Return without expression.
            consumeSemicolon();
            return makeNode<Return>(nullptr);
        }
        ExprPtr expr = parse_expression();
        consumeSemicolon();
        return makeNode<Return>(move(expr));
    } else if (match(TokenType::KeywordClass)) {
        return parse_aggregate(AggregateKind::CLASS);
    } else if (match(TokenType::KeywordRecord)) {
        return parse_aggregate(AggregateKind::RECORD);
    } else if (match(TokenType::KeywordTypealias)) {
        return parse_typeAlias();
    } else if (match(TokenType::KeywordEnum)) {
        return parse_enum();
    } else {
        ExprPtr expr = parse_expression();
        consumeSemicolon();
        return makeNode<ExprStmt>(move(expr));
    }
}

StmtPtr Parser::parse_let(bool isMutable) {
    auto traceFrame = recorder.scope(__func__, current);
    // `let` and `var` share the rest of their grammar. The declaration
    // keyword was consumed by parse_statement(), parse_for(), or the
    // aggregate-member parser.
    Token varToken = consume(TokenType::Identifier, "Expected an identifier");

    TypeNodePtr type = nullptr;
    if (match(TokenType::Colon)) {
        // Type signature exists.
        type = parse_type();
    }
    
    ExprPtr initialiser = nullptr;

    if (match(TokenType::Assign)) {
        // let x = (smth) form.
        initialiser = parse_expression();
    }
    // "let x;" form
    consumeSemicolon();
    return makeNode<Let>(
        type,
        varToken.lexeme,
        move(initialiser),
        isMutable,
        false // Not a direct function decl
    );
}

// functionDecl → "fun" IDENTIFIER (funtionExpr without initial "fun")
StmtPtr Parser::parse_functionDecl() {
    auto traceFrame = recorder.scope(__func__, current);
    // consume(TokenType::KeywordFun, "Expected fun keyword. If this error is thrown in normal func decl, contact KMY");

    Token name = consume(TokenType::Identifier, "Expected an identifier");

    ExprPtr fnExpr = parse_functionExpr();

    // desugar: 
    return makeNode<Let>(
        nullptr, // Function type left blank in parser phase.
        name.lexeme,
        move(fnExpr),
        false, // Function declarations are immutable for default.
        true // Special flag that this is from function decl.
    );
}

// for_stmt → "for" '(' initialiser? ';' condition? ';' increment? ')' block
StmtPtr Parser::parse_for() {
    auto traceFrame = recorder.scope(__func__, current);
    consume(TokenType::LeftParen, "Expected '(' at the start of a for loop");

    StmtPtr initialiser = nullptr;
    ExprPtr condition = nullptr;
    ExprPtr increment = nullptr;

    if (match(TokenType::Semicolon)) {
        initialiser = nullptr;
    } else if (match(TokenType::KeywordLet)) {
        initialiser = parse_let(false); // This consumes ';'
    } else if (match(TokenType::KeywordVar)) {
        initialiser = parse_let(true); // This consumes ';'
    } else {
        initialiser = makeNode<ExprStmt>(parse_expression());
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
    
    consume(TokenType::RightParen, "Expected ')' at the end of a for loop");

    StmtPtr body = parse_block();

    // add increment at end of body
    if (increment != nullptr) {
        body = makeNode<Block>(
            std::vector<StmtPtr>{
                body,
                makeNode<ExprStmt>(increment)
            }
        );
    }

    // default condition = true
    if (condition == nullptr) {
        condition = makeNode<Literal>(true);
    }

    // while loop
    body = makeNode<While>(move(condition), move(body));

    // wrap with initializer
    if (initialiser != nullptr) {
        body = makeNode<Block>(
            std::vector<StmtPtr>{
                initialiser,
                body
            }
        );
    }

    return body;
}

/*
class_stmt   → "class" IDENTIFIER '{' aggregate_member* '}' ';'
record_stmt → "record" IDENTIFIER '{' aggregate_member* '}' ';'
aggregate_member → functionDecl | let_stmt (TODO: change to fieldDecl. its actually not letstmt.)
*/
StmtPtr Parser::parse_aggregate(AggregateKind kind) {
    auto traceFrame = recorder.scope(__func__, current);
    //consume(TokenType::KeywordClass, "Expected class keyword");

    Token name = consume(TokenType::Identifier, "Expected an identifier");

    consume(TokenType::LeftBrace, "Expected '{'");

    std::vector<FieldMember> fieldMembers;
    std::vector<MethodMember> methodMembers;
    std::vector<EnumMember> enumMembers;
    std::vector<ConstructorMember> constructorMembers;

    // NOTICE: the expressions will be shared among all constructors.
    std::vector<StmtPtr> initStmts;

    while (!check(TokenType::RightBrace)) {
        if (match(TokenType::KeywordInit)) {
            // Constructors
            FunctionExprPtr constructorFunc = std::static_pointer_cast<FunctionExpr>(parse_functionExpr());
            constructorMembers.push_back(
                ConstructorMember(constructorFunc)
            );

        } else if (match(TokenType::KeywordLet)) {
            std::shared_ptr<Let> letStmt = std::static_pointer_cast<Let>(parse_let(false));
            /*
            Token name = consume(TokenType::Identifier, "Expected an identifier");
            
            TypeNodePtr typeAnnotation = nullptr;
            if (match(TokenType::Colon)) {
                typeAnnotation = parse_type();
            }
            
            ExprPtr initExpr = nullptr;
            if (match(TokenType::Assign)) {
                initExpr = parse_expression();
            }

            consumeSemicolon();
            
            if (initExpr) {
                initStmts.push_back(makeNode<ExprStmt>(
                    makeNode<Assignment>(
                        AssignmentOp::Assign,
                        makeNode<Get>(
                            makeNode<ThisExpr>(),
                            name.lexeme
                        ),
                        std::move(initExpr)
                    )
                ));
            }
            */

            if (letStmt->expr) {
                auto initialisation = makeNode<Assignment>(
                    AssignmentOp::Assign,
                    makeNode<Get>(
                        makeNode<ThisExpr>(),
                        letStmt->name
                    ),
                    std::move(letStmt->expr)
                );
                initialisation->isInitialisation = true;
                initStmts.push_back(makeNode<ExprStmt>(std::move(initialisation)));
            }

            fieldMembers.push_back(
                FieldMember(
                    move(letStmt->annotatedType),
                    letStmt->name,
                    move(letStmt->expr),
                    letStmt->isMutable
                )
            );
        } else if (match(TokenType::KeywordVar)) {
            std::shared_ptr<Let> letStmt = std::static_pointer_cast<Let>(parse_let(true));
            /*
            Token name = consume(TokenType::Identifier, "Expected an identifier");
            
            TypeNodePtr typeAnnotation = nullptr;
            if (match(TokenType::Colon)) {
                typeAnnotation = parse_type();
            }
            
            ExprPtr initExpr = nullptr;
            if (match(TokenType::Assign)) {
                initExpr = parse_expression();
            }

            consumeSemicolon();
            
            if (initExpr) {
                initStmts.push_back(makeNode<ExprStmt>(
                    makeNode<Assignment>(
                        AssignmentOp::Assign,
                        makeNode<Get>(
                            makeNode<ThisExpr>(),
                            name.lexeme
                        ),
                        std::move(initExpr)
                    )
                ));
            }
            */

            if (letStmt->expr) {
                auto initialisation = makeNode<Assignment>(
                    AssignmentOp::Assign,
                    makeNode<Get>(
                        makeNode<ThisExpr>(),
                        letStmt->name
                    ),
                    std::move(letStmt->expr)
                );
                initialisation->isInitialisation = true;
                initStmts.push_back(makeNode<ExprStmt>(std::move(initialisation)));
            }

            fieldMembers.push_back(
                FieldMember(
                    move(letStmt->annotatedType),
                    letStmt->name,
                    move(letStmt->expr),
                    // TODO: Unify parsing [modifiers] [name] ':' [type] '=' [expr] format.
                    letStmt->isMutable
                )
            );
        } else if (match(TokenType::KeywordFun)) {
            // Desugared form: let [functionName] = [functionExpr]
            std::shared_ptr<Let> letStmt = std::static_pointer_cast<Let>(parse_functionDecl());
            methodMembers.push_back(
                MethodMember(
                    move(letStmt->name),
                    // This must be FunctionExpr.
                    std::static_pointer_cast<FunctionExpr>(move(letStmt->expr))
                )
            );
        } else if (match(TokenType::KeywordEnum)) {
            auto enumMember = std::static_pointer_cast<Enum>(parse_enum());
            enumMembers.push_back(
                EnumMember( std::move(enumMember) )
            );
        } else {
            throw KMYParseError(
                "Only field declarations (let/var), method declarations (fun), and enum declarations (enum) are allowed in aggregates... FOR NOW!!!!",
                peek().line, // This isnt previous().
                peek().startIdx,
                peek().endIdx
            );
        }
    }

    consume(TokenType::RightBrace, "Expected '}' after aggregate body");
    consumeSemicolon();

    auto fieldInitFunc = makeNode<FunctionExpr>(
        std::vector<Parameter> {},
        makeNode<Block>(std::move(initStmts)),
        nullptr
    );
    
    return makeNode<Aggregate>(
        kind,
        move(name.lexeme),
        move(fieldMembers),
        move(methodMembers),
        move(constructorMembers),
        move(enumMembers),
        move(fieldInitFunc)
    );
}

/*
typealias_stmt → "typealias" IDENTIFIER '=' type ';'
*/
StmtPtr Parser::parse_typeAlias() {
    auto traceFrame = recorder.scope(__func__, current);
    Token name = consume(TokenType::Identifier, "Expected an identifier");

    consume(TokenType::Assign, "Expected '='");

    TypeNodePtr type = parse_type();

    consumeSemicolon();

    return makeNode<TypeAlias>(
        name.lexeme,
        std::move(type)
    );
}


/*
enum_stmt → "enum" IDENTIFIER '{' enum_list? '}' ';'
enum_list → IDENTIFIER (',' IDENTIFIER)*
*/
StmtPtr Parser::parse_enum() {
    auto traceFrame = recorder.scope(__func__, current);
    Token name = consume(TokenType::Identifier, "Expected an identifier for the enum's name");
    
    consume(TokenType::LeftBrace, "Expected '{' after enum keyword");

    std::vector<std::string> variants;
    if (!check(TokenType::RightBrace)) {
        do {
            Token var = consume(TokenType::Identifier, "Expected an identifier for the enum's variant");
            variants.push_back(var.lexeme);
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightBrace, "Expected '}' after enum definition");

    consumeSemicolon();

    return makeNode<Enum>(name.lexeme, std::move(variants));
}

// =============================
// Helpers
// =============================

// TODO: INCOMPLETE!!!! &= fails. (Really?)
int Parser::get_binding_power(TokenType type) {
    switch (type) {
        case TokenType::Assign:
        case TokenType::PlusAssign:
        case TokenType::MinusAssign:
        case TokenType::StarAssign:
        case TokenType::SlashAssign:
        case TokenType::BitAndAssign:
        case TokenType::BitOrAssign:
        case TokenType::BitXorAssign:
        case TokenType::LShiftAssign:
        case TokenType::RShiftAssign:
        case TokenType::LogicalAndAssign:
        case TokenType::LogicalOrAssign:
            return BP_ASSIGNMENT;

        case TokenType::LogicalOr: return BP_LOGICAL_OR;
        case TokenType::NullCoalesce: return BP_NULL_COALESCE;
        case TokenType::LogicalAnd: return BP_LOGICAL_AND;

        case TokenType::BitOr: return BP_BITWISE_OR;
        case TokenType::BitXor: return BP_BITWISE_XOR;
        case TokenType::BitAnd: return BP_BITWISE_AND;

        case TokenType::EqualEqual:
        case TokenType::NotEqual:
            return BP_EQUALITY;

        case TokenType::Greater:
        case TokenType::GreaterEqual:
        case TokenType::Less:
        case TokenType::LessEqual:
            return BP_COMPARISON;

        case TokenType::LShift:
        case TokenType::RShift:
            return BP_SHIFT;

        case TokenType::Plus:
        case TokenType::Minus:
            return BP_TERM;

        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Percent:
            return BP_FACTOR;

        case TokenType::LeftParen:
        case TokenType::LeftBracket:
        case TokenType::Dot:
        case TokenType::ForceUnwrap:
        case TokenType::PlusPlus:
        case TokenType::MinusMinus:
            return BP_POSTFIX;

        default:
            return BP_NONE;
    }
}

// Right-associative operators
bool Parser::is_right_associative(TokenType type) {
    switch (type) {
        case TokenType::Assign:
        case TokenType::PlusAssign:
        case TokenType::MinusAssign:
        case TokenType::StarAssign:
        case TokenType::SlashAssign:
        case TokenType::BitAndAssign:
        case TokenType::BitOrAssign:
        case TokenType::BitXorAssign:
        case TokenType::LShiftAssign:
        case TokenType::RShiftAssign:
        case TokenType::LogicalAndAssign:
        case TokenType::LogicalOrAssign:
            return true;
        default:
            return false;
    }
}

// =============================
// Main Pratt Parse (Expressions)
// =============================

/*
Parse left
Let stronger operators steal right side
Return when weaker operator appears

Do it with 1 + 2 * 3 - 4.

result = [ recursion 1 / minBp = 10]
First, parse 1
    left = 1
See +
Check can I continue?
    Since bp(100) >= minBp(0), we can continue consuming.
Consume +
parse rest as right1 and build +(1, right1) = +(1, -(*(2, 3), 4))
We are at EOF. (Continue check omitted) return left.
Final tree:
    +
   / \
  1   -
     / \
    *   4
   / \
  2   3

right1 = [ recursion 2 / minBp = 100 ]
First, parse 2
    left = 2
See *
Check can I continue?
    Since bp(110) >= minBp(100), we can continue consuming.
Consume *
parse rest as right and left = *(2, right2) = *(2, 3)
See -
Check can I continue?
    Since bp(100) >= minBp(100), we can continue consuming.
Consume -
parse rest as right3 and left = -(*(2, 3), 4)
We are at EOF. (Continue check omitted) return left


right2 = [ recursion 3 / minBp = 110 ]
First, parse 3
    left = 3
See -
Check can I continue?
    Since bp(100) < minBp(110), we STOP.
We DONT consume - and return left = 3

right3 = [ recursion 4 / minBp = 100 ]
First, parse 4
    left = 4
Check can I continue?
    Since bp(0) < minBp(100), we STOP.
We are at EOF. Return left = 4

*/

/*
e.g. a = arr[i] = 5 * 3 + 2 * 1

minBp = 10
left = a
bp = 10(=)
nextBp = 10 + 1 (Since = is r-associative)
    [right]
    minBp = 11
    left = arr
    bp = 10(=)
    index = i (We skip this for convenience)
    left = index(arr, i)
    nextBp = 10 + 1
        [right]
        minBp = 11
        left = 5
        bp = 110(*)
        nextBp = 110
            [right]
            minBp = 110
            left = 3
            bp = 100(+)
            return left
        right = 3
        left = *(5, 3)
        bp = 100(+)
        nextBp = 100
            [right]
            minBp = 100
            left = 2
            bp = 110(*)
            nextBp = 110
                [right]
                minBp = 110
                left = 1
                bp = 0
                return left
            right = 1
            left = *(2, 1)
            bp = 0
            return left
        right = *(2, 1)
        left = +(*(5, 3), *(2, 1))
        bp = 0
        return left
    right = +(*(5, 3), *(2, 1))
    left = =(index(arr, i), +(*(5, 3), *(2, 1)))
    bp = 0
    return left
right = =(index(arr, i), +(*(5, 3), *(2, 1)))
left = =(a, =(index(arr, i), +(*(5, 3), *(2, 1))))
bp = 0
return left
*/
ExprPtr Parser::parse_expression(int minBP) {
    auto traceFrame = recorder.scope(__func__, current, minBP);
    ExprPtr left = parse_prefix();

    // Accept operators at least as strong as minBP. A stronger operator is
    // consumed by the recursive RHS; a weaker one breaks for the caller.
    // Equal precedence stays here for left-associative ops, but belongs to
    // the RHS for right-associative assignments.
    while (true) {
        TokenType op = peek().type;
        int bp = get_binding_power(op);

        if (bp < minBP) {
            recorder.emit("returnExpression", current, minBP, bp);
            break;
        }

        recorder.emit("acceptOperator", current, minBP, bp);

        advance();

        // This part is postfix parsing, which parses stuff right after the left expression,
        // which are basically postfix operators ((), [], ., !!, ++, --)
        if (op == TokenType::LeftParen) {
            left = finishCall(std::move(left));
            continue;
        }

        // Index
        if (op == TokenType::LeftBracket) {
            ExprPtr index = parse_expression();
            consume(TokenType::RightBracket, "Expected ']'");
            left = makeNode<Index>(std::move(left), std::move(index));
            continue;
        }

        // Get
        if (op == TokenType::Dot) {
            Token name = consume(TokenType::Identifier, "Expected property name");
            left = makeNode<Get>(std::move(left), name.lexeme);
            continue;
        }

        if (op == TokenType::ForceUnwrap) {
            left = makeNode<UnaryExpr>(UnaryOp::ForceUnwrap, std::move(left));
            continue;
        }

        if (op == TokenType::PlusPlus || op == TokenType::MinusMinus) {
            left = makeNode<UnaryExpr>(
                op == TokenType::PlusPlus
                    ? UnaryOp::PostIncrement
                    : UnaryOp::PostDecrement,
                std::move(left)
            );
            continue;
        }

        // For left-associative operators, increasing minBP prevents
        // equal-precedence operators from binding on the right.
        int nextMinBP = bp + (is_right_associative(op) ? 0 : 1);

        // IMPORTANT OBSERVATION:
        // Parse the RHS until encountering an operator
        // with lower precedence (or equal precedence for left-associative ops).
        // e.g. 1 + 2 * 3 / 1 - 3
        //          ^^^^^^^^^right
        //      ^left
        // e.g. 1 + 2 * 3 / 1 - 3
        //              ^right
        //          ^left
        // e.g. 1 + 2 * 3 / 1 - 3
        //                  ^right
        //          ^^^^^left
        // e.g. x = y += z = 1 + 2 * 3
        //          ^^^^^^^^^^^^^^^^^^right (doesn't stop at += or = since its r-associative)         
        //      ^left
        // e.g. x = y += z = 1 + 2 * 3
        //               ^^^^^^^^^^^^^right
        //          ^left
        // e.g. x = y += z = 1 + 2 * 3
        //                   ^^^^^^^^^right
        //               ^left
        // e.g. x = y += z = 1 + 2 * 3
        //                       ^^^^^^right
        //                   ^left
        // e.g. x = y += z = 1 + 2 * 3
        //                           ^right
        //                       ^left

        ExprPtr right = parse_expression(nextMinBP);

        // Assignment operators
        if (bp == BP_ASSIGNMENT) {
            left = makeNode<Assignment>(
                toAssignmentOp(op),
                std::move(left),
                std::move(right)
            );
        }
        else {
            left = makeNode<BinaryExpr>(
                toBinaryOp(op),
                std::move(left),
                std::move(right)
            );
        }
    }

    return left;
}

// =============================
// Prefix(Primary) Parsing
// =============================

/*
array_elements → expression ("," expression)*
primary → 
    LITERAL |
    '(' expression ')'
    '[' array_elements? ']' |
    record |
    fnExpr |
    newExpr
*/
ExprPtr Parser::parse_prefix() {
    auto traceFrame = recorder.scope(__func__, current);
    Token tok = advance();

    switch (tok.type) {

        // Literals
        case TokenType::Int:
            return makeNode<Literal>(stoi(tok.lexeme));

        case TokenType::Double:
            return makeNode<Literal>(stod(tok.lexeme));

        case TokenType::StringLiteral:
            return makeNode<Literal>(tok.lexeme);

        case TokenType::KeywordTrue:
            return makeNode<Literal>(true);

        case TokenType::KeywordFalse:
            return makeNode<Literal>(false);

        case TokenType::KeywordNull:
            return makeNode<Literal>(nullptr);

        case TokenType::Identifier: {
            // Check for IDENTIFIER "::" IDENTIFIER
            if (match(TokenType::ColonColon)) {
                std::vector<std::string> parts;
                parts.push_back(tok.lexeme);
                do {
                    Token memberName = consume(TokenType::Identifier, "Expected an identifier after \"::\"");
                    parts.push_back(memberName.lexeme);
                } while(match(TokenType::ColonColon));
                
                return makeNode<ScopeAccessExpr>(std::move(parts));
            }

            return makeNode<Variable>(tok.lexeme);
        }

        // Grouping
        case TokenType::LeftParen: {
            ExprPtr expr = parse_expression();
            consume(TokenType::RightParen, "Expected ')'");
            return expr;
        }

        // Arrays
        case TokenType::LeftBracket: {
            std::vector<ExprPtr> elements;
            if (!check(TokenType::RightBracket)) {
                do {
                    elements.push_back(parse_expression());
                } while(match(TokenType::Comma));
            }
            
            consume(TokenType::RightBracket, "Expected ']' after array literal");

            return makeNode<ArrayLiteral>( move(elements) );
        }

        // Function Expressions ( e.g., fun() {} )
        case TokenType::KeywordFun:
            return parse_functionExpr();
        
        case TokenType::LeftBrace:
            return parse_recordExpr();

        case TokenType::KeywordThis:
            return makeNode<ThisExpr>();
        
        case TokenType::KeywordNew:
            return parse_newExpr();

        // Unary operators. '&' and '*' reuse BitAnd and Star tokens; their
        // prefix position distinguishes address-of/dereference from the
        // binary operators handled by parse_expression's Pratt loop.
        case TokenType::Plus:
        case TokenType::Minus:
        case TokenType::Bang:
        case TokenType::BitNot: // ~
        case TokenType::BitAnd: // &
        case TokenType::Star: {
            ExprPtr right = parse_expression(BP_UNARY);
            return makeNode<UnaryExpr>(toUnaryOp(tok.type), std::move(right));
        }
        case TokenType::PlusPlus: {
            // Prefix increment: ++x
            ExprPtr right = parse_expression(BP_UNARY);
            return makeNode<UnaryExpr>(UnaryOp::PreIncrement, std::move(right));
        }
        case TokenType::MinusMinus: {
            // Prefix decrement: --x
            ExprPtr right = parse_expression(BP_UNARY);
            return makeNode<UnaryExpr>(UnaryOp::PreDecrement, std::move(right));
        }
        default:
            throw KMYParseError("Unexpected token in expression", tok.line, tok.startIdx, tok.endIdx);
    }
}

/*
type        → ( basicType | functionType | recordType | scopedType ) ( '*' | arraySuffix )* '?'?
basicType   → "int" | "string" | "bool" | "any" | "void"

functionType → '(' typeList? ')' "->" type
typeList    → type (',' type)*
arraySuffix  → '[' ( integer | "..." )? ']'

recordType → '{'  typePairs?  '}'
typePairs → typePair (',' typePair)*
typePair → IDENTIFIER ':' type

scopedType → IDENTIFIER ("::" IDENTIFIER)* // This includes just named types.
*/

TypeNodePtr Parser::parseTypeToken(Token t) {
    auto traceFrame = recorder.scope(__func__, current);
    switch (t.type) {
        case TokenType::KeywordInt:
            return makeNode<NamedTypeNode>("int");

        case TokenType::KeywordDouble:
            return makeNode<NamedTypeNode>("double");

        case TokenType::KeywordBool:
            return makeNode<NamedTypeNode>("bool");

        case TokenType::KeywordString:
            return makeNode<NamedTypeNode>("string");

        case TokenType::KeywordAny:
            return makeNode<NamedTypeNode>("any");

        case TokenType::KeywordVoid:
            return makeNode<NamedTypeNode>("void");

        case TokenType::KeywordI64:
            return makeNode<NamedTypeNode>("i64");

        case TokenType::KeywordU64:
            return makeNode<NamedTypeNode>("u64");

        case TokenType::KeywordF64:
            return makeNode<NamedTypeNode>("f64");

        case TokenType::KeywordByte:
            return makeNode<NamedTypeNode>("byte");
        
        default: 
            // TODO: previous() here gets the invalid token, not ':'
            throw KMYParseError("Expected identifier or type keyword after ':'", previous().line, previous().startIdx, previous().endIdx);
    }
}

TypeNodePtr Parser::parse_recordType() {
    auto traceFrame = recorder.scope(__func__, current);
    consume(TokenType::LeftBrace, "Expected '{'");

    std::vector<std::pair<std::string, TypeNodePtr>> paramTypePairs; 

    if (!check(TokenType::RightBrace)) {
        do {
            Token name = consume(TokenType::Identifier, "Expected an identifier");
            
            consume(TokenType::Colon, "Expected ':'");

            TypeNodePtr type = parse_type();

            paramTypePairs.push_back( { name.lexeme, type } );
        } while(match(TokenType::Comma));
    }

    consume(TokenType::RightBrace, "Expected '}'");

    return makeNode<RecordTypeNode>(
        std::move(paramTypePairs)
    );
}

TypeNodePtr Parser::parse_functionType() {
    auto traceFrame = recorder.scope(__func__, current);
    consume(TokenType::LeftParen, "Expected '('");
    std::vector<TypeNodePtr> paramTypes; 
    if (!check(TokenType::RightParen)) {
        do {
            paramTypes.push_back( parse_type() );
        } while(match(TokenType::Comma));
    }
    consume(TokenType::RightParen, "Expected ')'");
    consume(TokenType::Arrow, "Expected \"->\"");
    
    TypeNodePtr returnType = parse_type();
    return makeNode<FunctionTypeNode>(
        std::move(paramTypes),
        std::move(returnType)
    );
}

// TODO: Need to type intern this one too
TypeNodePtr Parser::parse_scopedType() {
    auto traceFrame = recorder.scope(__func__, current);
    // Check for IDENTIFIER "::" IDENTIFIER
    Token first = consume(TokenType::Identifier, "Expected an identifier... but if this error is shown, smths wrong");
    std::vector<std::string> parts = {first.lexeme};
    if (match(TokenType::ColonColon)) {
        do {
            Token memberName = consume(TokenType::Identifier, "Expected an identifier after \"::\"");
            parts.push_back(memberName.lexeme);
        } while(match(TokenType::ColonColon));
    }

    return makeNode<ScopedTypeNode>(std::move(parts));
}

// type -> "shared"? baseType ( '*' | arraySuffix )* '?'?
// Each '*' wraps the type parsed so far: `int**` is pointer-to-pointer,
// `int*[]` is an array of pointers, and `int[]*` is pointer-to-array.
// A trailing '?' wraps the complete type, e.g. `int*?` or `int[]?`.
// `shared` wraps the completed base/suffix type in a SharedTypeNode.
TypeNodePtr Parser::parse_type(bool parseArraySuffix) {
    auto traceFrame = recorder.scope(__func__, current);
    const bool hasSharedQualifier = match(TokenType::KeywordShared);
    TypeNodePtr result = nullptr;
    if (check(TokenType::LeftParen)) {
        result = parse_functionType();
    } else if (check(TokenType::LeftBrace)) {
        result = parse_recordType();
    } else if (check(TokenType::Identifier)) {
        result = parse_scopedType();
    } else {
        Token typeToken = peek();
        result = parseTypeToken(typeToken);
        advance(); // It is like this due to proper usage of previous() in parseTypeToken() error
    }

    // parseArraySuffix is false for `new T[expression]`. Pointer suffixes
    // still belong to the element type, so `new int*[n]` remains possible.
    while (true) {
        if (match(TokenType::Star)) {
            result = makeNode<PointerTypeNode>(std::move(result));
            continue;
        }
        if (!parseArraySuffix || !match(TokenType::LeftBracket)) break;
        if (match(TokenType::RightBracket)) {
            // Unsized array type, e.g., int[] (used with runtime-sized new).
            result = makeNode<ArrayTypeNode>(result, -1, false, false);
        } else if (false) {
            // NOTE: UNUSED BRANCH!
            // TODO: Dynamic array will have diff expr...
            // Dynamic array, e.g., int[...]
            consume(TokenType::RightBracket, "Expected ']' after '...'");
            result = makeNode<ArrayTypeNode>(result, 0, true, true);
        } else if (check(TokenType::Int)) {
            // Bounded array, e.g., int[5]
            Token sizeToken = consume(TokenType::Int, "Expected integer for array size");
            consume(TokenType::RightBracket, "Expected ']' after array size");
            result = makeNode<ArrayTypeNode>(result, std::stoi(sizeToken.lexeme), true, false);
        } else {
            throw KMYParseError(
                "Invalid array type syntax. Expected ']' or an integer",
                previous().line,
                previous().startIdx, 
                previous().endIdx
            );
        }
    }

    if (hasSharedQualifier) {
        result = makeNode<SharedTypeNode>(std::move(result));
    }
    if (parseArraySuffix && match(TokenType::Nullable)) {
        result = makeNode<NullableTypeNode>(std::move(result));
    }
    return result;
}

/*
fnExpr → "fun" '(' params? ')' block
params → param (',' param)*
param  → "var"? ( ( IDENTIFIER (':' type)? ('=' expression)? ) | "..." IDENTIFIER (':' type)? )

fnExpr is more like a lambda function!
*/
ExprPtr Parser::parse_functionExpr() {
    auto traceFrame = recorder.scope(__func__, current);
    consume(TokenType::LeftParen, "Expected '(' after function declaration");

    std::vector<Parameter> params;
    bool defaultSeen = false;
    if (!check(TokenType::RightParen)) {
        do {
            // Parameters are immutable bindings unless explicitly introduced
            // with `var`, matching ordinary let/var declarations.
            bool isMutable = match(TokenType::KeywordVar);
            if (match(TokenType::Ellipsis)) {

                Token name = consume(TokenType::Identifier, "Expected an identifier");

                TypeNodePtr varargType = match(TokenType::Colon) ? parse_type() : nullptr;

                if (!check(TokenType::RightParen)) {
                    throw KMYParseError(
                        "Variadic parameter should come at the end of the parameter list. Didn't see ')'",
                        previous().line,
                        previous().startIdx,
                        previous().endIdx
                    );
                }

                params.push_back(
                    Parameter(std::move(varargType), name.lexeme, true, isMutable, false, nullptr) 
                );
                break;
            }

            Token name = consume(TokenType::Identifier, "Expected an identifier");
            
            TypeNodePtr paramType = match(TokenType::Colon) ? parse_type() : nullptr;
            
            ExprPtr defaultValue = nullptr;
            bool isDefault;
            if (isDefault = match(TokenType::Assign)) {
                defaultSeen = true;
                defaultValue = parse_expression();
            } else if (defaultSeen) {
                throw KMYParseError(
                    "Non-default parameter detected after a default parameter",
                    previous().line,
                    previous().startIdx,
                    previous().endIdx
                );
            }
            
            params.push_back(
                Parameter(std::move(paramType), name.lexeme, false, isMutable, isDefault, defaultValue) 
            );
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "Expected ')' after function parameters");
    
    TypeNodePtr returnType = match(TokenType::Colon) ? parse_type() : nullptr;

    StmtPtr body = parse_block();

    return makeNode<FunctionExpr>(
        std::move(params),
        std::move(body),
        std::move(returnType)
    );
}


/*
record → '{' (pair (',' pair)*)? '}'
pair   → IDENTIFIER ':' expression
*/
ExprPtr Parser::parse_recordExpr() {
    auto traceFrame = recorder.scope(__func__, current);
    std::vector<std::pair<std::string, ExprPtr>> fields;

    if (check(TokenType::RightBrace)) {
        advance();
        return makeNode<RecordLiteral>(
            std::move(fields)
        );
    }

    do {
        Token name = consume(TokenType::Identifier, "Expected an identifier");
        
        consume(TokenType::Colon, "Expected ':'");
        
        ExprPtr expr = parse_expression();
        fields.push_back({name.lexeme, expr});

    } while (match(TokenType::Comma));
    
    consume(TokenType::RightBrace, "Expected '}' after record declaration");

    return makeNode<RecordLiteral>(
        std::move(fields)
    ); 
}  


/*
newExpr → "new" type '(' args? ')' | "new" type '[' expression ']'
args    → expression ("," expression)*
*/
ExprPtr Parser::parse_newExpr() {
    auto traceFrame = recorder.scope(__func__, current);
    // Parse the base type first so a runtime expression can appear in []
    // (for example, `new int[count + 1]`).
    TypeNodePtr allocatedType = parse_type(false);

    if (match(TokenType::LeftBracket)) {
        ExprPtr sizeExpr = parse_expression();
        consume(TokenType::RightBracket, "Expected ']' after array size expression");

        // Preserve the fixed-array representation for a bare integer literal.
        auto literalSize = std::dynamic_pointer_cast<Literal>(sizeExpr);
        if (literalSize && std::holds_alternative<int>(literalSize->value)) {
            allocatedType = makeNode<ArrayTypeNode>(
                std::move(allocatedType), std::get<int>(literalSize->value), true, false);
            return makeNode<NewExpr>(std::move(allocatedType));
        }

        allocatedType = makeNode<ArrayTypeNode>(
            std::move(allocatedType), -1, false, true);
        return makeNode<NewExpr>(std::move(allocatedType), std::move(sizeExpr));
    }

    if (allocatedType->kind == TypeNodeKind::ARRAY && !check(TokenType::LeftParen)) {
        return makeNode<NewExpr>(std::move(allocatedType));
    }

    // The ownership wrapper does not change which aggregate is constructed.
    TypeNodePtr aggregateType = allocatedType;
    if (aggregateType->kind == TypeNodeKind::SHARED) {
        aggregateType = std::static_pointer_cast<SharedTypeNode>(aggregateType)->innerType;
    }

    if (aggregateType->kind != TypeNodeKind::NAMED &&
        aggregateType->kind != TypeNodeKind::SCOPED) {
        throw KMYParseError(
            "Expected an aggregate type followed by constructor arguments or a fixed array type",
            previous().line,
            previous().startIdx,
            previous().endIdx
        );
    }

    std::string aggregateName;
    if (aggregateType->kind == TypeNodeKind::NAMED) {
        aggregateName = std::static_pointer_cast<NamedTypeNode>(aggregateType)->name;
    } else {
        auto scopedType = std::static_pointer_cast<ScopedTypeNode>(aggregateType);
        aggregateName = scopedType->scopeParts.front();
        for (size_t i = 1; i < scopedType->scopeParts.size(); ++i) {
            aggregateName += "::" + scopedType->scopeParts[i];
        }
    }
    consume(TokenType::LeftParen, "Expected '(' after class/record name");
    std::vector<ExprPtr> args;

    if (!check(TokenType::RightParen)) {
        do {
            args.push_back(parse_expression());
        } while (match(TokenType::Comma));
    }
    
    consume(TokenType::RightParen, "Expected ')' after class constructor args");

    return makeNode<NewExpr>(
        std::move(aggregateName),
        std::move(args),
        std::move(allocatedType)
    );
}


ExprPtr Parser::finishCall(ExprPtr callee) {
    auto traceFrame = recorder.scope(__func__, current);
    std::vector<ExprPtr> args;

    if (!check(TokenType::RightParen)) {
        do {
            args.push_back(parse_expression());
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "Expected ')'");

    return makeNode<Call>(
        std::move(callee),
        std::move(args)
    );
}
