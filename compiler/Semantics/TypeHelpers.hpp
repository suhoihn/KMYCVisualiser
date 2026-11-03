#pragma once

#include "../Core/Scope.hpp"
#include "../Core/Symbol.hpp"
#include "../Utils/utils.hpp"
#include "../Core/errorhandler.hpp"
#include "../Core/newParser.hpp"
#include <iostream>
#include "TypeInterner.hpp"

// Converts TypeSymbol* to Type*
// This may be trivial if sym->type is defined.
// Otherwise, it walks sym->typeNode via typeSigToType
// Forward decl.
inline Type* typeSigToType(Module& module, Scope* currScope, const TypeNodePtr& type);

inline Type* resolveTypeSymbol(Module& module, Scope* currScope, TypeSymbol* sym) {
    if (!sym)
        throw KMYCompileError("Null type symbol");

    // Basic case: TypeSymbol already has type resolved.
    if (sym->type)
        return sym->type;

    if (sym->resolving)
        throw KMYCompileError("Cyclic type alias: " + sym->name);
        
    sym->resolving = true;
        
    // Otherwise, resolving TypeSymbol's type expression AST.
    if (!sym->typeNode) {
        throw KMYCompileError("Type symbol's AST is nullptr. This is a serious bug.");
    }
    sym->type = typeSigToType(module, currScope, sym->typeNode);

    sym->resolving = false;

    return sym->type;
}

inline TypeSymbol* lookupTypeSymbol(Scope* currScope, const std::string& name) {
    Scope* scope = currScope;
    printLog(LogLevel::DEBUG, "Looking up type symbol: " + name + "\n");

    while (scope) {
        auto it = scope->types.find(name);

        if (it != scope->types.end()) {
            return it->second;
        }

        scope = scope->parent;
    }

    printLog(LogLevel::DEBUG, "Type symbol not found: " + name + "\n");
    return nullptr; // Undefined type name.
}

static std::unordered_map<std::string, Type*> primitiveToType = {
    {"int", &Types::INT_TYPE},
    {"double", &Types::DOUBLE_TYPE},
    {"bool", &Types::BOOL_TYPE},
    {"string", &Types::STRING_TYPE},
    {"null", &Types::NULL_TYPE},
    {"void", &Types::VOID_TYPE},
    {"any", &Types::ANY_TYPE},
    // Native-width aliases until narrower integer/float layouts are added.
    {"i64", &Types::INT_TYPE},
    {"u64", &Types::INT_TYPE},
    {"f64", &Types::DOUBLE_TYPE},
    {"byte", &Types::INT_TYPE},
};

// Converts TypeNodePtr to Type*
inline Type* typeSigToType(Module& module, Scope* currScope, const TypeNodePtr& type) {
    printLog(LogLevel::DEBUG, "Converting type annotation to Type*: (TODO...)\n" );
    
    if (!type) {
        throw KMYCompileError("Type annotation is null.");
    }
    switch (type->kind) {
        // Primitives
        case TypeNodeKind::NAMED: {
            const auto& named = static_cast<NamedTypeNode&>(*type);

            auto it = primitiveToType.find(named.name);
            if (it != primitiveToType.end()) {
                std::cout << "found!\n";
                return it->second;
            }
            throw KMYCompileError("Serious error. Unhandled primitive: " + named.name);
        }

        case TypeNodeKind::ARRAY: {
            std::cout << "ur array\n";
            const auto& arrayTypeNode = static_cast<ArrayTypeNode&>(*type);
            Type* elementType = typeSigToType(module, currScope, arrayTypeNode.elementType);
            if (arrayTypeNode.isSizeDetermined && !arrayTypeNode.isDynamic) {
                return TypeInterner::getArrayType(
                    elementType,
                    static_cast<size_t>(arrayTypeNode.size)
                );
            }
            return TypeInterner::getArrayType(elementType);
        }

        case TypeNodeKind::POINTER: {
            const auto& pointer = static_cast<PointerTypeNode&>(*type);
            return TypeInterner::getPointerType(typeSigToType(module, currScope, pointer.pointee));
        }

        case TypeNodeKind::SHARED: {
            const auto& shared = static_cast<SharedTypeNode&>(*type);
            // This only interns the semantic wrapper. RC operations are emitted
            // later by NewExpr, assignment, call, and scope-exit lowering.
            return TypeInterner::getSharedType(typeSigToType(module, currScope, shared.innerType));
        }

        case TypeNodeKind::NULLABLE: {
            const auto& nullable = static_cast<NullableTypeNode&>(*type);
            return new NullableType(typeSigToType(module, currScope, nullable.innerType));
        }

        case TypeNodeKind::FUNCTION: {
            std::cout << "ur function\n";
            const auto& funcTypeNode = static_cast<FunctionTypeNode&>(*type);
            std::vector<Type*> paramTypes;
            for (const auto& param : funcTypeNode.params) {
                paramTypes.push_back(typeSigToType(module, currScope, param));
            }
            Type* returnType = typeSigToType(module, currScope, funcTypeNode.returnType);
            // IMPORTANT NOTE: No param info is preserved. (default value, var arg etc.)
            return TypeInterner::getFunctionType(std::move(paramTypes), returnType);
        }

        case TypeNodeKind::RECORD: {
            std::cout << "ur record\n";
            const auto& recordTypeNode = static_cast<RecordTypeNode&>(*type);
            std::unordered_map<std::string, Type*> fieldTypes;

            for(auto& pair : recordTypeNode.paramTypePairs) {
                if (fieldTypes.count(pair.first)) {
                    throw KMYCompileError(
                        "Redeclaration of field name \"" + pair.first + "\" in type signature."
                    );
                }
                fieldTypes[pair.first] = typeSigToType(module, currScope, pair.second);
            }

            return TypeInterner::getStructualType(std::move(fieldTypes));
        }
        
        case TypeNodeKind::SCOPED: {
            std::cout << "ur scoped\n";
            const auto& scopedTypeNode = static_cast<ScopedTypeNode&>(*type);

            const std::string& firstPart = scopedTypeNode.scopeParts[0];
            Module* typeModule = &module;
            Scope* typeScope = currScope;
            size_t typePart = 0;

            // `math::Vector` starts from the imported module's global type
            // scope. A non-import first part retains normal local/nested-type
            // resolution, so existing `Outer::Inner` syntax is unchanged.
            auto import = module.imports.find(firstPart);
            if (import != module.imports.end()) {
                typeModule = import->second;
                typeScope = typeModule->globalScope;
                typePart = 1;
                if (typePart == scopedTypeNode.scopeParts.size()) {
                    throw KMYCompileError("Module alias \"" + firstPart + "\" is not a type.");
                }
            }

            const std::string& typeName = scopedTypeNode.scopeParts[typePart++];
            TypeSymbol* typeSym = lookupTypeSymbol(typeScope, typeName);
            if (!typeSym) {
                throw KMYCompileError("Unknown type symbol \"" + typeName + "\" in scoped type");
            }

            Type* currType = resolveTypeSymbol(*typeModule, typeScope, typeSym);

            // Resolve remaining nested aggregate/enum type members.
            for (size_t i = typePart; i < scopedTypeNode.scopeParts.size(); i++) {
                const std::string& currPart = scopedTypeNode.scopeParts[i];

                if (currType->kind != TypeKind::INSTANCE) {
                    throw KMYCompileError("Only aggregates support nested ::");
                }

                auto aggType = static_cast<InstanceType*>(currType);

                std::cout << "enummap contains:\n";
                for (auto& [f, _] : aggType->enumMap) {
                    std::cout << f << "\n";
                }

                auto it = aggType->enumMap.find(currPart);
                if (it == aggType->enumMap.end()) {
                    throw KMYCompileError(
                        "No type symbol \"" + currPart + "\" found in " + aggType->name
                    );
                }

                currType = it->second->type;
            }

            return currType;
        }
    }

    throw KMYCompileError("Severe: Unknown type node kind.");
}
