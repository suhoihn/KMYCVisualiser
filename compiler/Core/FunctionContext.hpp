#pragma once

#include <vector>
#include <unordered_map>
#include "VariableStorage.hpp"
#include "Scope.hpp"

// Closure analyser builds this
struct FunctionContext {
    FunctionContext* parent = nullptr;
    
    std::vector<Local> locals;
    Scope* fnScope = nullptr;
    
    // Functions are declarated at the very start (after params)
    std::vector<std::pair<VarSymbol*, std::shared_ptr<struct FunctionExpr>>> funcDecls;
    std::unordered_map<VarSymbol*, int> localMap;
    
    // Constants are accessed via chunk.constants
    // This is handled in codegen part.
    // std::unordered_map<ConstValue, int> constantMap;

    int scopeDepth = 0;
    int nextSlot = 0;

    std::unordered_map<VarSymbol*, int> upvalueMap;
    std::vector<UpvalueInfo> upvalues;

    // For IR codegen
    std::unordered_map<VarSymbol*, int> envMap;
    int envSize = 0;
};
