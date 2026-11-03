#include "DefaultVisitor.hpp"

#include <iostream>
#include "../Core/Ast.hpp"
#include "utils.hpp"

// TODO: finish adding printLogs here...
void DefaultVisitor::visit(Literal&) {
    printLog(LogLevel::DEBUG, "Visited literal node.\n");
}
void DefaultVisitor::visit(Variable&) {
    printLog(LogLevel::DEBUG, "Visited variable node.\n");
}
void DefaultVisitor::visit(ArrayLiteral& e) {
    printLog(LogLevel::DEBUG, "Entering array literal node.\n");

    for (auto& elem : e.elements)
        elem->accept(*this);
    
    printLog(LogLevel::DEBUG, "Exiting array literal node.\n");
}
void DefaultVisitor::visit(RecordLiteral& e) {
    for (auto& [_, value] : e.fields) {
        value->accept(*this);
    }
}
void DefaultVisitor::visit(BinaryExpr& e) {
    e.left->accept(*this);
    e.right->accept(*this);
}
void DefaultVisitor::visit(UnaryExpr& e) {
    e.operand->accept(*this);
}
void DefaultVisitor::visit(Assignment& e) {
    e.left->accept(*this);
    e.right->accept(*this);
}
void DefaultVisitor::visit(Index& e) {
    e.obj->accept(*this);
    e.index->accept(*this);
}
void DefaultVisitor::visit(Call& e) {
    e.func->accept(*this);
    for (auto& arg : e.args)
        arg->accept(*this);
}
void DefaultVisitor::visit(Get& e) {
    e.obj->accept(*this);
}
void DefaultVisitor::visit(ScopeAccessExpr&) {}
void DefaultVisitor::visit(ThisExpr&) {}
void DefaultVisitor::visit(NewExpr& e) {
    for (auto& arg : e.args)
        arg->accept(*this);
}
void DefaultVisitor::visit(FunctionExpr& e) {
    for (const auto& param : e.params) {
        if (param.defaultValue) {
            param.defaultValue->accept(*this);
        }
    }

    e.body->accept(*this);
}

// ======================================================
// Statements
// ======================================================

void DefaultVisitor::visit(Block& s) {
    for (auto& stmt : s.statements) {
        stmt->accept(*this);
    }
}
void DefaultVisitor::visit(Print& s) {
    s.expr->accept(*this);
}
void DefaultVisitor::visit(If& s) {
    s.condition->accept(*this);

    s.thenbranch->accept(*this);

    if (s.elsebranch)
        s.elsebranch->accept(*this);
}
void DefaultVisitor::visit(While& s) {
    s.condition->accept(*this);
    s.body->accept(*this);
}
void DefaultVisitor::visit(Break&) {}
void DefaultVisitor::visit(Continue&) {}
void DefaultVisitor::visit(Return& s) {
    if (s.expr)
        s.expr->accept(*this);
}
void DefaultVisitor::visit(Let& s) {
    if (s.expr)
        s.expr->accept(*this);
}
void DefaultVisitor::visit(Aggregate& s) {
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

    // Enums
    for (auto& member : s.enumMembers) {
        member.customEnum->accept(*this);
    }
}
void DefaultVisitor::visit(TypeAlias& s) {}
void DefaultVisitor::visit(Enum& s) {}
void DefaultVisitor::visit(ExprStmt& s) {
    s.expr->accept(*this);
}