#pragma once

#include <ostream>

enum class MIROp {

    // No op
    NOP,

    // Constants
    CONST,
    CONST_STRING,
    STRING_EQUAL,
    STRING_CONCAT,
    STRING_LENGTH,
    STRING_BYTE_AT,
    STRING_FROM_BYTE,
    FILE_READ,
    FILE_WRITE,
    FORCE_UNWRAP,

    // Arithmetic
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,

    // Comparison
    CMP_EQ,
    CMP_NE,
    CMP_LT,
    CMP_LE,
    CMP_GT,
    CMP_GE,

    // Bitwise
    AND,
    OR,
    XOR,
    SHL,
    SHR,

    // Unary
    NEG,
    NOT,

    // Memory
    LOAD,
    STORE,
    LOAD_GLOBAL,
    STORE_GLOBAL,
    GLOBAL_ADDR,
    LOAD_INDIRECT,
    STORE_INDIRECT,
    LOAD_INDEX,
    STORE_INDEX,
    LEA,
    ALLOC, // Allocates zero-initialized heap memory with a given size
    ALLOC_SHARED, // Allocates an RC payload; imm is its byte size
    RETAIN, // Adds one strong owner for arg0
    RELEASE, // Removes one strong owner for arg0
    MALLOC_BYTES, // Allocates uninitialized heap memory from a runtime byte count
    FREE, // Releases one raw heap pointer through the C runtime
    ALLOC_DYNAMIC, // Allocates zeroed memory for runtime count * element size

    // Stack / calling convention
    PARAM,
    ARG,
    CALL,
    RET,

    // Control flow
    JMP,
    BRANCH,

    // Function
    LABEL,

    // Runtime calls
    RUNTIME_CALL,

    // For phi. Simple copy instr
    MOVE,
};


inline const char* toString(MIROp op) {

    switch(op) {

        case MIROp::NOP: return "NOP";
        case MIROp::CONST: return "CONST";
        case MIROp::CONST_STRING: return "CONST_STRING";
        case MIROp::STRING_EQUAL: return "STRING_EQUAL";
        case MIROp::STRING_CONCAT: return "STRING_CONCAT";
        case MIROp::STRING_LENGTH: return "STRING_LENGTH";
        case MIROp::STRING_BYTE_AT: return "STRING_BYTE_AT";
        case MIROp::STRING_FROM_BYTE: return "STRING_FROM_BYTE";
        case MIROp::FILE_READ: return "FILE_READ";
        case MIROp::FILE_WRITE: return "FILE_WRITE";
        case MIROp::FORCE_UNWRAP: return "FORCE_UNWRAP";

        case MIROp::ADD: return "ADD";
        case MIROp::SUB: return "SUB";
        case MIROp::MUL: return "MUL";
        case MIROp::DIV: return "DIV";
        case MIROp::MOD: return "MOD";


        case MIROp::CMP_EQ: return "CMP_EQ";
        case MIROp::CMP_NE: return "CMP_NE";
        case MIROp::CMP_LT: return "CMP_LT";
        case MIROp::CMP_LE: return "CMP_LE";
        case MIROp::CMP_GT: return "CMP_GT";
        case MIROp::CMP_GE: return "CMP_GE";


        case MIROp::AND: return "AND";
        case MIROp::OR: return "OR";
        case MIROp::XOR: return "XOR";
        case MIROp::SHL: return "SHL";
        case MIROp::SHR: return "SHR";


        case MIROp::NEG: return "NEG";
        case MIROp::NOT: return "NOT";


        case MIROp::LOAD: return "LOAD";
        case MIROp::STORE: return "STORE";
        case MIROp::LOAD_GLOBAL: return "LOAD_GLOBAL";
        case MIROp::STORE_GLOBAL: return "STORE_GLOBAL";
        case MIROp::GLOBAL_ADDR: return "GLOBAL_ADDR";
        case MIROp::LOAD_INDIRECT: return "LOAD_INDIRECT";
        case MIROp::STORE_INDIRECT: return "STORE_INDIRECT";
        case MIROp::LOAD_INDEX: return "LOAD_INDEX";
        case MIROp::STORE_INDEX: return "STORE_INDEX";
        case MIROp::LEA: return "LEA";
        case MIROp::ALLOC: return "ALLOC";
        case MIROp::ALLOC_SHARED: return "ALLOC_SHARED";
        case MIROp::RETAIN: return "RETAIN";
        case MIROp::RELEASE: return "RELEASE";
        case MIROp::MALLOC_BYTES: return "MALLOC_BYTES";
        case MIROp::FREE: return "FREE";
        case MIROp::ALLOC_DYNAMIC: return "ALLOC_DYNAMIC";


        case MIROp::PARAM: return "PARAM";
        case MIROp::ARG: return "ARG";
        case MIROp::CALL: return "CALL";
        case MIROp::RET: return "RET";


        case MIROp::JMP: return "JMP";
        case MIROp::BRANCH: return "BRANCH";


        case MIROp::LABEL: return "LABEL";


        case MIROp::RUNTIME_CALL:
            return "RUNTIME_CALL";

        case MIROp::MOVE: return "MOVE";
    }

    return "UNKNOWN";
}


inline std::ostream& operator<<(std::ostream& os, MIROp op) {
    return os << toString(op);
}
