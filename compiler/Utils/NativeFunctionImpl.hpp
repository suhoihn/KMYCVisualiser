#pragma once

#include <string>
#include "../Core/type.hpp"
#include "../Core/value.hpp"

struct NativeEntry {
    FunctionType* type;
    NativeFnPtr fn;
    int globalSlot;
};

extern std::unordered_map<std::string, NativeEntry> nativeFnTypes;