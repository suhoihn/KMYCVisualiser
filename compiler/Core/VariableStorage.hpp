#pragma once

#include "Symbol.hpp"

struct Local {
    VarSymbol* sym = nullptr;
    int slot = INVALID_SLOT;  // Stack slot index. For future, it can also mean virtual reg index.
    int scopeDepth = 0; // This is relative scope depth from current fnCtx.
    bool captured = false;
};

struct UpvalueInfo {
    bool isLocal;
    int index; // Slot index in parent locals OR parent upvalues
    VarSymbol* symbol = nullptr;
    
    bool capturedByChildren = false;
    /*
    Example case
    ------------
    fun outer() {
        let x = 42;
        return fun() {
            print(x);   <- index = <local slot of x in outer()>
        };
    }

    fun outer() {
        let x = 42;
        fun middle() {    <- index = <local slot of x in outer()> + its "capturedByChildren" flag is on
            return fun() {
                print(x); <- index = <upvalue slot of x in middle()>
            };
        }
    }
    */
};

struct ResolvedVar {
    enum Kind {
        LOCAL, UPVALUE, GLOBAL
    } kind;

    int index;
};
