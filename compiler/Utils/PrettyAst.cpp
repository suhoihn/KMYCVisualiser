#include "PrettyAst.hpp"

#include <ostream>
#include <string>
#include <type_traits>

#include "../Core/Ast.hpp"
#include "SymbolPrinter.hpp"

namespace {

std::string escaped(const std::string& value) {
    std::string result;
    for (char c : value) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string typeSyntax(const TypeNodePtr& type) {
    if (!type) return "inferred";
    std::string syntax = [&]() -> std::string {
        switch (type->kind) {
            case TypeNodeKind::NAMED:
                return static_cast<const NamedTypeNode&>(*type).name;
            case TypeNodeKind::SCOPED: {
                std::string result;
                for (const auto& part : static_cast<const ScopedTypeNode&>(*type).scopeParts) {
                    if (!result.empty()) result += "::";
                    result += part;
                }
                return result;
            }
            case TypeNodeKind::POINTER:
                return typeSyntax(static_cast<const PointerTypeNode&>(*type).pointee) + "*";
            case TypeNodeKind::SHARED:
                return "shared " + typeSyntax(static_cast<const SharedTypeNode&>(*type).innerType);
            case TypeNodeKind::NULLABLE:
                return typeSyntax(static_cast<const NullableTypeNode&>(*type).innerType) + "?";
            case TypeNodeKind::ARRAY: {
                const auto& array = static_cast<const ArrayTypeNode&>(*type);
                return typeSyntax(array.elementType) +
                    (array.isSizeDetermined ? "[" + std::to_string(array.size) + "]" : "[]");
            }
            case TypeNodeKind::FUNCTION: {
                const auto& function = static_cast<const FunctionTypeNode&>(*type);
                std::string result = "(";
                for (size_t i = 0; i < function.params.size(); ++i) {
                    if (i) result += ", ";
                    result += typeSyntax(function.params[i]);
                }
                return result + ") -> " + typeSyntax(function.returnType);
            }
            case TypeNodeKind::RECORD: {
                const auto& record = static_cast<const RecordTypeNode&>(*type);
                std::string result = "{";
                for (size_t i = 0; i < record.paramTypePairs.size(); ++i) {
                    if (i) result += ", ";
                    result += record.paramTypePairs[i].first + ": " + typeSyntax(record.paramTypePairs[i].second);
                }
                return result + "}";
            }
        }
        return "<?>";
    }();
    return syntax;
}

const char* binarySymbol(BinaryOp op) {
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

const char* unarySymbol(UnaryOp op) {
    switch (op) {
        case UnaryOp::Plus: return "+";
        case UnaryOp::Minus: return "-";
        case UnaryOp::LogicalNot: return "!";
        case UnaryOp::BitNot: return "~";
        case UnaryOp::AddressOf: return "&";
        case UnaryOp::Dereference: return "*";
        case UnaryOp::ForceUnwrap: return "!!";
    }
    return "?";
}

const char* assignmentSymbol(AssignmentOp op) {
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

void line(std::ostream& out, int depth, const std::string& role, const std::string& label) {
    out << std::string(depth * 2, ' ') << role << label << '\n';
}

void expr(std::ostream& out, const ExprPtr& node, int depth, const std::string& role);
void stmt(std::ostream& out, const StmtPtr& node, int depth, const std::string& role);

void expr(std::ostream& out, const ExprPtr& node, int depth, const std::string& role) {
    if (!node) { line(out, depth, role, "<none>"); return; }
    const auto* raw = node.get();
    const std::string type = node->type ? " : " + typeToString(node->type) : "";

    if (const auto* n = dynamic_cast<const Literal*>(raw)) {
        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) line(out, depth, role, "null" + type);
            else if constexpr (std::is_same_v<T, bool>) line(out, depth, role, std::string(value ? "true" : "false") + type);
            else if constexpr (std::is_same_v<T, std::string>) line(out, depth, role, "\"" + escaped(value) + "\"" + type);
            else line(out, depth, role, std::to_string(value) + type);
        }, n->value);
    } else if (const auto* n = dynamic_cast<const Variable*>(raw)) {
        line(out, depth, role, "name " + n->name + type);
    } else if (const auto* n = dynamic_cast<const UnaryExpr*>(raw)) {
        line(out, depth, role, std::string("unary ") + unarySymbol(n->op) + type);
        expr(out, n->operand, depth + 1, "operand: ");
    } else if (const auto* n = dynamic_cast<const BinaryExpr*>(raw)) {
        line(out, depth, role, std::string("binary ") + binarySymbol(n->op) + type);
        expr(out, n->left, depth + 1, "left: ");
        expr(out, n->right, depth + 1, "right: ");
    } else if (const auto* n = dynamic_cast<const Assignment*>(raw)) {
        line(out, depth, role, std::string("assignment ") + assignmentSymbol(n->op) + type);
        expr(out, n->left, depth + 1, "target: ");
        expr(out, n->right, depth + 1, "value: ");
    } else if (const auto* n = dynamic_cast<const ArrayLiteral*>(raw)) {
        line(out, depth, role, "array literal" + type);
        for (size_t i = 0; i < n->elements.size(); ++i)
            expr(out, n->elements[i], depth + 1, "element[" + std::to_string(i) + "]: ");
    } else if (const auto* n = dynamic_cast<const RecordLiteral*>(raw)) {
        line(out, depth, role, "record literal" + type);
        for (const auto& [name, value] : n->fields)
            expr(out, value, depth + 1, "field " + name + ": ");
    } else if (const auto* n = dynamic_cast<const Index*>(raw)) {
        line(out, depth, role, "index" + type);
        expr(out, n->obj, depth + 1, "array: ");
        expr(out, n->index, depth + 1, "index: ");
    } else if (const auto* n = dynamic_cast<const Call*>(raw)) {
        line(out, depth, role, "call" + type);
        expr(out, n->func, depth + 1, "callee: ");
        for (size_t i = 0; i < n->args.size(); ++i)
            expr(out, n->args[i], depth + 1, "argument[" + std::to_string(i) + "]: ");
    } else if (const auto* n = dynamic_cast<const Get*>(raw)) {
        line(out, depth, role, "field ." + n->name + type);
        expr(out, n->obj, depth + 1, "object: ");
    } else if (const auto* n = dynamic_cast<const ScopeAccessExpr*>(raw)) {
        std::string name;
        for (const auto& part : n->parts) { if (!name.empty()) name += "::"; name += part; }
        line(out, depth, role, name + type);
    } else if (const auto* n = dynamic_cast<const FunctionExpr*>(raw)) {
        line(out, depth, role, n->isEntry ? "program" : "function -> " + typeSyntax(n->annotatedReturnType) + type);
        for (const auto& param : n->params) {
            line(out, depth + 1, "parameter: ", param.name + ": " + typeSyntax(param.type) +
                 (param.implicitThis ? " (implicit this)" : ""));
            if (param.defaultValue) expr(out, param.defaultValue, depth + 2, "default: ");
        }
        stmt(out, n->body, depth + 1, "body: ");
    } else if (dynamic_cast<const ThisExpr*>(raw)) {
        line(out, depth, role, "this" + type);
    } else if (const auto* n = dynamic_cast<const NewExpr*>(raw)) {
        const std::string allocatedType = n->arrayType
            ? typeSyntax(n->arrayType)
            : n->allocatedType ? typeSyntax(n->allocatedType) : n->typeName;
        line(out, depth, role, "new " + allocatedType + type);
        if (n->arraySize) expr(out, n->arraySize, depth + 1, "size: ");
        for (size_t i = 0; i < n->args.size(); ++i)
            expr(out, n->args[i], depth + 1, "argument[" + std::to_string(i) + "]: ");
    } else {
        line(out, depth, role, "<unknown expression>");
    }
}

void stmt(std::ostream& out, const StmtPtr& node, int depth, const std::string& role) {
    if (!node) { line(out, depth, role, "<none>"); return; }
    const auto* raw = node.get();
    if (const auto* n = dynamic_cast<const Block*>(raw)) {
        line(out, depth, role, "block (" + std::to_string(n->statements.size()) + " statements)");
        for (size_t i = 0; i < n->statements.size(); ++i)
            stmt(out, n->statements[i], depth + 1, "statement[" + std::to_string(i) + "]: ");
    } else if (const auto* n = dynamic_cast<const Let*>(raw)) {
        std::string label = std::string(n->isMutable ? "var " : "let ") + n->name;
        if (n->annotatedType) label += ": " + typeSyntax(n->annotatedType);
        if (n->symbol && n->symbol->type && !n->annotatedType) label += " : " + typeToString(n->symbol->type);
        line(out, depth, role, label);
        if (n->expr) expr(out, n->expr, depth + 1, n->isFunctionDecl ? "function: " : "initializer: ");
    } else if (const auto* n = dynamic_cast<const Print*>(raw)) {
        line(out, depth, role, "print");
        expr(out, n->expr, depth + 1, "value: ");
    } else if (const auto* n = dynamic_cast<const If*>(raw)) {
        line(out, depth, role, "if");
        expr(out, n->condition, depth + 1, "condition: ");
        stmt(out, n->thenbranch, depth + 1, "then: ");
        if (n->elsebranch) stmt(out, n->elsebranch, depth + 1, "else: ");
    } else if (const auto* n = dynamic_cast<const While*>(raw)) {
        line(out, depth, role, "while");
        expr(out, n->condition, depth + 1, "condition: ");
        stmt(out, n->body, depth + 1, "body: ");
    } else if (dynamic_cast<const Break*>(raw)) {
        line(out, depth, role, "break");
    } else if (dynamic_cast<const Continue*>(raw)) {
        line(out, depth, role, "continue");
    } else if (const auto* n = dynamic_cast<const Return*>(raw)) {
        line(out, depth, role, "return");
        if (n->expr) expr(out, n->expr, depth + 1, "value: ");
    } else if (const auto* n = dynamic_cast<const Aggregate*>(raw)) {
        line(out, depth, role, std::string(n->kind == AggregateKind::CLASS ? "class " : "record ") + n->name);
        for (const auto& field : n->fieldMembers) {
            line(out, depth + 1, "field: ",
                 std::string(field.isMutable ? "var " : "let ") +
                 field.name + ": " + typeSyntax(field.annotatedType));
            if (field.initialiser) expr(out, field.initialiser, depth + 2, "initializer: ");
        }
        for (const auto& method : n->methodMembers)
            expr(out, method.methodExpr, depth + 1, "method " + method.name + ": ");
        for (const auto& constructor : n->constructorMembers)
            expr(out, constructor.initFuncExpr, depth + 1, "init: ");
        for (const auto& member : n->enumMembers)
            stmt(out, member.customEnum, depth + 1, "nested enum: ");
    } else if (const auto* n = dynamic_cast<const Enum*>(raw)) {
        line(out, depth, role, "enum " + n->name);
        for (size_t i = 0; i < n->variants.size(); ++i)
            line(out, depth + 1, "variant[" + std::to_string(i) + "]: ", n->variants[i]);
    } else if (const auto* n = dynamic_cast<const TypeAlias*>(raw)) {
        line(out, depth, role, "typealias " + n->name + " = " + typeSyntax(n->aliasingType));
    } else if (const auto* n = dynamic_cast<const ExprStmt*>(raw)) {
        expr(out, n->expr, depth, role);
    } else {
        line(out, depth, role, "<unknown statement>");
    }
}

} // namespace

void printPrettyAST(std::ostream& out, const ExprPtr& root) {
    expr(out, root, 0, "");
}
