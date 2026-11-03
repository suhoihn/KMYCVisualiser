#include "IRFunction.hpp"
#include "IRInstr.hpp"

using HIRFunction = IRFunction<IRInstr>;
using HIRBlock = BasicBlock<IRInstr>;
using HIRTerm = Terminator<IRInstr>;