#pragma once

#include <variant>
#include <vector>
#include <string>
#include "AstBaseForward.hpp"
#include "VariableStorage.hpp"


enum class Opcode {
    PUSH_UNINITIALISED,
    PUSH_CONST,   // constant slot
    POP,
    DUPLICATE, // Used only for constructors for now...
    SWAP,

    // Arithmetic
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    NEG,

    // Comparison
    EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL,

    // Logical
    LOGICAL_AND,
    LOGICAL_OR,
    LOGICAL_NOT,

    // Bitwise
    BIT_AND,
    BIT_OR,
    BIT_XOR,
    LEFT_SHIFT,
    RIGHT_SHIFT,
    BIT_NOT,

    // Variables
    LOAD_LOCAL,     // slot in stack
    STORE_LOCAL,    // slot in stack
    LOAD_GLOBAL,  
    STORE_GLOBAL,   
    LOAD_UPVALUE,   // slot in function->upvalues
    STORE_UPVALUE,  // slot in function->upvalues
    CAPTURE_LOCAL,
    CAPTURE_UPVALUE,
    CLOSE_UPVALUE,

    // Control flow
    JUMP,         // address
    JUMP_IF_FALSE,// address

    // Functions
    MAKE_FUNCTION,
    MAKE_CLOSURE,
    CALL,         // function argc
    RETURN_VOID,
    RETURN_VALUE,

    // Arrays
    NEW_ARRAY, // size
    GET_INDEX,
    SET_INDEX, 

    // Annonymous records, nominal records, and classes
    MAKE_RECORD, // field count
    INIT_RECORD, // fieldInit funcProto idx
    MAKE_INSTANCE, // aggregate id
    MAKE_INSTANCE_SHARED, // aggregate id
    GET_PROPERTY, // slot
    SET_PROPERTY, // slot

    // Program end
    HALT,

    // Print the top value by popping
    PRINT,
};



constexpr int UNUSED_OPERAND = -1;
struct Instruction {
    Opcode opcode;
    int operand = UNUSED_OPERAND; // -1 means unused. if this is used somehow, that's bad.
};

struct Chunk {
    std::vector<Instruction> code;
    std::vector<ConstValue> constants;
    //std::vector<FunctionProto> functionProtos;

    Chunk() = default;

    Chunk(
        std::vector<Instruction> code,
        std::vector<ConstValue> constants
        //std::vector<FunctionProto> functionProtos
    ) : code(move(code)), constants(move(constants)) {}
};  

struct FunctionProto {
    Chunk chunk; // The code it will run when called.
    
    int requiredParams = 0;
    int totalParams = 0;
    bool isVariadic = false;

    int upValueCnt = 0;
    std::vector<UpvalueInfo> upvalues; // For closure. The actual upvalue objects are created at runtime by CAPTURE opcodes.
    int frameSize = 0; // For stack allocation. Equals to max slot index used + 1.
    
    std::vector<Chunk> defaultValues; // if ith default value is missing, run code in ith slot.
};
