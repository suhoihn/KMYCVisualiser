#pragma once

#include <ostream>

enum class IROp {
    GARBAGE,
    CONST_INT,
    CONST_STRING,
    CONST_NULL,
    STRING_EQUAL,
    STRING_CONCAT,
    STRING_LENGTH,
    STRING_BYTE_AT,
    STRING_FROM_BYTE,
    FILE_READ,
    FILE_WRITE,
    FORCE_UNWRAP,

    ADD,
    SUB,
    MUL,
    DIV,
    MOD,

    EQUAL,
    NOT_EQUAL,

    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL,

    LOGICAL_AND,
    LOGICAL_OR,

    BIT_AND,
    BIT_OR,
    BIT_XOR,

    LEFT_SHIFT,
    RIGHT_SHIFT,

    // Unary
    NEG,
    BIT_NOT,
    LOGICAL_NOT,

    PRINT,

    PHI, // Special since it takes
    BIND, // Binding varref to one

    FUNC_LABEL, // Takes args (which are stored as upvalues) and imm (function id)
    PARAM, // Takes imm which is param_id (1-1 match from CALL's args)
    UPVALUE, // Takes imm which is upvalue_idx for the current closure (1-1 match from FUNC_LABEL's captured vars).
    
    INTRODUCE_ENV, // Introduces the env pointer for the current function. (e.g., v0 = INTRODUCE_ENV)
    ALLOC_ENV, // Takes imm which is the size. (e.g., v0 = ALLOC_ENV 3)
    SET_ENV, // Takes a value of environment and an index and a value. (e.g., SET_ENV v0 1 v1)
    GET_ENV, // Gets a value from environment in a given index (e.g., v1 = GET_ENV v0 1)
    CALL,

    GET_ADDR, // Gets the address of a local/field/index storage location
    GET_GLOBAL_ADDR, // Gets the address of program-global slot imm
    LOAD_INDIRECT, // Load one machine word through a pointer value
    STORE_INDIRECT, // Store one machine word through a pointer value
    ALLOC_HEAP, // Allocate imm bytes and return a pointer
    ALLOC_SHARED, // Allocate an imm-byte RC payload; destructor is null for now
    RETAIN, // Add one strong owner for the shared payload in arg0
    RELEASE, // Remove one strong owner for the shared payload in arg0
    MALLOC_BYTES, // Allocate a runtime byte count without zero-initialization
    FREE, // Release one raw heap pointer; has no result
    ALLOC_ARRAY, // Allocate and zero imm bytes for a fixed-size array
    ALLOC_ARRAY_DYNAMIC, // Allocate zeroed elementSize * count bytes
    LOAD_FIELD, // Load a pointer-sized aggregate field at byte offset imm
    STORE_FIELD, // Store a pointer-sized aggregate field at byte offset imm
    STORE_ARRAY, // Store an array element at byte offset imm
    LOAD_ARRAY_INDEX, // Load array[index], using imm as element size
    STORE_ARRAY_INDEX, // Store array[index], using imm as element size
    ALLOC_CELL_INIT, // Allocates a cell with a given value (e.g., v0 = ALLOC_CELL_INIT v1)
    STORE_CELL, // Stores a value to a cell (e.g., STORE_CELL v0 v1)
    LOAD_CELL, // Loads a value from a cell (e.g., v1 = LOAD_CELL v0)
    LOAD_GLOBAL, // Loads program-global slot imm into dst
    STORE_GLOBAL, // Stores arg0 into program-global slot imm
    RETURN
};

inline const char* toString(IROp op) {
    switch (op) {
        case IROp::CONST_INT:           return "CONST_INT";
        case IROp::CONST_STRING:        return "CONST_STRING";
        case IROp::CONST_NULL:          return "CONST_NULL";
        case IROp::STRING_EQUAL:        return "STRING_EQUAL";
        case IROp::STRING_CONCAT:      return "STRING_CONCAT";
        case IROp::STRING_LENGTH:      return "STRING_LENGTH";
        case IROp::STRING_BYTE_AT:     return "STRING_BYTE_AT";
        case IROp::STRING_FROM_BYTE:   return "STRING_FROM_BYTE";
        case IROp::FILE_READ:          return "FILE_READ";
        case IROp::FILE_WRITE:         return "FILE_WRITE";
        case IROp::FORCE_UNWRAP:       return "FORCE_UNWRAP";

        case IROp::ADD:                 return "ADD";
        case IROp::SUB:                 return "SUB";
        case IROp::MUL:                 return "MUL";
        case IROp::DIV:                 return "DIV";
        case IROp::MOD:                 return "MOD";

        case IROp::EQUAL:               return "EQUAL";
        case IROp::NOT_EQUAL:           return "NOT_EQUAL";

        case IROp::LESS:                return "LESS";
        case IROp::LESS_EQUAL:          return "LESS_EQUAL";
        case IROp::GREATER:             return "GREATER";
        case IROp::GREATER_EQUAL:       return "GREATER_EQUAL";

        case IROp::LOGICAL_AND:         return "LOGICAL_AND";
        case IROp::LOGICAL_OR:          return "LOGICAL_OR";

        case IROp::BIT_AND:             return "BIT_AND";
        case IROp::BIT_OR:              return "BIT_OR";
        case IROp::BIT_XOR:             return "BIT_XOR";

        case IROp::LEFT_SHIFT:          return "LEFT_SHIFT";
        case IROp::RIGHT_SHIFT:         return "RIGHT_SHIFT";

        case IROp::PHI:                 return "PHI";
        case IROp::BIND:                return "BIND";

        case IROp::PRINT:               return "PRINT";

        case IROp::FUNC_LABEL:          return "FUNC_LABEL";
        case IROp::PARAM:               return "PARAM";
        case IROp::UPVALUE:             return "UPVALUE";
        case IROp::INTRODUCE_ENV:       return "INTRODUCE_ENV";
        case IROp::ALLOC_ENV:           return "ALLOC_ENV";
        case IROp::SET_ENV:             return "SET_ENV";
        case IROp::GET_ENV:             return "GET_ENV";
        case IROp::CALL:                return "CALL";

        case IROp::GET_ADDR:            return "GET_ADDR";
        case IROp::GET_GLOBAL_ADDR:     return "GET_GLOBAL_ADDR";
        case IROp::LOAD_INDIRECT:       return "LOAD_INDIRECT";
        case IROp::STORE_INDIRECT:      return "STORE_INDIRECT";
        case IROp::ALLOC_HEAP:          return "ALLOC_HEAP";
        case IROp::ALLOC_SHARED:        return "ALLOC_SHARED";
        case IROp::RETAIN:              return "RETAIN";
        case IROp::RELEASE:             return "RELEASE";
        case IROp::MALLOC_BYTES:        return "MALLOC_BYTES";
        case IROp::FREE:                return "FREE";
        case IROp::ALLOC_ARRAY:         return "ALLOC_ARRAY";
        case IROp::ALLOC_ARRAY_DYNAMIC: return "ALLOC_ARRAY_DYNAMIC";
        case IROp::LOAD_FIELD:          return "LOAD_FIELD";
        case IROp::STORE_FIELD:         return "STORE_FIELD";
        case IROp::STORE_ARRAY:         return "STORE_ARRAY";
        case IROp::LOAD_ARRAY_INDEX:    return "LOAD_ARRAY_INDEX";
        case IROp::STORE_ARRAY_INDEX:   return "STORE_ARRAY_INDEX";
        case IROp::ALLOC_CELL_INIT:     return "ALLOC_CELL_INIT";
        case IROp::STORE_CELL:          return "STORE_CELL";
        case IROp::LOAD_CELL:           return "LOAD_CELL";
        case IROp::LOAD_GLOBAL:         return "LOAD_GLOBAL";
        case IROp::STORE_GLOBAL:        return "STORE_GLOBAL";
        case IROp::RETURN:              return "RETURN";
    }

    return "UNKNOWN";
}

inline std::ostream& operator<<(std::ostream& os, IROp op) {
    return os << toString(op);
}
