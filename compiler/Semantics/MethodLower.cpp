#include "MethodLower.hpp"
#include "../Core/newParser.hpp"

#include <vector>
#include "../Core/errorhandler.hpp"
#include "../Core/Ast.hpp"
#include <unordered_set>

MethodLower::MethodLower(
    Module& module
) : module(module) {}

void MethodLower::lower() {
    for (const StmtPtr& statement : module.topLevelStatements) {
        statement->accept(*this);
    }
}

void MethodLower::visit(Call& e) {
    // Lower children first
    e.func->accept(*this);
    for (auto& arg : e.args) {
        arg->accept(*this);
    }
    
    // Lowering obj.f(...) form to f(obj, ...)
    // NOTE: f must be a method, not a function!
    if (e.func->kind == ExprKind::Get) {
        auto callee = std::static_pointer_cast<Get>(e.func);
        // change maybe. hard to track.
        if (callee->resolvedMethod) {
            // Instance object.
            auto obj = callee->obj;
            e.args.push_back(obj); // REALLY DONT KNOW WHY THIS WORKS LOL
            //e.args.insert(e.args.begin(), obj);
        }
        // HACK!!!!
    }
}
