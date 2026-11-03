#pragma once

#include <vector>
#include "../Core/Ast.hpp"
#include "../Utils/DefaultVisitor.hpp"

struct Module;

// Pass 3: Method lowerer
// Lowers all function calls in form of 
//  obj.f(...) -> f(obj, ...)
// NOTE: The method's parameters already have implicit_this inserted in pass 1. 


class MethodLower : public DefaultVisitor {
public:
    MethodLower(Module& module);
    void lower();

private:
    Module& module;

    // Expressions
    void visit(Call& e) override;
};
