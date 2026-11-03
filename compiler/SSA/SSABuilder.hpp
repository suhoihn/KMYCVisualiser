#pragma once

#include <unordered_set>
#include "../CodegenIR/BasicBlock.hpp"
#include "../CodegenIR/IRFunction.hpp"
#include "../CodegenIR/IRBuilder.hpp" // HACK

using BlockSet = std::unordered_set<HIRBlock*>;
using VarSet = std::unordered_set<VarSymbol*>;

class SSABuilder {
public:
    SSABuilder(const std::vector<HIRFunction*> funcs);
    void printDoms() const;
    void printIDoms() const;
    void printDomTree(HIRBlock* block, int depth=0) const;
    void printDF() const;
    void printDefBlocks() const;
    void printPhiBlocks() const;
    void build();

private:
    std::vector<HIRFunction*> funcs;
    // Dominators (Blocks that are essential to pass through from entry)
    /*
    Example
    -------
    B0 (entry) -> B1 -> B3 -> B5 -> B6 -> B3
               -> B2 -> B4 /
    
    Note that dom[B] includes B itself.
    dom[B0] = {B0}
    dom[B1] = {B0, B1}
    dom[B2] = {B0, B2}
    dom[B3] = {B0, B1, B3}
    dom[B4] = {B0, B2, B4}
    // Note that nothing except entry and itself dominates 
    // since there are multiple ways to reach B5
    dom[B5] = {B0, B5} 
    dom[B6] = {B0, B5, B6}

    Formula: For all B, dom[B] = {B} U (for all P in B.preds, intersection dom[P])

    Proof.
    Say D is a dominator of B.
        D ∈ dom[B]

    By definition, all path from E (entry) -> B includes D.

    The path must be E -> ... -> P (pred) -> B for any P of B.

    Since B != P,  D must occur on the path E -> ... -> P.

    Thus, D is a dominator of P by definition.
    -> D ∈ dom[P]

    Since P was arbitary, 
        if D != B:
            D ∈ intersection(dom[P]) for all P ∈ B.preds
        And since B trivially dominates itself:
            D ∈ {B} U intersection(dom[P])

    Thus,
        For all D ∈ dom[B] -> D ∈ {B} U intersection(dom[P]) holds.

    By definition of ⊆,
        dom[B] ⊆ {B} U intersection(dom[P])

    Let's prove the reverse direction.
    (Aim: {B} U intersection(dom[P]) ⊆ dom[B])

    Assume D ∈ {B} U intersection(dom[P])

    Case 1: D = B
        Trivially, D ∈ dom[B]
    
    Case 2: D ∈ intersection(dom[P])
        This means D ∈ dom[P] for any P.

        Since P is a predecessor of B, the path is like
            E -> ... -> P -> B
        
        By assumption, D ∈ dom[P], so D must occur on the path
            E -> ... -> P
        
        This means D also occurs on the path
            E -> ... -> P -> B
        
        Thus, D ∈ dom[B].

    Since all cases lead to the conclusion D ∈ dom[B],
        For all D ∈ {B} U intersection(dom[P]) -> D ∈ dom[B] holds.
    
    By definition of ⊆,
        {B} U intersection(dom[P]) ⊆ dom[B]

    Therefore, since 
        {B} U intersection(dom[P]) ⊆ dom[B]
        and 
        dom[B] ⊆ {B} U intersection(dom[P]),

        dom[B] = {B} U intersection(dom[P])

    */
    std::unordered_map<HIRBlock*, BlockSet> dom;
    std::unordered_map<HIRBlock*, HIRBlock*> idom;
    std::unordered_map<HIRBlock*, BlockSet> domTree;
    std::unordered_map<HIRBlock*, BlockSet> DF; // Dominance frontier
    std::unordered_map<VarSymbol*, BlockSet> defBlocks; // VarSymbol <-> blocks that define it.

    // For each block, the set of variables that need a phi-function at that block.
    // phiBlocks[X] = { var1, var2, ... } means 
    // phi for var1, phi for var2, ... declaration lives in X.
    // NOTE: the phi args are not filled yet. 
    // (stores info like v? = phi(???) for variable cnt is in block X)
    std::unordered_map<HIRBlock*, VarSet> phiBlocks;
    
    void computeDoms(HIRFunction* func);
    void computeIDoms(HIRFunction* func);
    void buildDomTree();

    void computeDFNaive(HIRFunction* func);
    void computeDF(HIRFunction* func);
    void computeDF_runner(HIRFunction* func);

    void collectDefs(HIRFunction* func);
    void computePhiPos(HIRFunction* func);
    void insertPhis(HIRFunction* func);
    void renameSSA(HIRFunction* func);
    void rename(std::unordered_map<VarSymbol*, std::vector<IRValue>>& varStack, HIRFunction* func, HIRBlock* block);
};
