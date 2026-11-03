#include "vm.hpp"

#include <stdexcept>
#include <iostream>
#include "../Core/value.hpp"
#include "../Utils/NativeFunctionImpl.hpp"

VM::VM() {
    // Register all native functions.
    // TODO: Kinda looks ugly.
    int maxSlot = 0;
    for (auto& [name, info] : nativeFnTypes) {
        maxSlot = std::max(maxSlot, info.globalSlot);
    }

    nativeFunctions.resize(maxSlot + 1);

    for (auto& [name, info] : nativeFnTypes) {
        std::cout << "Native binding " << name << " address=" << (void*)(info.fn)
                  << " slot=" << info.globalSlot << '\n';
        nativeFunctions[info.globalSlot] =
            Value(std::make_shared<FunctionObj>(info.fn));
    }
}

// DEBUG
static std::string opcodeToString(Opcode op) {
    switch (op) {
        case Opcode::PUSH_UNINITIALISED: return "PUSH_UNINITIALISED";
        case Opcode::PUSH_CONST: return "PUSH_CONST";
        case Opcode::POP: return "POP";
        case Opcode::DUPLICATE: return "DUPLICATE"; 
        case Opcode::SWAP: return "SWAP"; 

        case Opcode::ADD: return "ADD";
        case Opcode::SUB: return "SUB";
        case Opcode::MUL: return "MUL";
        case Opcode::DIV: return "DIV";
        case Opcode::MOD: return "MOD";
        case Opcode::NEG: return "NEG";

        case Opcode::EQUAL: return "EQUAL";
        case Opcode::NOT_EQUAL: return "NOT_EQUAL";
        case Opcode::LESS: return "LESS";
        case Opcode::LESS_EQUAL: return "LESS_EQUAL";
        case Opcode::GREATER: return "GREATER";
        case Opcode::GREATER_EQUAL: return "GREATER_EQUAL";

        case Opcode::LOGICAL_AND: return "LOGICAL_AND";
        case Opcode::LOGICAL_OR: return "LOGICAL_OR";
        case Opcode::LOGICAL_NOT: return "LOGICAL_NOT";

        case Opcode::BIT_AND: return "BIT_AND";
        case Opcode::BIT_OR: return "BIT_OR";
        case Opcode::BIT_XOR: return "BIT_XOR";
        case Opcode::LEFT_SHIFT: return "LEFT_SHIFT";
        case Opcode::RIGHT_SHIFT: return "RIGHT_SHIFT";
        case Opcode::BIT_NOT: return "BIT_NOT";

        case Opcode::LOAD_LOCAL: return "LOAD_LOCAL";
        case Opcode::LOAD_UPVALUE: return "LOAD_UPVALUE";
        case Opcode::STORE_LOCAL: return "STORE_LOCAL";
        case Opcode::STORE_UPVALUE: return "STORE_UPVALUE";
        case Opcode::LOAD_GLOBAL: return "LOAD_GLOBAL";
        case Opcode::STORE_GLOBAL: return "STORE_GLOBAL";
        case Opcode::CAPTURE_LOCAL: return "CAPTURE_LOCAL";
        case Opcode::CAPTURE_UPVALUE: return "CAPTURE_UPVALUE";
        case Opcode::CLOSE_UPVALUE: return "CLOSE_UPVALUE";
        case Opcode::MAKE_CLOSURE: return "MAKE_CLOSURE";

        case Opcode::JUMP: return "JUMP";
        case Opcode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";

        case Opcode::CALL: return "CALL";
        case Opcode::RETURN_VALUE: return "RETURN_VALUE";
        case Opcode::RETURN_VOID: return "RETURN_VOID";

        case Opcode::NEW_ARRAY: return "NEW_ARRAY";
        case Opcode::GET_INDEX: return "GET_INDEX";
        case Opcode::SET_INDEX: return "SET_INDEX"; 

        case Opcode::MAKE_RECORD: return "MAKE_RECORD";
        case Opcode::GET_PROPERTY: return "GET_PROPERTY";
        case Opcode::SET_PROPERTY: return "SET_PROPERTY";

        case Opcode::HALT: return "HALT";
        case Opcode::PRINT: return "PRINT";

        default: return "UNKNOWN";
    }
}

static std::string constValueToString(const ConstValue& v) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return "null";
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return "\"" + arg + "\"";
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        }
        else {
            return std::to_string(arg);
        }
    }, v);
}

void VM::load(std::vector<FunctionProto> functionProtos) {
    this->functionProtos = std::move(functionProtos);
    stack.clear();
    frames.clear();
    openUpvalues.clear();
    frames.push_back(CallFrame{
        &this->functionProtos.back().chunk, // global chunk
        0, // base
        0, // ip
        nullptr, // no function object for global scope
    });
    frames.back().base = stack.size();
    std::cout << "framesize: " << this->functionProtos.back().frameSize << std::endl;
    stack.resize(stack.size() + this->functionProtos.back().frameSize, Value(GarbageValue{})); // or frameSize
}

void VM::setProgramOutput(std::streambuf* output) {
    programOutput = output;
}

Instruction VM::fetchInstr() {
    auto& frame = frames.back();
    if (frame.ip >= frame.chunk->code.size()) {
        throw std::runtime_error("IP out of bounds (no HALT or bad jump)");
    }
    return frame.chunk->code[frame.ip++];
}

void VM::push(Value value) {
    stack.push_back(value);
}

Value VM::pop() {
    if (stack.empty())
        throw std::runtime_error("Stack underflow");

    Value v = stack.back();
    stack.pop_back();
    return v;
}

// Every expression must leave exactly ONE value on the stack.
// (which is then popped at ExprStmt since its a statement!)
// Every statement must leave NO value on the stack.

/*
| caller locals | caller temps | function | callee locals | callee temps |
                            ^
                         frame.base

*/

void VM::closeUpvalues(int base) {
    // iterate manually (safe for modification)
    for (size_t i = 0; i < openUpvalues.size(); ) {
        UpvaluePtr uv = openUpvalues[i];

        if (uv->stackSlot >= base) {
            // close it
            std::cout << "Closing upvalue at stack slot " << uv->stackSlot << std::endl;
            uv->closed = stack[uv->stackSlot];
            uv->isClosed = true;

            // remove from open list (swap-remove)
            openUpvalues[i] = openUpvalues.back();
            openUpvalues.pop_back();
        } else {
            i++;
        }
    }
}

UpvaluePtr VM::captureUpvalue(int stackSlot) {
    // Reuse existing open upvalue
    for (auto& uv : openUpvalues) {
        if (!uv->isClosed && uv->stackSlot == stackSlot) {
            return uv;
        }
    }

    auto uv = std::make_shared<Upvalue>(
        stackSlot,
        nullptr,
        false
    );

    openUpvalues.push_back(uv);
    openUpvalueMap[stackSlot] = uv;

    return uv;
}


void VM::executeInstr(const Instruction& instr) {
    auto& frame = frames.back();
    switch (instr.opcode) {
        case Opcode::PUSH_UNINITIALISED: {
            for (int i = 0; i < instr.operand; i++) {
                push(Value(GarbageValue{}));
            }
            break;
        }
        case Opcode::PUSH_CONST: {
            push(
                constValToVal(frame.chunk->constants[instr.operand])
            );
            break;
        }

        case Opcode::POP: {
            pop();
            break;
        }

        case Opcode::DUPLICATE: {
            Value v = pop();
            push(v);
            push(v); // No clone. References are copied.
            break;
        }

        case Opcode::SWAP: {
            // [...] [v1] [v2]
            Value v2 = pop();
            Value v1 = pop();

            // [...] [v2] [v1]
            push(v2);
            push(v1);
            break;
        }

        // ----- Binary Operators -----

        case Opcode::ADD: 
        case Opcode::SUB: 
        case Opcode::MUL: 
        case Opcode::DIV:
        case Opcode::MOD: 

        case Opcode::EQUAL: 
        case Opcode::NOT_EQUAL: 
        case Opcode::LESS: 
        case Opcode::LESS_EQUAL:
        case Opcode::GREATER: 
        case Opcode::GREATER_EQUAL:

        case Opcode::LOGICAL_AND:
        case Opcode::LOGICAL_OR: 

        case Opcode::BIT_AND: 
        case Opcode::BIT_OR:
        case Opcode::BIT_XOR: 
        case Opcode::LEFT_SHIFT:
        case Opcode::RIGHT_SHIFT: {
            Value b = pop();
            Value a = pop();
            push(applyBinary(
                opcodeToBinaryOp(instr.opcode), a, b
            ));
            break;
        }

        case Opcode::BIT_NOT:
        case Opcode::LOGICAL_NOT:
        case Opcode::NEG: {
            Value a = pop();
            push(applyUnary(
                opcodeToUnaryOp(instr.opcode), a
            ));
            break;
        }
        
        
        // ----- Variables -----
        case Opcode::LOAD_LOCAL: {
            push(stack[frame.base + instr.operand]);
            break;
        }

        case Opcode::STORE_LOCAL: {
            Value v = pop();
            stack[frame.base + instr.operand] = v.clone();
            push(v); // Assignment produces a value. Including Let stmt.
            break;
        }
        case Opcode::LOAD_UPVALUE: {
            auto up = frame.function->upvalues[instr.operand];
            if (up->isClosed) {
                push(up->closed);
            } else {
                push(stack[up->stackSlot]);
            }
            break;
        }
        
        case Opcode::STORE_UPVALUE: {
            Value v = pop();

            auto up = frame.function->upvalues[instr.operand];
            if (up->isClosed) {
                up->closed = v;
            } else {
                stack[up->stackSlot] = v.clone();
            }

            push(v);
            break;
        }

        case Opcode::LOAD_GLOBAL: {
            push(nativeFunctions[instr.operand]);
            break;
        }
        case Opcode::STORE_GLOBAL: {
            throw std::runtime_error("If this runs, that's a bug. global modification not implemented yet.");
            break;
        }
        
        case Opcode::CAPTURE_LOCAL: 
        case Opcode::CAPTURE_UPVALUE: {
            throw std::runtime_error("Capture_xxx deprecated.");
            break;
        }

        case Opcode::CLOSE_UPVALUE: {
            throw std::runtime_error("Close_upvalue deprecated.");
            break;
        }

        // ----- Control flow  -----
        case Opcode::JUMP:
            frame.ip = instr.operand;
            break;

        case Opcode::JUMP_IF_FALSE: {
            Value v = pop();
            if (!isTruthy(v)) {
                frame.ip = instr.operand;
            }
            break;
        }

        // ----- Functions -----
        case Opcode::MAKE_FUNCTION: {
            auto& fnProto = this->functionProtos[instr.operand];

            // This is guarenteed to have no upvalues.
            auto function = std::make_shared<FunctionObj>(
                &fnProto,
                std::vector<UpvaluePtr>{}
            );

            push(Value(function));
            break;
        }

        case Opcode::MAKE_CLOSURE: {
            auto& fnProto = this->functionProtos[instr.operand];

            auto upvalues = std::vector<UpvaluePtr>(fnProto.upValueCnt);

            for (int i = 0; i < fnProto.upValueCnt; i++) {
                auto& up = fnProto.upvalues[i];
                int slot = up.index;
                if (up.isLocal) {
                    std::cout << "frame.base: " << frame.base << "\n";
                    std::cout << "slot " << slot << "\n";

                    // From immediate parent,
                    // take x directly from parent stack frame
                    // Value* ptr = &stack[frame.base + slot];

                    // ensure it's in openUpvalues and get the UpvaluePtr
                    UpvaluePtr uv = captureUpvalue(frame.base + slot);

                    upvalues[i] = uv;
                } else {
                    // take variable from my parent closure’s upvalue list
            
                    // parent closure is the current function calling this opcode.
                    UpvaluePtr parentUV = frame.function->upvalues[slot];

                    // reuse SAME upvalue object.
                    upvalues[i] = parentUV;
                }
            }

            auto closure = std::make_shared<FunctionObj>(
                &fnProto,
                std::move(upvalues)
            );

            // allocate upvalue slots
            std::cout << "Upvalue count: " << fnProto.upValueCnt << std::endl;
            for (int i = 0; i < fnProto.upValueCnt; i++) {
                std::cout << "Upvalue " << i << ": " << (closure->upvalues[i]->isClosed ? "closed" : "open") << std::endl;
                std::cout << "  Stack Slot: " << closure->upvalues[i]->stackSlot << std::endl;
            }

            push(Value(closure));
            break;
        }

        case Opcode::CALL: {
            // Stack
            // [ ...other stuff ] [ callee function object ] [ arg0 ] ... [ argN ] [ local(N+1) ]
            //                                                  ^^ frame.base

            // 1. Get function from stack
            Value calleeVal = stack[stack.size() - instr.operand - 1];

            if (!std::holds_alternative<ObjectPtr>(calleeVal.data)) {
                throw std::runtime_error("Attempt to call non-function");
            }

            auto obj = std::get<ObjectPtr>(calleeVal.data);

            if (obj->kind != ObjKind::Function) {
                throw std::runtime_error("Can only call functions");
            }

            auto fn = std::static_pointer_cast<FunctionObj>(obj);

            if (fn->kind == FunctionKind::User) {

                // 2. Create new call frame
                CallFrame newFrame;
                newFrame.chunk = &(fn->proto->chunk);
                newFrame.ip = 0;
                newFrame.base = stack.size() - instr.operand; // arguments are already on stack
                newFrame.function = fn;
                
                std::cout << "New frame size: " << fn->proto->frameSize << std::endl;
                std::cout << "New frame base: " << stack.size() - instr.operand << std::endl;
                stack.resize(newFrame.base + fn->proto->frameSize, Value(GarbageValue{})); // or frameSize

                std::cout << "Entering function chunk " << newFrame.chunk << '\n';

                //chunkToString(fn->proto->chunk);
                frames.push_back(newFrame);
            } else if (fn->kind == FunctionKind::Native) {
                // Native call. No virtual(?) frame created.
                Value* args = new Value[instr.operand];
                
                std::cout << "Native call argument count: " << instr.operand << '\n';
                std::cout << "Stack size before argument pop: " << stack.size() << '\n';
                for (int i = 0; i < instr.operand; i++) {
                    std::cout << "stksize: " << stack.size() << " / i: " << i << "\n";
                    args[i] = pop();
                }
                std::cout << "Stack size after argument pop: " << stack.size() << '\n';

                std::cout << "Native arguments:\n";
                for (int i = 0; i < instr.operand; i++) {
                    std::cout << args[i].toString() << "\n";
                }
                std::cout << "Calling native function at " << (void*)(fn->nativeFn) << '\n';

                // Clean func & args
                //stack.resize(stack.size() - instr.operand - 1);
                //std::cout << "resize ok\n";
                pop();
                
                push(fn->nativeFn(instr.operand, args));
                std::cout << "Native result pushed\n";

                delete[] args;
                std::cout << "Native argument buffer released\n";
            }
            std::cout << "CALL complete\n";
            break;
        }

        case Opcode::RETURN_VOID: {
            closeUpvalues(frame.base);
            stack.resize(frame.base - 1, Value(GarbageValue{}));
            frames.pop_back();
            push(Value(GarbageValue{})); // return void produces null value? (pop will be the next instr.)
            if (frames.empty()) {
                running = false; // main function returned, stop execution
            }
            break;
        }  

        case Opcode::RETURN_VALUE: {
            closeUpvalues(frame.base);
            Value retVal = pop();
            stack.resize(frame.base - 1, Value(GarbageValue{}));
            frames.pop_back();
            push(retVal);
            if (frames.empty()) {
                running = false; // main function returned, stop execution
            }
            break;
        }

        // ----- Arrays -----
        
        case Opcode::NEW_ARRAY: {
            auto arr = std::make_shared<ArrayObj>();

            for (int i = 0; i < instr.operand; ++i) {
                arr->array.push_back(pop());    
            }

            push(Value(arr));
            break;
        }

        case Opcode::GET_INDEX: {
            Value idx = pop();
            Value arr = pop();

            if (!checkObjType(arr, ObjKind::Array)) {
                throw std::runtime_error("Illegal Operation: Indexing non-array.");
            }
            if (!isInt(idx)) {
                throw std::runtime_error("Illegal Operation: Indexing with non-int.");
            }

            auto realArr = getArray(arr);
            int intIdx = std::get<int>(idx.data);

            if (intIdx < 0 || intIdx >= realArr->array.size()) {
                throw std::runtime_error("Index out of bounds");
            }

            //std::cout << arr.toString() << std::endl;
            //std::cout << idx.toString() << std::endl;
        
            push( realArr->array[intIdx] );

            break;
        }

        case Opcode::SET_INDEX: {
            Value idx = pop();
            Value arr = pop();
            Value val = pop();

            if (!checkObjType(arr, ObjKind::Array)) {
                throw std::runtime_error("Illegal Operation: Indexing non-array.");
            }
            if (!isInt(idx)) {
                throw std::runtime_error("Illegal Operation: Indexing with non-int.");
            }

            auto realArr = getArray(arr);
            int intIdx = std::get<int>(idx.data);

            if (intIdx < 0 || intIdx >= realArr->array.size()) {
                throw std::runtime_error("Index out of bounds");
            }

            realArr->array[intIdx] = val.clone();

            push(val);

            break;
        }

        // ----- Aggregates -----
        case Opcode::MAKE_RECORD: {
            auto rec = std::make_shared<Record>();

            for (int i = 0; i < instr.operand; ++i) {
                rec->fields.push_back(pop());
            }

            push(Value(rec));
            break;
        }

        case Opcode::MAKE_INSTANCE: {

        }

        case Opcode::GET_PROPERTY: {
            Value rec = pop();

            if (!std::holds_alternative<RecordPtr>(rec.data)) {
                throw std::runtime_error("Serious Illegal Operation: Getting property of non-record.");
            }

            auto recPtr = std::get<RecordPtr>(rec.data);
            push(recPtr->fields[instr.operand]);
            break;
        }

        case Opcode::SET_PROPERTY: {
            Value rec = pop();
            Value val = pop();

            if (!std::holds_alternative<RecordPtr>(rec.data)) {
                throw std::runtime_error("Serious Illegal Operation: Setting property of non-record.");
            }

            auto recPtr = std::get<RecordPtr>(rec.data);
            recPtr->fields[instr.operand] = val;
            push(val);
            break;
        }

        case Opcode::INIT_RECORD: {
            
        }

        // ----- IO -----

        case Opcode::PRINT: {
            Value v = pop();
            // Compiler diagnostics may be captured for -d, but user-visible
            // print output must always reach the original stdout stream.
            std::ostream out(programOutput ? programOutput : std::cout.rdbuf());
            out << "\033[31m" << v.toString() << "\033[0m" << std::endl;
            break;
        }

        case Opcode::HALT: {
            running = false;
            //ip = code.size(); // stop execution
            break;
        }

        default:
            throw std::runtime_error("Unknown opcode");
    }
}

void VM::run() {
    running = true;
    while (running) {
        //std::cout << "STACK SIZE: " << stack.size() << "\n";
        for(auto& v : stack) {
            std::cout << v.toString() << " | ";
        }
        std::cout << std::endl;

        // std::cout << "IP: " << frames.back().ip <<  std::endl;
        Instruction instr = fetchInstr();
        std::cout << "Executing: " << opcodeToString(instr.opcode);

        if (instr.operand != INVALID_SLOT)
            std::cout << " " << instr.operand;

        std::cout << std::endl;
        executeInstr(instr);
    }
}


std::string chunkToString(const Chunk chunk) {
    std::string out;

    out += "=== CONSTANTS ===\n";
    for (size_t i = 0; i < chunk.constants.size(); i++) {
        out += std::to_string(i) + ": " + constValueToString(chunk.constants[i]) + "\n";
    }

    out += "\n=== CODE ===\n";

    for (size_t i = 0; i < chunk.code.size(); i++) {
        const Instruction& ins = chunk.code[i];

        out += std::to_string(i) + "  ";
        out += opcodeToString(ins.opcode);

        // Only print operand if meaningful
        if (ins.opcode == Opcode::PUSH_CONST ||
            ins.opcode == Opcode::PUSH_UNINITIALISED || 
            ins.opcode == Opcode::LOAD_LOCAL ||
            ins.opcode == Opcode::LOAD_UPVALUE ||
            ins.opcode == Opcode::LOAD_GLOBAL ||
            ins.opcode == Opcode::STORE_LOCAL ||
            ins.opcode == Opcode::STORE_UPVALUE ||
            ins.opcode == Opcode::STORE_GLOBAL ||
            ins.opcode == Opcode::CAPTURE_LOCAL ||
            ins.opcode == Opcode::CAPTURE_UPVALUE ||
            ins.opcode == Opcode::CLOSE_UPVALUE ||
            ins.opcode == Opcode::MAKE_CLOSURE ||
            ins.opcode == Opcode::JUMP ||
            ins.opcode == Opcode::JUMP_IF_FALSE ||
            ins.opcode == Opcode::CALL ||
            ins.opcode == Opcode::NEW_ARRAY ||
            ins.opcode == Opcode::MAKE_RECORD ||
            ins.opcode == Opcode::GET_PROPERTY ||
            ins.opcode == Opcode::SET_PROPERTY
        ) {

            out += " " + std::to_string(ins.operand);
        }

        out += "\n";
    }

    return out;
}
