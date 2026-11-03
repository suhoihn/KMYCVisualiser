#include "SymbolPrinter.hpp"

#include <iostream>
#include "../Core/Ast.hpp"

SymbolPrinter::SymbolPrinter(
    FunctionExprPtr program
) : program(program) {}

void SymbolPrinter::print() {
    program->accept(*this);
}

std::string SymbolPrinter::indent() {
    return std::string(depth * 2, ' ');
}

std::string typeToString(Type* type) {
    if (!type) return "UNASSIGNED";

    switch (type->kind) {
        case TypeKind::INT: return "int";
        case TypeKind::DOUBLE: return "double";
        case TypeKind::BOOL: return "bool";
        case TypeKind::STRING: return "string";
        case TypeKind::NULLTYPE: return "null";
        case TypeKind::VOID: return "void";
        case TypeKind::ANY: return "any";
        case TypeKind::UNKNOWN: return "unknown";
        case TypeKind::UNINITIALISED: return "uninitialised";
        case TypeKind::SHARED:
            return "shared " + typeToString(static_cast<const SharedType*>(type)->innerType);
        case TypeKind::NULLABLE:
            return typeToString(static_cast<const NullableType*>(type)->innerType) + "?";

        case TypeKind::ARRAY: {
            auto arr = static_cast<const ArrayType*>(type);
            if (arr->fixedLength.has_value()) {
                return typeToString(arr->elementType) + "[" + std::to_string(*arr->fixedLength) + "]";
            }
            return typeToString(arr->elementType) + "[]";
        }

        case TypeKind::FUNCTION: {
            auto fn = static_cast<const FunctionType*>(type);

            std::string s = "(";

            for (size_t i = 0; i < fn->paramTypes.size(); i++) {
                if (i) s += ", ";
                s += typeToString(fn->paramTypes[i]);
            }

            s += ") -> ";
            s += typeToString(fn->returnType);

            return s;
        }

        case TypeKind::STRUCTUAL: {
            auto st = static_cast<const StructualType*>(type);

            std::string s = "{ ";

            bool first = true;
            for (auto& [name, type] : st->fieldTypes) {
                if (!first) s += ", ";
                first = false;

                s += name + ": " + typeToString(type);
            }

            s += " }";
            return s;
        }

        case TypeKind::INSTANCE:
            return "instance";
        
        case TypeKind::ENUM:
            return "enum";

        case TypeKind::POINTER: {
            auto p = static_cast<const PointerType*>(type);

            return typeToString(p->pointee) + "*";
        }
        case TypeKind::CELL: {
            auto p = static_cast<const CellType*>(type);

            return "Cell<" + typeToString(p->pointee) + ">";
        }


        default:
            return "SEVERE: UNCLASSIFIED TYPE";
    }
}

void SymbolPrinter::printSymbol(VarSymbol* sym) {
    std::cout
    << indent() << sym->name << "(" << sym << ")\n"
    << indent() << "----------\n"
    << indent() << "mutable: " << std::string(sym->isMutable ? "true" : "false") << "\n"
    << indent() << "type: " << typeToString(sym->type) << "\n"
    << indent() << "local slot: " << (sym->slot == INVALID_SLOT ? std::string("UNASSIGNED") : std::to_string(sym->slot)) << "\n"
    //<< indent() << "captured: " << std::string(sym->captured ? "true" : "false") << "\n"
    //<< indent() << "upvalue slot: " << (sym->upvalueIndex == INVALID_SLOT ? std::string("UNASSIGNED") : std::to_string(sym->upvalueIndex)) << "\n"
    << indent() << "field offset: " << (sym->fieldOffset == INVALID_SLOT ? std::string("UNASSIGNED") : std::to_string(sym->fieldOffset)) << "\n"
    << indent() << "func proto index: " << (sym->funcProtoIdx == INVALID_SLOT ? std::string("UNASSIGNED") : std::to_string(sym->funcProtoIdx)) << "\n"
    << "\n";
}

void SymbolPrinter::visit(Literal&) {}
void SymbolPrinter::visit(Variable&) {}
void SymbolPrinter::visit(ArrayLiteral& e) {
    for (auto& elem : e.elements)
        elem->accept(*this);
}
void SymbolPrinter::visit(RecordLiteral& e) {
    for (auto& [_, value] : e.fields) {
        value->accept(*this);
    }
}
void SymbolPrinter::visit(BinaryExpr& e) {
    e.left->accept(*this);
    e.right->accept(*this);
}
void SymbolPrinter::visit(UnaryExpr& e) {
    e.operand->accept(*this);
}
void SymbolPrinter::visit(Assignment& e) {
    e.left->accept(*this);
    e.right->accept(*this);
}
void SymbolPrinter::visit(Index& e) {
    e.obj->accept(*this);
    e.index->accept(*this);
}
void SymbolPrinter::visit(Call& e) {
    e.func->accept(*this);
    for (auto& arg : e.args)
        arg->accept(*this);
}
void SymbolPrinter::visit(Get& e) {
    e.obj->accept(*this);
}
void SymbolPrinter::visit(ScopeAccessExpr&) {}
void SymbolPrinter::visit(ThisExpr&) {}
void SymbolPrinter::visit(NewExpr& e) {
    for (auto& arg : e.args)
        arg->accept(*this);
}

void SymbolPrinter::visit(FunctionExpr& e) {
    std::cout << indent() << "[Function (params)] (Scope: " << e.scope << ")\n";
    std::cout << indent() << "{\n";
    
    for(auto& [_, sym]: e.scope->values) {
        printSymbol(sym);
    }

    depth++;
    e.body->accept(*this);
    depth--;

    std::cout << indent() << "}\n";
}

// ======================================================
// Statements
// ======================================================

void SymbolPrinter::visit(Block& s) {
    std::cout << indent() << "[Block (locals)] (Scope: " << s.scope << ")\n";
    std::cout << indent() << "{\n";
    
    for(auto& [_, sym]: s.scope->values) {
        printSymbol(sym);
    }
    
    depth++;
    for (auto& stmt : s.statements) {
        stmt->accept(*this);
    }
    depth--;

    std::cout << indent() << "}\n";
}

void SymbolPrinter::visit(Print& s) {
    s.expr->accept(*this);
}
void SymbolPrinter::visit(If& s) {
    s.condition->accept(*this);

    s.thenbranch->accept(*this);

    if (s.elsebranch)
        s.elsebranch->accept(*this);
}
void SymbolPrinter::visit(While& s) {
    s.condition->accept(*this);
    s.body->accept(*this);
}
void SymbolPrinter::visit(Break&) {}
void SymbolPrinter::visit(Continue&) {}
void SymbolPrinter::visit(Return& s) {
    if (s.expr)
        s.expr->accept(*this);
}
void SymbolPrinter::visit(Let& s) {
    if (s.expr)
        s.expr->accept(*this);
}

void SymbolPrinter::visit(Aggregate& s) {

    std::cout << indent() << "[Aggregate (members)] (Scope: " << s.scope << ")\n";
    std::cout << indent() << "{\n";
    
    for(auto& [_, sym]: s.scope->values) {
        printSymbol(sym);
    }

    depth++;
    // Fields
    for (auto& member : s.fieldMembers) {
        if (member.initialiser)
            member.initialiser->accept(*this);
    }

    // Methods
    for (auto& member : s.methodMembers) {
        member.methodExpr->accept(*this);
    }

    // Constructors
    for (auto& member : s.constructorMembers) {
        member.initFuncExpr->accept(*this);
    }
    depth--;

    std::cout << indent() << "}\n";
}

void SymbolPrinter::visit(TypeAlias& s) {
    // TODO?
}

void SymbolPrinter::visit(Enum& s) {
    // TODO?
}

void SymbolPrinter::visit(ExprStmt& s) {
    s.expr->accept(*this);
}
