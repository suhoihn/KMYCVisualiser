#pragma once

#include <unordered_map>
#include <string>
#include "Symbol.hpp"

struct Scope {
    Scope* parent = nullptr;
    std::unordered_map<std::string, VarSymbol*> values; // Variable symbols in this scope.
    std::unordered_map<std::string, TypeSymbol*> types; // Type symbols here.
    int depth = 0; // TODO: unused?

    Scope() = default;

    Scope(Scope* parent, int depth)
        : parent(parent), depth(depth) {}
};
