#pragma once

#include <string>
#include "AstBaseForward.hpp"

inline constexpr int INVALID_SLOT = -1;

// Forward decl.
struct Type;
struct Scope;

enum class SymbolKind {
    VARIABLE, // "let x", "fun f" build them (+ aggregate member decls).
    TYPE // "record/class X", "typealias IntArr", "enum E" build them.
};

// Symbol is the semantic identity of a variable/function/type.
struct Symbol {
    std::string name;
    SymbolKind kind;

    Symbol(const std::string& name, SymbolKind kind)
        : name(name), kind(kind) {}

    virtual ~Symbol() = default;
};


struct VarSymbol : Symbol {
    bool isMutable;
    Type* type = nullptr;

    // On symbol creation, the symbol exists on scope's map.
    // However, you cannot use before actually passing through the decl again in pass 2.
    /*
        {
            print(x)     // Here, "x" has a symbol in its scope but is in an unavailable state.
            let x = 10;  // "x" is now available.
            print(x);    // Legal usage.
        }
        print(x); // "x" is not in its scope, so undefined.
    */

    // However, func DECLs have their symbols available on creation.
    // This allows for mutual recursions.
    /*
        printX(); // This is fine.

        fun printX() {
            print("x");
        }
    */
    
    // Still, func EXPRs follow var decl orders.
    /*
        printX(); // Again, printX is in an unavailable state.
    
        let printX = fun() { print("x"); };
    */
   
    // To use it in mutual recursion, you can forward declare.
    /*
        let odd: (int) -> bool;

        let even = fun(n: int): bool {
            if (n < 0) { return false; }
            if (n == 0) { return true; }
            return odd(n - 1);
        };

        odd = fun(n: int): bool {
            if (n < 0) { return false; }
            if (n == 1) { return true; }
            return even(n - 1);
        };

        print(even(10));
    */

    // Also, availability is set AFTER the init expr visit.
    // Otherwise, let x = x; is allowed, which causes weird errors.

    bool available = false;

    // To later answer: "In which scope is this defined in?" or "Is this defined in a scope or in its children?"
    Scope* definingScope = nullptr;

    // Fields below are ONLY for debug. Each pass (or phase) has its own storage of the same info.
    
    // Runtime storage info for locals and upvalues (if used)
    int slot = INVALID_SLOT;
    
    // Aggregate Info (if used)
    int fieldOffset = INVALID_SLOT;
    int methodIdx = INVALID_SLOT; // For vtables too...?
    
    // Function Info (if used)
    int funcProtoIdx = INVALID_SLOT;
    
    // Native function info (if used)
    NativeFnPtr nativeFnPtr = nullptr;
    int globalSlot = INVALID_SLOT;

    // Native x86 function label for a KMY function declaration. Closure
    // analysis assigns this from one compilation-wide counter, so an imported
    // reference can materialize a closure for the correct emitted function.
    int nativeFunctionId = INVALID_SLOT;

    // Persistent storage assigned to a top-level module value. This is kept
    // separate from `globalSlot`, whose existing values identify native
    // built-ins in the VM/runtime table.
    bool isModuleGlobal = false;
    int moduleGlobalSlot = INVALID_SLOT;

    VarSymbol(const std::string& name, bool isMutable)
        : Symbol(name, SymbolKind::VARIABLE), isMutable(isMutable) {}
};


struct TypeSymbol : Symbol {
    bool isMutable;
    // TODO: Move this field to base symbol?
    Type* type = nullptr;

    // Used this only for type expression defined ones.
    // Namely, typealias.
    TypeNodePtr typeNode = nullptr;
    bool resolving = false; // For cycle detection

    TypeSymbol(const std::string& name, bool isMutable)
        : Symbol(name, SymbolKind::TYPE), isMutable(isMutable) {}
};
