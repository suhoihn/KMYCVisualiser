#pragma once

#include <vector>
#include "../CodegenIR/BasicBlock.hpp"
#include "../CodegenIR/IRFunction.hpp"
#include "MIRInstr.hpp"
#include "../CodegenIR/IRInstr.hpp"
#include "../CodegenIR/CommonDef.hpp"

using MIRFunction = IRFunction<MIRInstr>;
using MIRBlock = BasicBlock<MIRInstr>;
using MIRTerm = Terminator<MIRInstr>;

class MIRBuilder {
private:
    std::vector<HIRFunction*> irFunctions;

    int lastValueId = 0; // Reset for every function.
    std::vector<MIRInstr> lowerHIRInstr(const IRInstr& instr);
    MIRFunction* lowerHIRFunc(HIRFunction* hirFunc);
    IRValue makeValue(Type* type);
    
    void insertMoves(
        const IRInstr& instr,
        std::unordered_map<HIRBlock*, MIRBlock*>& blockMap, 
        const std::vector<IncomingPhi>& phis
    );

public:
    MIRBuilder(std::vector<HIRFunction*> irFunctions);

    std::vector<MIRFunction*> lower();
};
