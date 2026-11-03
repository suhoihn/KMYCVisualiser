#include "FrontendJson.hpp"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <type_traits>

#include "../Core/operators.hpp"

namespace {

std::string jsonEscape(const std::string& value) {
    std::ostringstream escaped;
    for (unsigned char c : value) {
        switch (c) {
            case '"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (c < 0x20) {
                    escaped << "\\u" << std::hex << std::setw(4)
                            << std::setfill('0') << static_cast<int>(c)
                            << std::dec;
                } else {
                    escaped << c;
                }
        }
    }
    return escaped.str();
}

void writeString(std::ostream& out, const std::string& value) {
    out << '\"' << jsonEscape(value) << '\"';
}

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::Int: return "Int";
        case TokenType::Double: return "Double";
        case TokenType::Identifier: return "Identifier";
        case TokenType::StringLiteral: return "StringLiteral";
        case TokenType::Plus: return "Plus";
        case TokenType::Minus: return "Minus";
        case TokenType::Star: return "Star";
        case TokenType::Slash: return "Slash";
        case TokenType::Percent: return "Percent";
        case TokenType::BitAnd: return "BitAnd";
        case TokenType::BitOr: return "BitOr";
        case TokenType::BitNot: return "BitNot";
        case TokenType::BitXor: return "BitXor";
        case TokenType::LShift: return "LShift";
        case TokenType::RShift: return "RShift";
        case TokenType::LogicalAnd: return "LogicalAnd";
        case TokenType::LogicalOr: return "LogicalOr";
        case TokenType::NullCoalesce: return "NullCoalesce";
        case TokenType::Assign: return "Assign";
        case TokenType::PlusAssign: return "PlusAssign";
        case TokenType::MinusAssign: return "MinusAssign";
        case TokenType::StarAssign: return "StarAssign";
        case TokenType::SlashAssign: return "SlashAssign";
        case TokenType::PercentAssign: return "PercentAssign";
        case TokenType::BitAndAssign: return "BitAndAssign";
        case TokenType::BitOrAssign: return "BitOrAssign";
        case TokenType::BitXorAssign: return "BitXorAssign";
        case TokenType::LShiftAssign: return "LShiftAssign";
        case TokenType::RShiftAssign: return "RShiftAssign";
        case TokenType::LogicalAndAssign: return "LogicalAndAssign";
        case TokenType::LogicalOrAssign: return "LogicalOrAssign";
        case TokenType::Greater: return "Greater";
        case TokenType::Less: return "Less";
        case TokenType::GreaterEqual: return "GreaterEqual";
        case TokenType::LessEqual: return "LessEqual";
        case TokenType::EqualEqual: return "EqualEqual";
        case TokenType::NotEqual: return "NotEqual";
        case TokenType::KeywordPrint: return "KeywordPrint";
        case TokenType::KeywordIf: return "KeywordIf";
        case TokenType::KeywordElse: return "KeywordElse";
        case TokenType::KeywordWhile: return "KeywordWhile";
        case TokenType::KeywordFor: return "KeywordFor";
        case TokenType::KeywordContinue: return "KeywordContinue";
        case TokenType::KeywordBreak: return "KeywordBreak";
        case TokenType::KeywordTrue: return "KeywordTrue";
        case TokenType::KeywordFalse: return "KeywordFalse";
        case TokenType::KeywordNull: return "KeywordNull";
        case TokenType::KeywordLet: return "KeywordLet";
        case TokenType::KeywordVar: return "KeywordVar";
        case TokenType::KeywordConst: return "KeywordConst";
        case TokenType::KeywordFun: return "KeywordFun";
        case TokenType::KeywordReturn: return "KeywordReturn";
        case TokenType::KeywordThis: return "KeywordThis";
        case TokenType::KeywordClass: return "KeywordClass";
        case TokenType::KeywordNew: return "KeywordNew";
        case TokenType::KeywordShared: return "KeywordShared";
        case TokenType::KeywordTypealias: return "KeywordTypealias";
        case TokenType::KeywordRecord: return "KeywordRecord";
        case TokenType::KeywordInit: return "KeywordInit";
        case TokenType::KeywordEnum: return "KeywordEnum";
        case TokenType::KeywordImport: return "KeywordImport";
        case TokenType::KeywordAs: return "KeywordAs";
        case TokenType::KeywordFrom: return "KeywordFrom";
        case TokenType::KeywordInt: return "KeywordInt";
        case TokenType::KeywordDouble: return "KeywordDouble";
        case TokenType::KeywordBool: return "KeywordBool";
        case TokenType::KeywordString: return "KeywordString";
        case TokenType::KeywordVoid: return "KeywordVoid";
        case TokenType::KeywordAny: return "KeywordAny";
        case TokenType::KeywordI64: return "KeywordI64";
        case TokenType::KeywordU64: return "KeywordU64";
        case TokenType::KeywordF64: return "KeywordF64";
        case TokenType::KeywordByte: return "KeywordByte";
        case TokenType::LeftParen: return "LeftParen";
        case TokenType::RightParen: return "RightParen";
        case TokenType::LeftBrace: return "LeftBrace";
        case TokenType::RightBrace: return "RightBrace";
        case TokenType::Semicolon: return "Semicolon";
        case TokenType::Bang: return "Bang";
        case TokenType::ForceUnwrap: return "ForceUnwrap";
        case TokenType::LeftBracket: return "LeftBracket";
        case TokenType::RightBracket: return "RightBracket";
        case TokenType::Comma: return "Comma";
        case TokenType::Dot: return "Dot";
        case TokenType::Colon: return "Colon";
        case TokenType::Ellipsis: return "Ellipsis";
        case TokenType::Arrow: return "Arrow";
        case TokenType::ColonColon: return "ColonColon";
        case TokenType::EndOfFile: return "EndOfFile";
        case TokenType::Unknown: return "Unknown";
    }
    return "Unknown";
}

const char* tokenCategory(TokenType type) {
    const std::string name = tokenTypeName(type);
    if (name.rfind("Keyword", 0) == 0) return "keyword";
    switch (type) {
        case TokenType::Int:
        case TokenType::Double:
        case TokenType::StringLiteral: return "literal";
        case TokenType::Identifier: return "identifier";
        case TokenType::LeftParen:
        case TokenType::RightParen:
        case TokenType::LeftBrace:
        case TokenType::RightBrace:
        case TokenType::LeftBracket:
        case TokenType::RightBracket:
        case TokenType::Semicolon:
        case TokenType::Comma:
        case TokenType::Dot:
        case TokenType::Colon:
        case TokenType::Ellipsis:
        case TokenType::Arrow:
        case TokenType::ColonColon: return "punctuation";
        case TokenType::EndOfFile: return "eof";
        case TokenType::Unknown: return "unknown";
        default: return "operator";
    }
}

struct Position { int line; int column; };

Position positionAt(const std::string& source, int requestedOffset) {
    const int offset = std::clamp(requestedOffset, 0, static_cast<int>(source.size()));
    Position result{1, 1};
    for (int i = 0; i < offset; ++i) {
        if (source[i] == '\n') {
            ++result.line;
            result.column = 1;
        } else {
            ++result.column;
        }
    }
    return result;
}

std::string binaryOpName(BinaryOp op) {
    switch (op) {
        case BinaryOp::Plus: return "+";
        case BinaryOp::Minus: return "-";
        case BinaryOp::Star: return "*";
        case BinaryOp::Slash: return "/";
        case BinaryOp::Percent: return "%";
        case BinaryOp::BitAnd: return "&";
        case BinaryOp::BitOr: return "|";
        case BinaryOp::BitXor: return "^";
        case BinaryOp::LShift: return "<<";
        case BinaryOp::RShift: return ">>";
        case BinaryOp::LogicalAnd: return "&&";
        case BinaryOp::LogicalOr: return "||";
        case BinaryOp::NullCoalesce: return "??";
        case BinaryOp::Greater: return ">";
        case BinaryOp::Less: return "<";
        case BinaryOp::GreaterEqual: return ">=";
        case BinaryOp::LessEqual: return "<=";
        case BinaryOp::EqualEqual: return "==";
        case BinaryOp::NotEqual: return "!=";
    }
    return "?";
}

std::string unaryOpName(UnaryOp op) {
    switch (op) {
        case UnaryOp::Plus: return "+";
        case UnaryOp::Minus: return "-";
        case UnaryOp::LogicalNot: return "!";
        case UnaryOp::BitNot: return "~";
        case UnaryOp::ForceUnwrap: return "!!";
    }
    return "?";
}

std::string assignmentOpName(AssignmentOp op) {
    switch (op) {
        case AssignmentOp::Assign: return "=";
        case AssignmentOp::PlusAssign: return "+=";
        case AssignmentOp::MinusAssign: return "-=";
        case AssignmentOp::StarAssign: return "*=";
        case AssignmentOp::SlashAssign: return "/=";
        case AssignmentOp::PercentAssign: return "%=";
        case AssignmentOp::BitAndAssign: return "&=";
        case AssignmentOp::BitOrAssign: return "|=";
        case AssignmentOp::BitNotAssign: return "~=";
        case AssignmentOp::BitXorAssign: return "^=";
        case AssignmentOp::LShiftAssign: return "<<=";
        case AssignmentOp::RShiftAssign: return ">>=";
        case AssignmentOp::LogicalAndAssign: return "&&=";
        case AssignmentOp::LogicalOrAssign: return "||=";
    }
    return "?=";
}

std::string typeName(const TypeNodePtr& type) {
    if (!type) return "inferred";
    std::string syntax = [&]() -> std::string {
        switch (type->kind) {
            case TypeNodeKind::NAMED:
                return std::static_pointer_cast<NamedTypeNode>(type)->name;
            case TypeNodeKind::SCOPED: {
                const auto node = std::static_pointer_cast<ScopedTypeNode>(type);
                std::string result;
                for (size_t i = 0; i < node->scopeParts.size(); ++i) {
                    if (i) result += "::";
                    result += node->scopeParts[i];
                }
                return result;
            }
            case TypeNodeKind::ARRAY: {
                const auto node = std::static_pointer_cast<ArrayTypeNode>(type);
                std::string suffix = "[]";
                if (node->isSizeDetermined) suffix = "[" + std::to_string(node->size) + "]";
                else if (node->isDynamic) suffix = "[dynamic]";
                return typeName(node->elementType) + suffix;
            }
            case TypeNodeKind::POINTER:
                return typeName(std::static_pointer_cast<PointerTypeNode>(type)->pointee) + "*";
            case TypeNodeKind::SHARED:
                return "shared " + typeName(std::static_pointer_cast<SharedTypeNode>(type)->innerType);
            case TypeNodeKind::FUNCTION: {
                const auto node = std::static_pointer_cast<FunctionTypeNode>(type);
                std::string result = "(";
                for (size_t i = 0; i < node->params.size(); ++i) {
                    if (i) result += ", ";
                    result += typeName(node->params[i]);
                }
                return result + ")->" + typeName(node->returnType);
            }
            case TypeNodeKind::RECORD: {
                const auto node = std::static_pointer_cast<RecordTypeNode>(type);
                std::string result = "{";
                for (size_t i = 0; i < node->paramTypePairs.size(); ++i) {
                    if (i) result += ", ";
                    result += node->paramTypePairs[i].first + ": " + typeName(node->paramTypePairs[i].second);
                }
                return result + "}";
            }
            case TypeNodeKind::NULLABLE:
                return typeName(std::static_pointer_cast<NullableTypeNode>(type)->innerType) + "?";
        }
        return "unknown";
    }();
    return syntax;
}

class AstJsonWriter {
public:
    explicit AstJsonWriter(std::ostream& out, bool shallow = false) : out(out), shallow(shallow) {}

    void writeExpr(const ExprPtr& expr) {
        if (!expr) { out << "null"; return; }
        pendingId = expr->traceId >= 0 ? "n" + std::to_string(expr->traceId) : "u" + std::to_string(nextId++);
        if (shallow && !owners.empty()) { out << "{\"id\":"; writeString(out, pendingId); out << ",\"ref\":true}"; return; }
        const auto* raw = expr.get();
        if (const auto* n = dynamic_cast<const Literal*>(raw)) {
            std::string value;
            std::visit([&](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) value = "null";
                else if constexpr (std::is_same_v<T, bool>) value = item ? "true" : "false";
                else if constexpr (std::is_same_v<T, std::string>) value = "\"" + item + "\"";
                else value = std::to_string(item);
            }, n->value);
            leaf("Literal", value);
        } else if (const auto* n = dynamic_cast<const ArrayLiteral*>(raw)) {
            begin("ArrayLiteral", "Array");
            bool first = true;
            for (size_t i = 0; i < n->elements.size(); ++i) child(first, "element[" + std::to_string(i) + "]", [&] { writeExpr(n->elements[i]); });
            end();
        } else if (const auto* n = dynamic_cast<const RecordLiteral*>(raw)) {
            begin("RecordLiteral", "Record literal");
            bool first = true;
            for (const auto& field : n->fields) child(first, field.first, [&] { writeExpr(field.second); });
            end();
        } else if (const auto* n = dynamic_cast<const Variable*>(raw)) {
            leaf("Variable", n->name);
        } else if (const auto* n = dynamic_cast<const UnaryExpr*>(raw)) {
            begin("UnaryExpr", unaryOpName(n->op)); bool first = true;
            child(first, "operand", [&] { writeExpr(n->operand); }); end();
        } else if (const auto* n = dynamic_cast<const BinaryExpr*>(raw)) {
            begin("BinaryExpr", binaryOpName(n->op)); bool first = true;
            child(first, "left", [&] { writeExpr(n->left); });
            child(first, "right", [&] { writeExpr(n->right); }); end();
        } else if (const auto* n = dynamic_cast<const Assignment*>(raw)) {
            begin("Assignment", assignmentOpName(n->op)); bool first = true;
            child(first, "target", [&] { writeExpr(n->left); });
            child(first, "value", [&] { writeExpr(n->right); }); end();
        } else if (const auto* n = dynamic_cast<const Index*>(raw)) {
            begin("Index", "Index access"); bool first = true;
            child(first, "object", [&] { writeExpr(n->obj); });
            child(first, "index", [&] { writeExpr(n->index); }); end();
        } else if (const auto* n = dynamic_cast<const Call*>(raw)) {
            begin("Call", "Function call"); bool first = true;
            child(first, "callee", [&] { writeExpr(n->func); });
            for (size_t i = 0; i < n->args.size(); ++i) child(first, "argument[" + std::to_string(i) + "]", [&] { writeExpr(n->args[i]); });
            end();
        } else if (const auto* n = dynamic_cast<const Get*>(raw)) {
            begin("Get", n->name); bool first = true;
            child(first, "object", [&] { writeExpr(n->obj); }); end();
        } else if (const auto* n = dynamic_cast<const ScopeAccessExpr*>(raw)) {
            std::string name;
            for (size_t i = 0; i < n->parts.size(); ++i) { if (i) name += "::"; name += n->parts[i]; }
            leaf("ScopeAccess", name);
        } else if (const auto* n = dynamic_cast<const FunctionExpr*>(raw)) {
            std::string label = n->isEntry ? "Program" : "Function";
            if (!n->isEntry) label += " → " + typeName(n->annotatedReturnType);
            begin(n->isEntry ? "Program" : "FunctionExpr", label); bool first = true;
            for (size_t i = 0; i < n->params.size(); ++i) {
                const auto& p = n->params[i];
                leafChild(first, "parameter[" + std::to_string(i) + "]", "Parameter", p.name + ": " + typeName(p.type));
                if (p.defaultValue) child(first, "default[" + std::to_string(i) + "]", [&] { writeExpr(p.defaultValue); });
            }
            child(first, "body", [&] { writeStmt(n->body); }); end();
        } else if (dynamic_cast<const ThisExpr*>(raw)) {
            leaf("ThisExpr", "this");
        } else if (const auto* n = dynamic_cast<const NewExpr*>(raw)) {
            std::string label = n->arrayType
                ? "new " + typeName(n->arrayType)
                : "new " + (n->allocatedType ? typeName(n->allocatedType) : n->typeName);
            begin("NewExpr", label); bool first = true;
            if (n->arraySize) child(first, "size", [&] { writeExpr(n->arraySize); });
            for (size_t i = 0; i < n->args.size(); ++i) child(first, "argument[" + std::to_string(i) + "]", [&] { writeExpr(n->args[i]); });
            end();
        } else {
            leaf("UnknownExpr", "Unknown expression");
        }
    }

    void writeStmt(const StmtPtr& stmt) {
        if (!stmt) { out << "null"; return; }
        pendingId = stmt->traceId >= 0 ? "n" + std::to_string(stmt->traceId) : "u" + std::to_string(nextId++);
        if (shallow && !owners.empty()) { out << "{\"id\":"; writeString(out, pendingId); out << ",\"ref\":true}"; return; }
        const auto* raw = stmt.get();
        if (const auto* n = dynamic_cast<const Print*>(raw)) {
            begin("Print", "print"); bool first = true; child(first, "value", [&] { writeExpr(n->expr); }); end();
        } else if (const auto* n = dynamic_cast<const If*>(raw)) {
            begin("If", "if"); bool first = true;
            child(first, "condition", [&] { writeExpr(n->condition); });
            child(first, "then", [&] { writeStmt(n->thenbranch); });
            if (n->elsebranch) child(first, "else", [&] { writeStmt(n->elsebranch); }); end();
        } else if (const auto* n = dynamic_cast<const While*>(raw)) {
            begin("While", "while"); bool first = true;
            child(first, "condition", [&] { writeExpr(n->condition); });
            child(first, "body", [&] { writeStmt(n->body); }); end();
        } else if (dynamic_cast<const Break*>(raw)) {
            leaf("Break", "break");
        } else if (dynamic_cast<const Continue*>(raw)) {
            leaf("Continue", "continue");
        } else if (const auto* n = dynamic_cast<const Block*>(raw)) {
            begin("Block", "Block"); bool first = true;
            for (size_t i = 0; i < n->statements.size(); ++i) child(first, "statement[" + std::to_string(i) + "]", [&] { writeStmt(n->statements[i]); });
            end();
        } else if (const auto* n = dynamic_cast<const Let*>(raw)) {
            std::string label = n->isMutable ? "var " : "let ";
            label += n->name + ": " + typeName(n->annotatedType);
            begin(n->isFunctionDecl ? "FunctionDeclaration" : "Let", label); bool first = true;
            if (n->expr) child(first, n->isFunctionDecl ? "function" : "initialiser", [&] { writeExpr(n->expr); }); end();
        } else if (const auto* n = dynamic_cast<const Return*>(raw)) {
            begin("Return", "return"); bool first = true;
            if (n->expr) child(first, "value", [&] { writeExpr(n->expr); }); end();
        } else if (const auto* n = dynamic_cast<const Aggregate*>(raw)) {
            const std::string kind = n->kind == AggregateKind::CLASS ? "Class" : "Record";
            begin(kind, kind + " " + n->name); bool first = true;
            for (const auto& field : n->fieldMembers) leafChild(first, "field", "Field", field.name + ": " + typeName(field.annotatedType));
            for (const auto& method : n->methodMembers) child(first, "method " + method.name, [&] { writeExpr(method.methodExpr); });
            for (const auto& constructor : n->constructorMembers) child(first, "constructor", [&] { writeExpr(constructor.initFuncExpr); });
            for (const auto& member : n->enumMembers) child(first, "enum", [&] { writeStmt(member.customEnum); });
            if (n->fieldInitFunc) child(first, "field initializers (generated)", [&] { writeExpr(n->fieldInitFunc); });
            end();
        } else if (const auto* n = dynamic_cast<const TypeAlias*>(raw)) {
            leaf("TypeAlias", n->name + " = " + typeName(n->aliasingType));
        } else if (const auto* n = dynamic_cast<const Enum*>(raw)) {
            begin("Enum", "enum " + n->name); bool first = true;
            for (const auto& variant : n->variants) leafChild(first, "variant", "EnumVariant", variant);
            end();
        } else if (const auto* n = dynamic_cast<const ExprStmt*>(raw)) {
            begin("ExprStmt", "Expression statement"); bool first = true; child(first, "expression", [&] { writeExpr(n->expr); }); end();
        } else {
            leaf("UnknownStmt", "Unknown statement");
        }
    }

private:
    std::ostream& out;
    int nextId = 0;
    bool shallow;
    std::string pendingId;
    std::vector<std::string> owners;

    void begin(const std::string& kind, const std::string& label) {
        owners.push_back(pendingId);
        out << "{\"id\":"; writeString(out, pendingId); out << ",\"kind\":";
        writeString(out, kind); out << ",\"label\":"; writeString(out, label); out << ",\"children\":[";
    }
    void end() { out << "]}"; owners.pop_back(); }
    void leaf(const std::string& kind, const std::string& label) { begin(kind, label); end(); }

    template <typename Writer>
    void child(bool& first, const std::string& role, Writer writer) {
        if (!first) out << ',';
        first = false;
        out << "{\"role\":"; writeString(out, role); out << ",\"node\":";
        writer(); out << '}';
    }
    void leafChild(bool& first, const std::string& role, const std::string& kind, const std::string& label) {
        pendingId = owners.back() + ":" + role;
        child(first, role, [&] { leaf(kind, label); });
    }
};

void writeTokens(std::ostream& out, const std::string& source, const std::vector<Token>& tokens) {
    out << '[';
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i) out << ',';
        const Token& token = tokens[i];
        const Position start = positionAt(source, token.startIdx);
        const Position end = positionAt(source, token.endIdx);
        out << "{\"id\":\"t" << i << "\",\"type\":";
        writeString(out, tokenTypeName(token.type));
        out << ",\"category\":"; writeString(out, tokenCategory(token.type));
        out << ",\"lexeme\":"; writeString(out, token.lexeme);
        out << ",\"span\":{\"start\":{\"offset\":" << token.startIdx
            << ",\"line\":" << start.line << ",\"column\":" << start.column
            << "},\"end\":{\"offset\":" << token.endIdx
            << ",\"line\":" << end.line << ",\"column\":" << end.column << "}}}";
    }
    out << ']';
}

void writeTrace(std::ostream& out, const std::vector<Token>& tokens, const std::vector<ParserTraceEvent>& trace) {
    out << '[';
    for (size_t i = 0; i < trace.size(); ++i) {
        if (i) out << ',';
        const auto& event = trace[i];
        const size_t tokenIndex = std::min(event.tokenIndex, tokens.empty() ? size_t{0} : tokens.size() - 1);
        out << "{\"step\":" << i << ",\"kind\":"; writeString(out, event.kind);
        out << ",\"tokenId\":\"t" << tokenIndex << "\"";
        out << ",\"callId\":" << event.callId << ",\"parentCallId\":" << event.parentCallId
            << ",\"depth\":" << event.depth << ",\"rule\":";
        writeString(out, event.rule);
        if (event.kind == "acceptOperator" || event.kind == "returnExpression" || event.kind == "enter") {
            out << ",\"minimumBindingPower\":" << event.minimumBindingPower
                << ",\"bindingPower\":" << event.bindingPower;
        }
        if (!event.nodeJson.empty()) out << ",\"node\":" << event.nodeJson;
        out << '}';
    }
    out << ']';
}

void writeDiagnostic(
    std::ostream& out,
    const std::string& source,
    const std::string& message,
    int start,
    int end
) {
    const Position startPosition = positionAt(source, start);
    const Position endPosition = positionAt(source, end);
    out << "{\"severity\":\"error\",\"phase\":\"parser\",\"message\":";
    writeString(out, message);
    out << ",\"span\":{\"start\":{\"offset\":" << start
        << ",\"line\":" << startPosition.line << ",\"column\":" << startPosition.column
        << "},\"end\":{\"offset\":" << end
        << ",\"line\":" << endPosition.line << ",\"column\":" << endPosition.column << "}}}";
}

} // namespace

std::string captureAstNode(const ASTNodePtr& node) {
    std::ostringstream out;
    AstJsonWriter writer(out, true);
    if (auto expr = std::dynamic_pointer_cast<BaseExpr>(node)) writer.writeExpr(expr);
    else if (auto stmt = std::dynamic_pointer_cast<BaseStmt>(node)) writer.writeStmt(stmt);
    else out << "null";
    return out.str();
}

void writeFrontendJson(
    std::ostream& out,
    const std::string& source,
    const std::vector<Token>& tokens,
    const std::vector<ParserTraceEvent>& parserTrace,
    const FunctionExprPtr& program
) {
    out << "{\"schemaVersion\":2,\"ok\":true,\"tokens\":";
    writeTokens(out, source, tokens);
    out << ",\"parser\":{\"strategy\":\"Pratt\",\"events\":";
    writeTrace(out, tokens, parserTrace);
    out << "},\"ast\":";
    AstJsonWriter(out).writeExpr(program);
    out << ",\"diagnostics\":[]}";
}

void writeFrontendErrorJson(
    std::ostream& out,
    const std::string& source,
    const std::vector<Token>& tokens,
    const std::vector<ParserTraceEvent>& parserTrace,
    const std::string& message,
    int,
    int start,
    int end
) {
    out << "{\"schemaVersion\":2,\"ok\":false,\"tokens\":";
    writeTokens(out, source, tokens);
    out << ",\"parser\":{\"strategy\":\"Pratt\",\"events\":";
    writeTrace(out, tokens, parserTrace);
    out << "},\"ast\":null,\"diagnostics\":[";
    writeDiagnostic(out, source, message, start, end);
    out << "]}";
}
