#include "utils.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <vector>
#include "../Core/errorhandler.hpp"
#include "../Core/tokens.hpp"
#include "../Core/AstBaseForward.hpp"
#include "../Core/Ast.hpp"

using std::cout, std::endl;

static bool astTraceEnabled = false;
static const char* astTracePass = nullptr;

void setAstTraceEnabled(bool enabled) { astTraceEnabled = enabled; }
void setAstTracePass(const char* pass) { astTracePass = pass; }

void traceAstVisit(const char* node) {
    if (astTraceEnabled && astTracePass) {
        std::cerr << "[" << astTracePass << "] visit " << node << '\n';
    }
}

constexpr const char* RESET  = "\033[0m";

void printLog(LogLevel level, const std::string& msg, bool withPrefix) {
    // No new line added.
    const char* color;
    const char* prefix;

    switch (level) {
        case LogLevel::DEBUG: // Logs. Infos. Mostly "doing (smth)" GRAY
            color = "\033[90m";
            prefix = "[DEBUG]";
            break;

        case LogLevel::INFO: // Important step (Each compiler pass enter or exit). CYAN
            color = "\033[36m";
            prefix = "[INFO ]";
            break;

        case LogLevel::WARN: // No use rn. YELLOW
            color = "\033[33m";
            prefix = "[WARN ]";
            break;

        case LogLevel::ERROR: // No use. currently output is red.
            color = "\033[31m";
            prefix = "[ERROR]";
            break;
    }

    std::cout << color << prefix << " "
              << msg << RESET;
}

static const std::unordered_map<TokenType, std::string> tokenTypeNames = {
    // Literals
    {TokenType::Int, "Int"},
    {TokenType::Double, "Double"},
    {TokenType::Identifier, "Identifier"},
    {TokenType::StringLiteral, "StringLiteral"},

    // Arithemetic Operators
    {TokenType::Plus, "Plus"},
    {TokenType::Minus, "Minus"},
    {TokenType::Star, "Star"},
    {TokenType::Slash, "Slash"},
    {TokenType::Percent, "Percent"},

    // Bitwise Operators
    {TokenType::BitAnd, "And"},
    {TokenType::BitOr, "Or"},
    {TokenType::BitNot, "Not"},
    {TokenType::BitXor, "Xor"},
    {TokenType::LShift, "LShift"},
    {TokenType::RShift, "RShift"},

    // Logical Operators
    {TokenType::LogicalAnd, "AndAnd"},
    {TokenType::LogicalOr, "OrOr"},
    {TokenType::NullCoalesce, "NullCoalesce"},
    {TokenType::Nullable, "Nullable"},

    // Assignment Operators
    {TokenType::Assign, "Assign"},
    {TokenType::PlusAssign, "PlusAssign"},
    {TokenType::MinusAssign, "MinusAssign"},
    {TokenType::StarAssign, "StarAssign"},
    {TokenType::SlashAssign, "SlashAssign"},
    {TokenType::PercentAssign, "PercentAssign"},
    {TokenType::BitAndAssign, "AndAssign"},
    {TokenType::BitOrAssign, "OrAssign"},
    {TokenType::BitXorAssign, "XorAssign"},
    {TokenType::LShiftAssign, "LShiftAssign"},
    {TokenType::RShiftAssign, "RShiftAssign"},
    {TokenType::LogicalAndAssign, "AndAndAssign"},
    {TokenType::LogicalOrAssign, "OrOrAssign"},

    // Relational Operators
    {TokenType::Greater, "Greater"},
    {TokenType::Less, "Less"},
    {TokenType::GreaterEqual, "GreaterEqual"},
    {TokenType::LessEqual, "LessEqual"},
    {TokenType::EqualEqual, "EqualEqual"},
    {TokenType::NotEqual, "NotEqual"},

    // Keywords
    {TokenType::KeywordPrint, "KeywordPrint"},
    {TokenType::KeywordIf, "KeywordIf"},
    {TokenType::KeywordElse, "KeywordElse"},
    {TokenType::KeywordWhile, "KeywordWhile"},
    {TokenType::KeywordFor, "KeywordFor"},
    {TokenType::KeywordContinue, "KeywordContinue"},
    {TokenType::KeywordBreak, "KeywordBreak"},
    {TokenType::KeywordTrue, "KeywordTrue"},
    {TokenType::KeywordFalse, "KeywordFalse"},
    {TokenType::KeywordNull, "KeywordNull"},
    {TokenType::KeywordLet, "KeywordLet"},
    {TokenType::KeywordVar, "KeywordVar"},
    {TokenType::KeywordConst, "KeywordConst"},
    {TokenType::KeywordFun, "KeywordFun"},
    {TokenType::KeywordReturn, "KeywordReturn"},
    {TokenType::KeywordThis, "KeywordThis"},
    {TokenType::KeywordClass, "KeywordClass"},
    {TokenType::KeywordNew, "KeywordNew"},
    {TokenType::KeywordShared, "KeywordShared"},
    {TokenType::KeywordTypealias, "KeywordTypealias"},
    {TokenType::KeywordRecord, "KeywordRecord"},
    {TokenType::KeywordInit, "KeywordInit"},
    {TokenType::KeywordEnum, "KeywordEnum"},
    {TokenType::KeywordImport, "KeywordImport"},
    {TokenType::KeywordAs, "KeywordAs"},
    {TokenType::KeywordFrom, "KeywordFrom"},

    // Type keywords
    {TokenType::KeywordInt, "KeywordInt"},
    {TokenType::KeywordDouble, "KeywordDouble"},
    {TokenType::KeywordBool, "KeywordBool"},
    {TokenType::KeywordString, "KeywordString"},
    {TokenType::KeywordVoid, "KeywordVoid"},
    {TokenType::KeywordAny, "KeywordAny"},
    {TokenType::KeywordI64, "KeywordI64"},
    {TokenType::KeywordU64, "KeywordU64"},
    {TokenType::KeywordF64, "KeywordF64"},
    {TokenType::KeywordByte, "KeywordByte"},

    // Symbols
    {TokenType::LeftParen, "LeftParen"},
    {TokenType::RightParen, "RightParen"},
    {TokenType::LeftBrace, "LeftBrace"},
    {TokenType::RightBrace, "RightBrace"},
    {TokenType::Semicolon, "Semicolon"},
    {TokenType::Bang, "Bang"},
    {TokenType::ForceUnwrap, "ForceUnwrap"},
    {TokenType::LeftBracket, "LeftBracket"},
    {TokenType::RightBracket, "RightBracket"},
    {TokenType::Comma, "Comma"},
    {TokenType::Dot, "Dot"},
    {TokenType::Colon, "Colon"},
    {TokenType::Ellipsis, "Ellipsis"},
    {TokenType::Arrow, "Arrow"},
    {TokenType::ColonColon, "ColonColon"},
    
    // Special
    {TokenType::EndOfFile, "EndOfFile"},
    {TokenType::Unknown, "Unknown"}
};

// Symbol / literal representation
static const std::unordered_map<TokenType, std::string> tokenTypeSymbols = {
    // Arithmetic Operators
    {TokenType::Plus, "+"},
    {TokenType::Minus, "-"},
    {TokenType::Star, "*"},
    {TokenType::Slash, "/"},
    {TokenType::Percent, "%"},

    // Bitwise Operators
    {TokenType::BitAnd, "&"},
    {TokenType::BitOr, "|"},
    {TokenType::BitNot, "~"},
    {TokenType::LShift, "<<"},
    {TokenType::RShift, ">>"},

    // Logical Operators
    {TokenType::LogicalAnd, "&&"},
    {TokenType::LogicalOr, "||"},

    // Assignment Operators
    {TokenType::Assign, "="},
    {TokenType::PlusAssign, "+="},
    {TokenType::MinusAssign, "-="},
    {TokenType::StarAssign, "*="},
    {TokenType::SlashAssign, "/="},
    {TokenType::PercentAssign, "%="},
    {TokenType::BitAndAssign, "&="},
    {TokenType::BitOrAssign, "|="},
    {TokenType::BitXorAssign, "^="},
    {TokenType::LShiftAssign, "<<="},
    {TokenType::RShiftAssign, ">>="},
    {TokenType::LogicalAndAssign, "&&="},
    {TokenType::LogicalOrAssign, "||="},

    // Relational Operators
    {TokenType::Less, "<"},
    {TokenType::LessEqual, "<="},
    {TokenType::Greater, ">"},
    {TokenType::GreaterEqual, ">="},
    {TokenType::EqualEqual, "=="},
    {TokenType::NotEqual, "!="},

    // Symbols
    {TokenType::LeftParen, "("},
    {TokenType::RightParen, ")"},
    {TokenType::LeftBrace, "{"},
    {TokenType::RightBrace, "}"},
    {TokenType::Semicolon, ";"},
    {TokenType::Bang, "!"},
    {TokenType::LeftBracket, "["},
    {TokenType::RightBracket, "]"},
    {TokenType::Comma, ","},
    {TokenType::Dot, "."},
    {TokenType::Colon, ":"},
    {TokenType::Ellipsis, "..."},
    {TokenType::Arrow, "->"},
    {TokenType::ColonColon, "::"}
};

void printTokens(const std::vector<Token>& tokens) {
    auto escape = [](const std::string& value) {
        std::string escaped;
        for (char c : value) {
            switch (c) {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += c; break;
            }
        }
        return escaped;
    };

    cout << "  #    line  span       token                 lexeme\n";
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        auto name = tokenTypeNames.find(token.type);
        // Debug output must never reject a token that the lexer produced.
        const std::string tokenName = name == tokenTypeNames.end()
            ? "Token(" + std::to_string(static_cast<int>(token.type)) + ")"
            : name->second;
        cout << "  " << std::left << std::setw(5) << i
             << std::setw(6) << token.line
             << std::setw(11) << (std::to_string(token.startIdx) + ".." + std::to_string(token.endIdx))
             << std::setw(22) << tokenName
             << '"' << escape(token.lexeme) << "\"\n";
    }
}

// Forward declaration.
static void printStmt(const StmtPtr stmt, int depth);

static void printExpr(const ExprPtr expr, int depth = 0) {
    auto indent = std::string(depth * 2, ' ');
    auto* raw = expr.get();

    if (auto* l = dynamic_cast<const Literal*>(raw)) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                std::cout << indent << "Literal(null)\n";
            } else if constexpr (std::is_same_v<T, bool>) {
                std::cout << indent << "Literal(" << (arg ? "true" : "false") << ")\n";
            } else {
                std::cout << indent << "Literal(" << arg << ")\n";
            }
        }, l->value);

    } else if (auto* a = dynamic_cast<const ArrayLiteral*>(raw)) {
        cout << indent << "ArrayLiteral\n";
        for(auto elem : a->elements) {
            printExpr(elem, depth + 1);
        }

    } else if (auto* o = dynamic_cast<const RecordLiteral*>(raw)) {
        cout << indent << "RecordLiteral\n";
        for(const auto& [key, value] : o->fields) {
            cout << indent << "  " << key << "\n";
            printExpr(value, depth + 2);
        }

    } else if (auto* v = dynamic_cast<const Variable*>(raw)) {
        cout << indent << "Variable(" << v->name << ")\n";

    } else if (auto* u = dynamic_cast<const UnaryExpr*>(raw)) {
        cout << indent << "UnaryOp(" << "" << ")\n";
        printExpr(u->operand, depth + 1);

    } else if (auto* b = dynamic_cast<const BinaryExpr*>(raw)) {
        cout << indent << "BinaryOp(" << "" << ")\n";
        printExpr(b->left, depth + 1);
        printExpr(b->right, depth + 1);

    } else if (auto* a = dynamic_cast<const Assignment*>(raw)) {
        cout << indent << "Assignment(" 
        << ((a->op == AssignmentOp::Assign)
            ? ""
            : "" //tokenTypeSymbols.at(a->op))
        ) << ")\n";
        printExpr(a->left, depth + 1);
        printExpr(a->right, depth + 1);

    // =========================
    // Indexing (arr[i])
    // =========================
    } else if (auto* i = dynamic_cast<const Index*>(raw)) {
        cout << indent << "Index: (\n";
        cout << indent << "  Obj:\n";
        cout << indent << "  (\n";
        printExpr(i->obj, depth + 2);
        cout << indent << "  )\n";

        cout << indent << "  IndexExpr: (\n";        
        printExpr(i->index, depth + 2);
        cout << indent << ")\n";

    // =========================
    // Function call (f(a,b))
    // =========================
    } else if (auto* c = dynamic_cast<const Call*>(raw)) {
        cout << indent << "Call(\n";
        printExpr(c->func, depth + 1);

        cout << indent << "  Args:\n";
        cout << indent << "    (\n";
        for (const auto& arg : c->args) {
            printExpr(arg, depth + 3);
        }
        cout << indent << ")\n";

    // =========================
    // Function call (f(a,b))
    // =========================
    } else if (auto* g = dynamic_cast<const Get*>(raw)) {
        cout << indent << "Get(\n";
        cout << indent << "  Object: (\n";
        printExpr(g->obj, depth + 1);
        cout << indent << ")\n";

        cout << indent << "  Name: " << g->name << endl;
        cout << indent << ")\n";
    } else if (auto* g = dynamic_cast<const ScopeAccessExpr*>(raw)) {
        cout << indent << "ScopeAccess(\n";
        for (const auto& part : g->parts) {
            cout << indent << "  " << part << endl;
        }
        cout << indent << ")\n";

    // =========================
    // Function expression (lambda / fun)
    // =========================
    } else if (auto* f = dynamic_cast<const FunctionExpr*>(raw)) {
        cout << indent << "FunctionExpr:\n";

        cout << indent << "  Params:\n";
        for (const auto& p : f->params) {
            cout << indent << "    " << p.name << endl;
        }

        cout << indent << "  Body:\n";
        printStmt(f->body, depth + 2);
        
    } else if (auto* t = dynamic_cast<const ThisExpr*>(raw)) {
        cout << indent << "ThisExpr\n";
    } else if (auto* n = dynamic_cast<const NewExpr*>(raw)) {
        cout << indent << "NewExpr(\n";

        cout << indent << "  TypeName: " << n->typeName << endl;
        cout << indent << "  Args: (";
        for (const auto& p : n->args) {
            cout << indent << "  " << p << endl;
        }
        cout << indent << ")\n";
    }
}

static void printStmt(const StmtPtr stmt, int depth = 0) {
    auto indent = std::string(depth * 2, ' ');
    auto* raw = stmt.get();

    if (auto* print = dynamic_cast<Print*>(raw)) {
        cout << indent << "Print(\n";
        printExpr(print->expr, depth + 1);
        cout << indent << ")\n";

    } else if (auto* ifStmt = dynamic_cast<If*>(raw)) {
        cout << indent << "If(\n";
        cout << indent << "  Condition: (\n";        
        printExpr(ifStmt->condition, depth + 1);
        cout << indent << "  )\n";

        cout << indent << "  ThenBranch: (\n";        
        printStmt(ifStmt->thenbranch, depth + 1);
        cout << indent << "  )\n";
        
        if (ifStmt->elsebranch) {
            cout << indent << "Else(\n";
            printStmt(ifStmt->elsebranch, depth + 1);
            cout << indent << ")\n";
        }
        cout << indent << ")\n";

    } else if (auto* whileStmt = dynamic_cast<While*>(raw)) {
        cout << indent << "While(\n";
        cout << indent << "  Condition: (\n";        
        printExpr(whileStmt->condition, depth + 1);
        cout << indent << "  )\n";

        cout << indent << "  Body: (\n";        
        printStmt(whileStmt->body, depth + 1);
        cout << indent << "  )\n";
        
        cout << indent << ")\n";

    } else if (auto* block = dynamic_cast<Block*>(raw)) {
        cout << indent << "Block(\n";
        for (const auto& s : block->statements) {
            printStmt(s, depth + 1);
        }
        cout << indent << ")\n";

    } else if (auto* b = dynamic_cast<Break*>(raw)) {
        cout << indent << "Break()\n";
    } else if (auto* c = dynamic_cast<Continue*>(raw)) {
        cout << indent << "Continue()\n";
    } else if (auto* l = dynamic_cast<Let*>(raw)) {
        cout << indent << "Let(\n";

        cout << indent << "  Name: " << l->name << "\n";
        cout << indent << "  Mutable: " << (l->isMutable ? "true\n" : "false\n");
        if (l->expr) {
            cout << indent << "  Expr:\n";
            printExpr(l->expr, depth + 2);
        }
        
        cout << indent << ")\n";
    } else if (auto* r = dynamic_cast<Return*>(raw)) {
        cout << indent << "Return(\n";
        printExpr(r->expr, depth + 1);
        cout << indent << ")\n";
    } else if (auto* c = dynamic_cast<Aggregate*>(raw)) {
        cout << indent << "Aggregate(\n";

        cout << indent << "  Name: " << c->name << "\n";
        cout << indent << "  FieldMembers: (\n";
        for (const auto& fm : c->fieldMembers) {
            cout << indent << "  " << "(\n";
            cout << indent << "    " << "Name: " << fm.name << "\n";
            cout << indent << "    " << "Mutable: " << (fm.isMutable ? "true\n" : "false");
            cout << indent << "    " << "Expr: (\n";
            if (fm.initialiser) printExpr(fm.initialiser, depth + 3);
            cout << indent << "    " << ")\n";
            cout << indent << "  " << ")\n";
        }
        cout << indent << "  )\n";

        cout << indent << "  MethodMembers: (\n";
        for (const auto& mm : c->methodMembers) {
            cout << indent << "  " << "(";
            cout << indent << "    " << "Name: " << mm.name << "\n";
            cout << indent << "    " << "Expr: (\n";
            printExpr(mm.methodExpr, depth + 3);
            cout << indent << "    " << ")\n";
            cout << indent << "  " << ")";
        }
        cout << indent << "  )\n";
        
        cout << indent << ")\n";
    } else if (auto* e = dynamic_cast<Enum*>(raw)) {
        cout << indent << "Enum(\n";

        cout << indent << "  Name: " << e->name << "\n";
        cout << indent << "  Variants: (\n";
        for (const auto& v : e->variants) {
            cout << indent << "    " << v << "\n";
        }
        cout << indent << "  )\n";
        
        cout << indent << ")\n";
    } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(raw)) {
        cout << indent << "ExprStmt(\n";
        printExpr(exprStmt->expr, depth + 1);
        cout << indent << ")\n";
    }
}

void printAST(const ExprPtr expr) {
    printExpr(expr);
}

void printAST(const std::vector<StmtPtr>& stmts) {
    for (const auto& stmt : stmts) {
        printStmt(stmt);
    }
}
