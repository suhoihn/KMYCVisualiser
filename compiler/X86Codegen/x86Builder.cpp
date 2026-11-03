#include "x86Builder.hpp"

#include <sstream>
#include <iostream>
#include <iomanip>

#include "../Core/errorhandler.hpp"
X86Builder::X86Builder(
    std::vector<MIRFunction*> mirFunctions, 
    std::ostream& out,
    const StringPool& stringPool,
    int globalSlotCount,
    X86Target target
) : out(out), mirFunctions(mirFunctions), stringPool(stringPool), globalSlotCount(globalSlotCount),
    target(target), argRegs(target == X86Target::Linux
        ? std::vector<std::string>{"rdi", "rsi", "rdx", "rcx", "r8", "r9"}
        : std::vector<std::string>{"rcx", "rdx", "r8", "r9"}) {}

int X86Builder::callStackBytes(size_t argumentCount) const {
    const int shadow = target == X86Target::Windows ? 32 : 0;
    const int stackArgs = argumentCount > argRegs.size()
        ? static_cast<int>(argumentCount - argRegs.size()) * 8 : 0;
    return (shadow + stackArgs + 15) & ~15; // Keep RSP 16-byte aligned before CALL.
}

void X86Builder::emitRuntimeCall(const std::string& symbol) {
    // Fixed small runtime calls use registers; only Windows needs shadow space.
    const int bytes = callStackBytes(0);
    if (bytes) emit("sub rsp, " + std::to_string(bytes));
    emit("call " + symbol);
    if (bytes) emit("add rsp, " + std::to_string(bytes));
}

// MIR keeps runtime intrinsics as compact numeric IDs, but emitted assembly
// calls descriptive C symbols. Future RC MIR operations can likewise call
// kmy_rc_alloc, kmy_rc_retain, and kmy_rc_release directly by symbol name.
static const char *runtimeSymbol(int64_t id) {
    switch (id) {
        case 0: return "kmy_print_int";
        case 1: return "kmy_print_string";
        case 2: return "kmy_string_equal";
        case 3: return "kmy_string_concat";
        case 4: return "kmy_string_length";
        case 5: return "kmy_string_byte_at";
        case 6: return "kmy_read_file";
        case 7: return "kmy_write_file";
        case 8: return "kmy_string_from_byte";
        default:
            throw KMYCompileError(
                "Unknown native runtime function ID " + std::to_string(id)
            );
    }
}

static int mirValToStackPos(int id) {
    return (id + 1) * 8;
};

static std::string loc(const IRValue& v) {
    return "[rbp-" + std::to_string(mirValToStackPos(v.id)) + "]";
}

static std::string escapeAsmString(const std::string& value);

void X86Builder::emitIndent() {
    out << std::string(indent * 4, ' ');
}

std::string X86Builder::makeBlockLabel(int blockId) {
    return "block" + std::to_string(blockId) + "_f" + std::to_string(currFunc->functionId);
}

void X86Builder::emit(std::string s) {
    emitIndent();
    out << s << "\n";
}

void X86Builder::lowerMIRInstr(const MIRInstr& instr) {

    switch(instr.op) {

        case MIROp::NOP:
            emit("nop");
            break;


        // =========================
        // Constants
        // =========================

        case MIROp::CONST:
            emit("mov rax, " + std::to_string(instr.imm.value()));
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;

        case MIROp::CONST_STRING: {
            const int stringId = instr.imm.value();
            // RIP-relative displacement resolves to the static string address.
            emit("lea rax, [rip + kmy_str_" + std::to_string(stringId) + "]");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::STRING_EQUAL:
        case MIROp::STRING_CONCAT:
            // Lowered to RUNTIME_CALL by MIRBuilder.
            throw KMYCompileError("String operation reached x86 directly instead of runtime lowering");


        // =========================
        // Arithmetic
        // =========================

        case MIROp::ADD:
            emit("mov rax, " + loc(instr.args[0]));
            emit("add rax, " + loc(instr.args[1]));
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;


        case MIROp::SUB:
            emit("mov rax, " + loc(instr.args[0]));
            emit("sub rax, " + loc(instr.args[1]));
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;


        case MIROp::MUL:
            emit("mov rax, " + loc(instr.args[0]));
            emit("imul rax, " + loc(instr.args[1]));
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;


        case MIROp::DIV:
            emit("mov rax, " + loc(instr.args[0]));
            emit("cqo");
            emit("idiv qword ptr " + loc(instr.args[1]));
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;


        case MIROp::MOD:
            emit("mov rax, " + loc(instr.args[0]));
            emit("cqo");
            emit("idiv qword ptr " + loc(instr.args[1]));
            emit("mov " + loc(instr.dst.value()) + ", rdx");
            break;


        // =========================
        // Comparison
        // =========================

        case MIROp::CMP_EQ:
        case MIROp::CMP_NE:
        case MIROp::CMP_LT:
        case MIROp::CMP_LE:
        case MIROp::CMP_GT:
        case MIROp::CMP_GE:
        {
            emit("mov rax, " + loc(instr.args[0]));
            emit("cmp rax, " + loc(instr.args[1]));

            const char* set;

            switch(instr.op) {
                case MIROp::CMP_EQ: set="sete"; break;
                case MIROp::CMP_NE: set="setne"; break;
                case MIROp::CMP_LT: set="setl"; break;
                case MIROp::CMP_LE: set="setle"; break;
                case MIROp::CMP_GT: set="setg"; break;
                case MIROp::CMP_GE: set="setge"; break;
                default: set="";
            }

            emit(std::string(set) + " al");
            emit("movzx rax, al");
            emit("mov " + loc(instr.dst.value()) + ", rax");

            break;
        }


        // =========================
        // Bitwise
        // =========================

        case MIROp::AND:
        case MIROp::OR:
        case MIROp::XOR:
        {
            emit("mov rax, " + loc(instr.args[0]));

            if(instr.op == MIROp::AND)
                emit("and rax, " + loc(instr.args[1]));

            else if(instr.op == MIROp::OR)
                emit("or rax, " + loc(instr.args[1]));

            else
                emit("xor rax, " + loc(instr.args[1]));

            emit("mov " + loc(instr.dst.value()) + ", rax");

            break;
        }


        case MIROp::SHL:
            emit("mov rax, " + loc(instr.args[0]));
            emit("mov rcx, " + loc(instr.args[1]));
            emit("shl rax, cl");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;


        case MIROp::SHR:
            emit("mov rax, " + loc(instr.args[0]));
            emit("mov rcx, " + loc(instr.args[1]));
            emit("shr rax, cl");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;


        // =========================
        // Unary
        // =========================

        case MIROp::NEG:
            emit("mov rax, " + loc(instr.args[0]));
            emit("neg rax");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;


        case MIROp::NOT:
            emit("mov rax, " + loc(instr.args[0]));
            emit("not rax");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;


        // =========================
        // Memory
        // =========================

        case MIROp::LOAD: {
            emit("mov rax, " + loc(instr.args[0]));

            int offset = instr.imm.value();

            if (offset == 0)
                emit("mov rax, [rax]");
            else
                emit("mov rax, [rax+" + std::to_string(offset) + "]");

            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }


       case MIROp::STORE: {
            emit("mov rax, " + loc(instr.args[1]));
            emit("mov rcx, " + loc(instr.args[0]));

            int offset = instr.imm.value();

            if (offset == 0)
                emit("mov [rcx], rax");
            else
                emit("mov [rcx+" + std::to_string(offset) + "], rax");

            break;
        }


        case MIROp::LEA: {
            if (instr.args.empty()) {
                throw KMYCompileError("LEA requires an address operand");
            }

            if (instr.args.size() == 1 && !instr.imm.has_value()) {
                // Compute the address of a local stack slot without loading
                // the value stored in that slot.
                emit("lea rax, " + loc(instr.args[0]));
            } else if (instr.args.size() == 1) {
                // Load the base object pointer before applying a field byte
                // offset to form the field address.
                emit("mov rax, " + loc(instr.args[0]));
                emit("lea rax, [rax+" + std::to_string(instr.imm.value()) + "]");
            } else if (instr.args.size() == 2) {
                // Load the array base pointer used by an indexed lvalue.
                emit("mov rax, " + loc(instr.args[0]));
                // Load the element index used by an indexed lvalue.
                emit("mov rcx, " + loc(instr.args[1]));
                // Scale the index by the element size in bytes.
                emit("imul rcx, " + std::to_string(instr.imm.value()));
                // Add the scaled index to the base address.
                emit("add rax, rcx");
            } else {
                throw KMYCompileError("LEA received an unsupported operand shape");
            }

            // Spill the computed address into the destination MIR slot.
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::LOAD_GLOBAL: {
            const int slot = instr.imm.value();
            emit("mov rax, [rip + kmy_globals + " + std::to_string(slot * 8) + "]");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::STORE_GLOBAL: {
            const int slot = instr.imm.value();
            emit("mov rax, " + loc(instr.args[0]));
            emit("mov [rip + kmy_globals + " + std::to_string(slot * 8) + "], rax");
            break;
        }

        case MIROp::GLOBAL_ADDR: {
            const int slot = instr.imm.value();
            // RIP-relative LEA computes the address of the global slot itself;
            // unlike LOAD_GLOBAL, it does not read the value stored there.
            emit("lea rax, [rip + kmy_globals + " + std::to_string(slot * 8) + "]");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::LOAD_INDIRECT: {
            // Load the pointer value from its MIR stack slot.
            emit("mov rcx, " + loc(instr.args[0]));
            // Read one machine-word value through that pointer.
            emit("mov rax, [rcx]");
            // Store the loaded value in the destination MIR slot.
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::STORE_INDIRECT: {
            // Load the destination pointer from its MIR stack slot.
            emit("mov rcx, " + loc(instr.args[0]));
            // Load the value that should be written through the pointer.
            emit("mov rax, " + loc(instr.args[1]));
            // Store the value at the pointed-to address.
            emit("mov [rcx], rax");
            break;
        }


        // =========================
        // Heap
        // =========================

        case MIROp::ALLOC: {
            // calloc(1, size) gives deterministic zeroed storage.
            emit("mov " + argRegs[0] + ", 1");
            emit("mov " + argRegs[1] + ", " + std::to_string(instr.imm.value()));
            emitRuntimeCall("calloc");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::ALLOC_SHARED: {
            // First target-ABI argument: payload size in bytes.
            emit("mov " + argRegs[0] + ", " + std::to_string(instr.imm.value()));

            // Second target-ABI argument: destructor.
            // XORing a register with itself produces zero, so this passes a
            // null destructor until generated class destructors are wired in.
            emit("xor " + argRegs[1] + ", " + argRegs[1]);

            emitRuntimeCall("kmy_rc_alloc");

            // kmy_rc_alloc returns the public payload pointer in RAX; keep the
            // hidden reference-count header immediately before that pointer.
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::RETAIN: {
            // Pass the nullable shared payload in the first target argument register.
            emit("mov " + argRegs[0] + ", " + loc(instr.args[0]));
            emitRuntimeCall("kmy_rc_retain");
            break;
        }

        case MIROp::RELEASE: {
            // Pass the nullable shared payload in the first target argument register.
            emit("mov " + argRegs[0] + ", " + loc(instr.args[0]));

            // kmy_rc_release may invoke the destructor and free the hidden
            // allocation when this operation removes the final strong owner.
            emitRuntimeCall("kmy_rc_release");
            break;
        }


        // =========================
        // Calls
        // =========================

        case MIROp::PARAM: {
            int i = instr.imm.value();
            if (i < 0) throw KMYCompileError("Negative native parameter index");
            if (static_cast<size_t>(i) < argRegs.size()) {
                emit("mov " + loc(instr.dst.value()) + ", " + argRegs[i]);
            } else {
                // RBP+16 skips saved RBP and return address; Windows also skips shadow space.
                const int offset = 16 + (target == X86Target::Windows ? 32 : 0)
                    + (i - static_cast<int>(argRegs.size())) * 8;
                emit("mov rax, [rbp+" + std::to_string(offset) + "]");
                emit("mov " + loc(instr.dst.value()) + ", rax");
            }
            break;
        }


        case MIROp::ARG:
            emit("# ARG handled by CALL");
            break;


        case MIROp::CALL: {
            // Call gets a closure pointer as an argument.
            emit("mov rax, " + loc(instr.args[0]));

            // Physical argument count includes the hidden environment, replacing closure arg0.
            const int bytes = callStackBytes(instr.args.size());
            if (bytes) emit("sub rsp, " + std::to_string(bytes));

            // First argument is always the env pointer
            emit("mov " + argRegs[0] + ", [rax+8]"); // Offset 8 is the env ptr from closure* 
            for (int i = 1; i < instr.args.size(); ++i) {
                if (static_cast<size_t>(i) < argRegs.size()) {
                    emit("mov " + argRegs[i] + ", " + loc(instr.args[i]));
                } else {
                    // Place overflow arguments above Windows shadow space (none on Linux).
                    const int offset = (target == X86Target::Windows ? 32 : 0)
                        + (i - static_cast<int>(argRegs.size())) * 8;
                    emit("mov r10, " + loc(instr.args[i]));
                    emit("mov [rsp+" + std::to_string(offset) + "], r10");
                }
            }
            // Code pointer is at offset zero; R11 is volatile on both ABIs.
            emit("mov r11, [rax]");
            emit("call r11");
            if (bytes) emit("add rsp, " + std::to_string(bytes));

            // Return value.
            if (instr.dst.has_value()) {
                emit("mov " + loc(*instr.dst) + ", rax");
            }
            break;
        }

        case MIROp::LABEL: {
            // imm: function id

            MIRFunction* f = nullptr;
            for (const auto& mirFunc : mirFunctions)  {
                if (mirFunc->functionId == instr.imm.value()) {
                    f = mirFunc;
                    break;
                }
            }
            if (!f) { throw KMYCompileError("???"); }

            MIRBlock* entryBlock = f->entry;

            auto oldFuncId = currFunc->functionId;
            currFunc->functionId = f->functionId;

            emit("lea rax, [rip + " + makeBlockLabel(entryBlock->id) + "]");
            emit("mov " + loc(instr.dst.value()) + ", rax");

            currFunc->functionId = oldFuncId;
            break;
        }

        case MIROp::RUNTIME_CALL: {
            if (instr.args.size() > argRegs.size()) {
                throw KMYCompileError("Too many native runtime arguments");
            }
            for (int i = 0; i < instr.args.size(); ++i) {
                emit("mov " + argRegs[i] + ", " + loc(instr.args[i]));
            }

            // The linker resolves this descriptive symbol from runtime.o.
            // KMY cannot call arbitrary C names yet because it has no general
            // FFI declaration syntax; compiler-known intrinsics can call them.
            emitRuntimeCall(runtimeSymbol(instr.imm.value()));

            if (instr.dst.has_value()) {
                emit("mov " + loc(instr.dst.value()) + ", rax");
            }

            break;
        }

        case MIROp::MOVE: {
            emit("mov rax, " + loc(instr.args[0]));
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::MALLOC_BYTES: {
            // The byte count is evaluated by KMY and passed in the first
            // target argument register. Unlike ALLOC, this calls malloc:
            // the returned bytes have no initialized value.
            emit("mov " + argRegs[0] + ", " + loc(instr.args[0]));
            emitRuntimeCall("malloc");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::FREE: {
            // Pass the raw pointer in the target's first argument register.
            // C free(NULL) is defined as a no-op, so no explicit null branch.
            emit("mov " + argRegs[0] + ", " + loc(instr.args[0]));
            emitRuntimeCall("free");
            break;
        }

        case MIROp::FORCE_UNWRAP: {
            // `expr!!` is an unchecked programmer assertion. Its semantic
            // type changes from T? to T, but x86 performs only a copy: a null
            // value is allowed to continue and may fail later if dereferenced.
            emit("mov rax, " + loc(instr.args[0]));
            // To enable explicit trapping instead, uncomment this block:
            // emit("cmp rax, 0");
            // emit("jne 1f");
            // emit("ud2");
            // emit("1:");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::ALLOC_DYNAMIC: {
            // calloc(1, count * element_size) for runtime-sized arrays.
            emit("mov rax, " + loc(instr.args[0]));
            emit("imul rax, " + std::to_string(instr.imm.value()));
            emit("mov " + argRegs[0] + ", 1");
            emit("mov " + argRegs[1] + ", rax");
            emitRuntimeCall("calloc");
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::LOAD_INDEX: {
            // Load array base pointer and dynamic element index.
            emit("mov rax, " + loc(instr.args[0]));
            emit("mov rcx, " + loc(instr.args[1]));

            // Read base[index * elementSize]. Bounds checks are intentionally
            // deferred; the static array type supplies the element stride.
            emit("mov rax, [rax+rcx*" + std::to_string(instr.imm.value()) + "]");

            // Spill the loaded element into the MIR destination slot.
            emit("mov " + loc(instr.dst.value()) + ", rax");
            break;
        }

        case MIROp::STORE_INDEX: {
            // Load the value, dynamic index, and array base into scratch regs.
            emit("mov rax, " + loc(instr.args[2]));
            emit("mov rcx, " + loc(instr.args[1]));
            emit("mov rdx, " + loc(instr.args[0]));

            // Write value into base[index * elementSize].
            emit("mov [rdx+rcx*" + std::to_string(instr.imm.value()) + "], rax");
            break;
        }

        default: {
            std::ostringstream tmp;
            tmp << instr;
            emit("# unknown MIR: " + tmp.str());
            break;
        }
    }
}

void X86Builder::lowerMIRTerm(const MIRTerm& term) {
    std::visit([&](const auto& t) {

        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, JumpTerm<MIRInstr>>) {

            emit("jmp " + makeBlockLabel(t.target->id));

        } 
        else if constexpr (std::is_same_v<T, BranchTerm<MIRInstr>>) {

            // HIR/SSA branches (including `??`) become labels and jumps.
            // These are the assembly-level equivalent of if/else control
            // flow, not a second source-level AST rewrite.
            // Move condition value to rax for comparison
            emit("mov rax, " + loc(std::get<IRValue>(t.cond)));
            // Compare it with 0 (Extract only the flags)
            emit("cmp rax, 0");
            emit("jne " + makeBlockLabel(t.trueTarget->id));
            emit("jmp " + makeBlockLabel(t.falseTarget->id));

        } 
        else if constexpr (std::is_same_v<T, ReturnTerm>) {

            if (t.value.has_value()) {
                // move return value into ABI register
                // e.g. rax
                emit("mov rax, " + loc(std::get<IRValue>(t.value.value())));
            }

            if (currFunc->isEntryFunc) {
                emit("mov eax, 0");
            }

            emit("mov rsp, rbp");
            emit("pop rbp");
            emit("ret");

        }
        else if constexpr (std::is_same_v<T, HaltTerm>) {
           emit("ret");
        }

    }, term);
}

void X86Builder::lowerMIRBlock(MIRBlock* mirBlock, MIRFunction* mirFunc) {

    bool isEntry = false;
    if (mirFunc->entry == mirBlock) {
        emit("# entry to function " + std::to_string(mirFunc->functionId));
        if (mirFunc->isEntryFunc) {
            emit("# also the entry function 'main'");
            isEntry = true;
        }
    }
    if (isEntry) {
        emit("main: ");
    } else {
        emit(makeBlockLabel(mirBlock->id) + ":");
    }
    indent++;
    if (mirFunc->entry == mirBlock) {
        
        int stackSize = mirFunc->lastValueId * 8; // Align in 8 bits.
        stackSize = (stackSize + 15) & ~15;

        // By this point, rsp (stack pointer) stores the return addr to the caller.
        // since call foo will be called from caller.

        /*
        After call foo

        | caller stuff         | 
        | arguments...(if any) |
        | return addr          | <- rsp 
        */
        
        // Save current frame pointer (rbp) on stack
        // push instr also moves the stack by 8 bytes too.
        /*
        After push rbp

        | caller stuff           | 
        | arguments...(if any)   |
        | return addr to caller  |
        | rbp for the caller     | <- rsp
        */

        emit("push rbp");
        // New frame base pointer (rbp) = rsp
        emit("mov rbp, rsp");
        // Adjust stack size
        emit("sub rsp, " + std::to_string(stackSize));

        /*
        After function entry
        | caller stuff           | 
        | arguments...(if any)   |
        | return addr to caller  |
        | rbp for the caller     | <- rbp
        | callee locals (v0...)  |
        | ...                    |
        | callee locals end      | <- rsp

        */
    }
    for (const auto& mirInstr : mirBlock->code) {
        lowerMIRInstr(mirInstr);
    }
    lowerMIRTerm(mirBlock->term.value()); // TODO: why optional?
    
    indent--;
    emit("\n");
}

void X86Builder::lowerMIRFunc(MIRFunction* mirFunc) {
    currFunc->functionId = mirFunc->functionId;
    // Allocate stack slots
    for (const auto& mirBlock : mirFunc->blocks) {
        lowerMIRBlock(mirBlock, mirFunc);
    }
}

void X86Builder::build() {
    emit(".intel_syntax noprefix");
    emit(target == X86Target::Linux ? ".section .rodata" : ".section .rdata");
    for (size_t i = 0; i < stringPool.size(); ++i) {
        emit("kmy_str_" + std::to_string(i) + ":");
        emit(".asciz \"" + escapeAsmString(stringPool.get(static_cast<StringId>(i))) + "\"");
    }
    if (globalSlotCount > 0) {
        emit(".section .bss");
        emit(".align 8");
        emit("kmy_globals:");
        emit(".zero " + std::to_string(globalSlotCount * 8));
    }
    emit(".text");
    emit(".globl main");
    for (const auto& mirFunc : mirFunctions) {
        currFunc = mirFunc;
        lowerMIRFunc(mirFunc);
    }
    if (target == X86Target::Linux) {
        emit(".section .note.GNU-stack,\"\",@progbits"); // ELF: no executable stack required.
    }
}

static std::string escapeAsmString(const std::string& value) {
    std::ostringstream escaped;
    for (unsigned char c : value) {
        switch (c) {
            case '\\': escaped << "\\\\"; break;
            case '"':  escaped << "\\\""; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (c >= 32 && c <= 126) {
                    escaped << static_cast<char>(c);
                } else {
                    escaped << "\\" << std::oct << std::setw(3)
                            << std::setfill('0') << static_cast<int>(c)
                            << std::dec << std::setfill(' ');
                }
        }
    }
    return escaped.str();
}
