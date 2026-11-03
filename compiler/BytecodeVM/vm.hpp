#pragma once

#include "../Core/value.hpp"
#include <stdexcept>
#include <iosfwd>

inline static BinaryOp opcodeToBinaryOp(Opcode op) {
    switch (op) {

        // Arithmetic
        case Opcode::ADD:           return BinaryOp::Plus;
        case Opcode::SUB:           return BinaryOp::Minus;
        case Opcode::MUL:           return BinaryOp::Star;
        case Opcode::DIV:           return BinaryOp::Slash;
        case Opcode::MOD:           return BinaryOp::Percent;

        // Comparison
        case Opcode::EQUAL:         return BinaryOp::EqualEqual;
        case Opcode::NOT_EQUAL:     return BinaryOp::NotEqual;
        case Opcode::LESS:          return BinaryOp::Less;
        case Opcode::LESS_EQUAL:    return BinaryOp::LessEqual;
        case Opcode::GREATER:       return BinaryOp::Greater;
        case Opcode::GREATER_EQUAL: return BinaryOp::GreaterEqual;

        // Logical
        case Opcode::LOGICAL_AND:   return BinaryOp::LogicalAnd;
        case Opcode::LOGICAL_OR:    return BinaryOp::LogicalOr;

        // Bitwise
        case Opcode::BIT_AND:       return BinaryOp::BitAnd;
        case Opcode::BIT_OR:        return BinaryOp::BitOr;
        case Opcode::BIT_XOR:       return BinaryOp::BitXor;
        case Opcode::LEFT_SHIFT:    return BinaryOp::LShift;
        case Opcode::RIGHT_SHIFT:   return BinaryOp::RShift;

        default:
            throw std::runtime_error("Opcode is not a BinaryOp");
    }
}

inline static UnaryOp opcodeToUnaryOp(Opcode op) {
    switch (op) {

        case Opcode::NEG:           return UnaryOp::Minus;
        case Opcode::LOGICAL_NOT:   return UnaryOp::LogicalNot;
        case Opcode::BIT_NOT:       return UnaryOp::BitNot;

        default:
            throw std::runtime_error("Opcode is not a UnaryOp");
    }
}

        

struct CallFrame {
    Chunk* chunk;           // Function code source.
    size_t ip;              // instruction pointer
    int base;               // stack base for this function
    FunctionPtr function;   // closure
};


class VM {
public:
    void load(std::vector<FunctionProto> functionProtos);
    void run(void);
    void setProgramOutput(std::streambuf* output);

    VM();
private:
    bool running = false;
    std::streambuf* programOutput = nullptr;

    std::vector<FunctionProto> functionProtos;

    std::vector<Value> stack; // Runtime stack (main memory; RAM)
    std::vector<CallFrame> frames; // Current frame running (has its own ip and chunk)
   
    std::vector<UpvaluePtr> openUpvalues;
    std::unordered_map<int, UpvaluePtr> openUpvalueMap; // For quick lookup of open upvalues by their stack location.
    
    void closeUpvalues(int base);
    UpvaluePtr captureUpvalue(int stackSlot);

    // Native functions as values
    std::vector<Value> nativeFunctions;

    // Execution
    Instruction fetchInstr(void);
    void executeInstr(const Instruction& instr);
    void push(Value value);
    Value pop();
};

std::string chunkToString(const Chunk chunk);
