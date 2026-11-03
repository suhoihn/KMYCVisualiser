#include "DeclTypeResolver.hpp"
#include "../Core/newParser.hpp"

#include <vector>
#include "../Core/errorhandler.hpp"
#include "../Core/Ast.hpp"
#include <unordered_set>
#include <assert.h>
#include "TypeInterner.hpp"
#include "../Utils/SymbolPrinter.hpp"
#include "TypeHelpers.hpp"

DeclTypeResolver::DeclTypeResolver(
    Module& module
) : 
    module(module),
    globalScope(module.globalScope),
    currScope(globalScope)
{}

void DeclTypeResolver::resolve() {
    for (const StmtPtr& statement : module.topLevelStatements) {
        statement->accept(*this);
    }
}

// Expressions
void DeclTypeResolver::visit(FunctionExpr& e) {
    std::cout << "Resolving function signature\n";
    std::vector<Type*> paramTypes;
    std::vector<ParamTypeInfo> info;

    auto oldScope = currScope;

    for (auto& param : e.params) {
        // Params must be explicitly annoated.
        if (!param.symbol->type) {
            // This(^) check is present since param.symbol->type can be already defined
            // in which case, it should not be overriden.
            // Namely, implicit "this" parameter. 
            if (!param.type) {
                throw KMYCompileError("Function signature requires explicit type for now... templates, type inference when?");
                // No annotation means we don't know the type. Assume any.
                // We delegate this in runtime.
                param.symbol->type = &Types::ANY_TYPE;
    
                // If strict mode:
                // throw KMYCompileError("Parameter must have explicit type signature.");
            } else {
                std::cout << "Assigning annotated parameter type: " << param.name << '\n';
                param.symbol->type = typeSigToType(module, currScope, param.type);
            }
        }
        paramTypes.push_back(param.symbol->type);

        if (param.defaultExists) {
            param.defaultValue->accept(*this);
            // Param type check is not done here.
            // Done in pass 3.
        }
        
        // Param is now usable (prevents f(a = a) or f(a = b, b = 3) style errors.)
        param.symbol->available = true;

        // TODO: why defaultExists needed?
        info.push_back({param.defaultExists, param.isVariadic, param.implicitThis, param.symbol->type});
    }

    e.body->accept(*this);
    
    currScope = oldScope;

    // IMPORTANT NOTE: SEMANTIC INFO IS LOST WHEN ANNOTATED.
    FunctionType* fnType = TypeInterner::getFunctionType(
        std::move(paramTypes),
        e.annotatedReturnType 
            ? typeSigToType(module, currScope, e.annotatedReturnType)
            : &Types::ANY_TYPE
    );

    fnType->info = std::move(info);
    fnType->infoExists = true;
    e.type = fnType;
    std::cout << "my type is " << typeToString(e.type) << "\n";
}

void DeclTypeResolver::visit(ThisExpr& e) {
    if (!currentThis) {
        throw KMYCompileError("\"this\" used outside of method... :(");
    }
    std::cout << "this symbol=" << currentThis
              << " type=" << static_cast<InstanceType*>(currentThis->type)->name << '\n';

    e.symbol = currentThis;
    e.type = currentThis->type;
}


// Statements
void DeclTypeResolver::visit(Block& s) {
    Scope* old = currScope;

    currScope = s.scope; // assigned in Pass 1

    for (auto& stmt : s.statements)
        stmt->accept(*this);

    currScope = old;
}


void DeclTypeResolver::visit(Let& s) {
    printLog(LogLevel::DEBUG, "Visiting let node for " + s.name + "\n");
    
    if (s.expr) {
        s.expr->accept(*this);
        
        if (s.isFunctionDecl) {
            s.symbol->type = s.expr->type; 
        }
    }
}


void DeclTypeResolver::visit(Aggregate& s) {
    // Aggregate type is made early. Initially its inner types are empty.
    auto oldAgg = currentAggregate;
    InstanceType* aggType = new InstanceType();
    aggType->name = s.name; // DEBUG

    // Aggregate type symbol is built in pass 1.
    assert(s.typeSymbol);

    s.typeSymbol->type = aggType;
    currentAggregate = aggType;

    auto oldScope = currScope;
    currScope = s.scope;

    // Enum first
    for (auto& member : s.enumMembers) {
        member.customEnum->accept(*this);

        aggType->enumMap[member.customEnum->name] = member.customEnum->typeSymbol;
    }

    for (auto& field : s.fieldMembers) {
        if (field.initialiser) {
            field.initialiser->accept(*this);
        }
    
        aggType->fieldMap[field.name] = field.symbol;
    }
    
    auto oldThis = currentThis; // Do i need this?
    for (auto& member : s.methodMembers) {
        // Note: First parameter is ALWAYS implicit "this" (from pass 1)
        currentThis = member.methodExpr->params[0].symbol;
        currentThis->type = currentAggregate;

        member.methodExpr->accept(*this);
        
        member.symbol->type = member.methodExpr->type; 
        aggType->methodMap[member.name] = member.symbol;
    }


    for (auto& member : s.constructorMembers) {
        currentThis = member.initFuncExpr->params[0].symbol;
        currentThis->type = currentAggregate;

        member.initFuncExpr->accept(*this);
        // Constructor's return type doesn't matter.
        // Its param types are important though.
        member.symbol->type = member.initFuncExpr->type;
        aggType->constructorVec.push_back(member.symbol);
    }

    // Field initialiser (TODO: This should not be here. it should be after pass 3)
    currentThis = s.fieldInitFunc->params[0].symbol;
    currentThis->type = currentAggregate;
    s.fieldInitFunc->accept(*this);
    aggType->fieldInitializerType = static_cast<FunctionType*>(s.fieldInitFunc->type);

    currentThis = oldThis;
    currentAggregate = oldAgg;
    currScope = oldScope;
}

void DeclTypeResolver::visit(TypeAlias& s) {
    /*
    typealias B = A; 
        - B typeSymbol->type = typeSigToType(A)
        - A is scopedNode, lookup typeSymbol for "A" in currScope
        - A typeSymbol exists. Its type is nullptr
        - So checks its typeNode and calls typeSigToType(int)
        - Gets &Types::INT_TYPE
        - A typeSymbol->type = &Types::INT_TYPE
        - B typeSymbol->type = &Types::INT_TYPE

    typealias A = int;
        - A typeSymbol->type is not null, so skipped

    typealias C = B[];
        - B typeSymbol->type = typeSigToType( Array(B) )
        - Array(B) is arrayNode, element typeNode is B
        - lookup typeSymbol for "B" in currScope
        - B typeSymbol exists. Its type is &Types::INT_TYPE
        - Simply returns &Types::INT_TYPE
        - Builds ArrayType with elementType = &Types::INT_TYPE
    */
    if (s.typeSymbol->type) {
        s.typeSymbol->type = typeSigToType(module, currScope, s.aliasingType);
    }
}

void DeclTypeResolver::visit(Enum& s) {
    //printLog(LogLevel::DEBUG, "Visiting enum node for " + s.name + "\n");
    
    auto* enumType = new EnumType;
    
    // TODO: To type interner?
    int offset = 0;
    for (auto& str : s.variants) {
        enumType->variantMap[str] = offset++;
    }
    
    s.typeSymbol->type = enumType;
    
    //printLog(LogLevel::DEBUG, "Enum offset is " + std::to_string(offset) + " for " + s.name + "\n");
}
