#include "SSABuilder.hpp"
#include <assert.h>
#include <iostream>
#include <functional>
#include "../Core/errorhandler.hpp"

SSABuilder::SSABuilder(
    const std::vector<HIRFunction*> funcs
) : funcs(funcs) {}

void SSABuilder::build() {
    for (auto* func : funcs) {
        dom.clear();
        idom.clear();
        domTree.clear();
        DF.clear();
        phiBlocks.clear();
        defBlocks.clear();

        computeDoms(func);
        printDoms();

        computeIDoms(func);
        printIDoms();

        buildDomTree();
        printDomTree(func->entry);

        computeDFNaive(func);
        printDF();

        collectDefs(func);
        printDefBlocks();

        std::cout << "This function " << func->functionId << " lastVId: " << func->lastValueId << "\n";
        
        computePhiPos(func);
        printPhiBlocks();
        insertPhis(func);

        // Final boss.
        renameSSA(func);
    }
}

static inline BlockSet intersect(BlockSet a, BlockSet b) {
    if (a.size() > b.size()) {
        std::swap(a, b);
    }

    // PRE: |a| <= |b|
    BlockSet res;
    for (const auto& elemA : a) {
        if (b.find(elemA) != b.end()) {
            res.insert(elemA);
        }
    }
    return std::move(res);
}
// TODO: bitset for optimisation!
// Or possibly Lengauer-Tarjan algorithm for linear time complexity. (idk what this is)
void SSABuilder::computeDoms(HIRFunction* func) {
    // Formula: dom[B] = {B} U intersection(dom[P])

    // Initialise first with dom[B] = EVERY_BLOCK;
    for (const auto& block : func->blocks) {
        if (block == func->entry) {
            dom[block] = {block};
        } else {
            dom[block] = BlockSet{
                func->blocks.begin(),
                func->blocks.end()
            };
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& block : func->blocks) {
            if (block == func->entry) continue;

            assert(!block->preds.empty());
            BlockSet newDom = dom.at(block->preds[0]);
            for (size_t i = 1; i < block->preds.size(); ++i) {
                newDom = intersect(dom.at(block->preds[i]), newDom);
            }

            newDom.insert(block);

            if (newDom != dom.at(block)) {
                // Something changed!
                changed = true;
                dom[block] = std::move(newDom);
            }
        }
    }
}

void SSABuilder::computeIDoms(HIRFunction* func) {
    /*
     * D is idom(B) iff:
     *
     *   - D strictly dominates B, and
     *   - D is dominated by every other strict dominator of B.
     *
     * In other words, D is the strict dominator of B that is closest
     * to B in the dominator tree.
     *
     * For every other strict dominator X of B:
     *
     *     X dominates D
     *
     * which we can check as:
     *
     *     X ∈ dom[D]
     *
     * The candidate is guaranteed to be unique.
     */

    for (const auto& block : func->blocks) {
        if (block == func->entry) {
            idom[block] = nullptr;
            continue;
        }

        HIRBlock* candidate = nullptr;
        for (const auto& d : dom[block]) {
            // Exclude itself (itself cannot be idom)
            if (d == block) continue;

            bool dominatedByAll = true;
            for (const auto& X: dom[block]) {
                // Exclude d itself.
                if (X == block || X == d) continue; 
                
                if (dom[d].find(X) == dom[d].end()) {
                    dominatedByAll = false;
                    break;
                }
            }

            if (dominatedByAll) {
                // Guaranteed to be unique (can be proved using proof by contradiction.)
                candidate = d;
                break;
            }
        }

        // The must be exactly one idom per block.
        assert(candidate != nullptr);
        idom[block] = candidate;
    }
}

void SSABuilder::buildDomTree() {
    for (const auto& [block, id] : idom) {
        if (id == nullptr) continue;

        // block (child) has idom of id (parent).
        domTree[id].insert(block);
    }
}

// Dominance frontier = the set of blocks where a block's dominance ends.
// More casually:
//     For a block X, Y is in DF[X] when X dominates a predecessor of Y,
//     but X does not strictly dominate Y.
//     In other words, X is involved in one route into Y,
//     but there is also a route into Y that does not require X.

/*
* Y ∈ DF[X] if X dominates a predecessor of Y, but X does not strictly dominate Y.
* in simple words, there is a way from X -> Y, but X doesn't dominate Y.

Bottom-up approach(?) (X to its preds and idom parent)
- For this iteration of X, we find 
    "which blocks dominate the predecessor of X, which is P, but doesn't strictly dominate X"
- We know if a node dominates some node K, its domTree ancestors also dominate K. (*)
    - This is essentially the first part required for the definition of DF[ancestor] to have X.
    - Once P dominates X, its ancestors in domTree will.
- So we stop once runner == idom[block] 
    - runner == idom[block] means runner strictly dominates block
    - domTree we can know: A0 - A1 - A2 - A3 - P
                                          ^runner (moves <-)
                                     ^ The one to have == idom[X]
    - All ancestors until runner like A3, P dominates P but doesn't strictly dominate X.
                          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                          |                             | because it passed the loop condition runner != idom[X]
                          |                             | More specifically, they are the child of the one to have "== idom[X]" (A2)
                          |                             | So they cannot dominate X. So the check runner != idom[X] is sufficient. (Read stuff below.)
                          | because it is walking P's domTree ancestor including P.
                                                        
    - Imagine runner stopped there (A2 == idom[X])
    - Again, automatically A0, A1 disqualifies as having X in DF since it dominates X. (Due to property (*))
*/
/*
Why TF !dom[X].contains(runner) || X == runner <=> idom[X] != runner?
For CFG edge P -> X:

Dominator tree:

             ...
              |
           idom[X]       <- stop here
              |
              A
              |
              P          <- runner starts here

runner walks upward:
    P -> A -> ... -> idom[X]

Every runner on this chain dominates P.

While runner != idom[X], runner is below idom[X] in the dominator tree.
If runner also strictly dominated X, the tree would have to look like:

           idom[X]
              |
           runner
              |
              X

But this is impossible: idom[X] is the immediate dominator of X,
so no other dominator can exist between idom[X] and X.

Therefore, every runner before idom[X] does NOT strictly dominate X.

Special case: runner == X (possible with a loop):

           idom[X]
              |
              X = runner

X does not strictly dominate itself, so it is also valid.

Therefore, in this specific idom-chain walk:

    runner != idom[X]
        <=> runner does not strictly dominate X
        <=> !dom[X].contains(runner) || runner == X

We stop when runner == idom[X], because idom[X] strictly dominates X
and therefore is not in DF[X].

*/
void SSABuilder::computeDF_runner(HIRFunction* func) {
    for (const auto& block : func->blocks) {
        if (block->preds.size() < 2) continue; // entry block is handled here.
        // Since the only way to X is through P, P dominates X.
        // Thus, P and P's ancestor will never have X in its DF.

        for (const auto& pred : block->preds) {
            auto runner = pred;

            while (runner != idom[block]) {
                DF[runner].insert(block);
                runner = idom[runner];
            }
        }
    }
}

/*
* Y ∈ DF[X] if X dominates a predecessor of Y, but X does not strictly dominate Y.
* in simple words, there is a way from X -> Y, but X doesn't dominate Y.

Top-down approach(?) (X to its succs and domTree children)
DF[X] =
    direct places where X's dominance stops
    - say X has pred Y. If idom[Y] != X, X dominance stops at Y. so DF[X].insert(Y)
    - Exactly applies to the condition: 
        - X dominates X, which is a predecessor of Y.
        - X doesn't strictly dominate Y.
            - We only check idom[Y] != X since graph is like X -> Y
        - i.e., path X -> Y is there but X doesn't dominate Y.
    +
    (Tricky case) frontier places inherited from X's dominator-tree children
    - Say X has domTree children Z. (idom[Z] = X)
    - There is obviously a path from X to Z. (X -> ... -> Z)
    - Say DF[Z] has Y.
        - So Z dominates the predecessor of Y.
        - But Z doesn't strictly dominate Y.
    - Then, X's domination may leak for Y. (they are connected but X may not dominate Y)
    - This also applies to the condition: 
        - X dominates Z, so does the predecessor of Y. (Bc of transitivity of dominance.)
            - X dominates Z.
            - Z dominates the predecessor of Y.
            - Thus, X dominates the predecessor of Y.
        - X doesn't strictly dominate Y.
            - Naively, we need to check !dom[Y].contains(X) || X == Y.
            - However, this check can be simplified to idom[Y] != X
            - Why?
                // General condition: X must not strictly dominate Y:
                //     !dom[Y].contains(X) || X == Y
                //
                // Here, Y ∈ DF[Z] and idom[Z] == X.
                // Thus Z dominates a predecessor P of Y, giving a domTree path:
                //
                //     X -> Z -> ... -> P -> Y (p1)
                //
                // If X strictly dominated Y but idom[Y] != X, there would have
                // to be some intermediate dominator A between X and Y:
                //
                //     X -> ... -> A -> Y
                //
                // But A cannot dominate Y because the path above (p1) reaches Y from
                // Z without passing through A. Contradiction.
                //
                // Therefore, in this specific DF propagation case:
                //
                //     X strictly dominates Y  <=>  idom[Y] == X
                //
                // So we can simply check:
                //
                //     idom[Y] != X
        - i.e., path X -> ... -> Z -> ... -> Y exists
        - but X may not strictly dominate Y.
        - So we check idom[Y] != X.
*/

void SSABuilder::computeDFNaive(HIRFunction* func) {
    /*
     * Y ∈ DF[X] iff:
     *
     *     X dominates some predecessor P of Y
     *     AND
     *     X does not strictly dominate Y.
     */

    for (HIRBlock* X : func->blocks) {
        for (HIRBlock* Y : func->blocks) {

            // X must not strictly dominate Y.
            if (X != Y && dom[Y].count(X) > 0)
                continue;

            for (HIRBlock* P : Y->preds) {
                // Does X dominate this predecessor of Y?
                if (dom[P].count(X) > 0) {
                    DF[X].insert(Y);
                    break;
                }
            }
        }
    }
}

void SSABuilder::computeDF(HIRFunction* func) {
    std::vector<HIRBlock*> postorder;

    std::function<void(HIRBlock*)> dfs =
        [&](HIRBlock* block) {
            for (HIRBlock* child : domTree[block]) {
                dfs(child);
            }

            postorder.push_back(block);
        };

    dfs(func->entry);

    for (HIRBlock* block : postorder) {
        DF[block] = BlockSet{};
        if (block == func->entry)
            continue;

        // Local DF
        for (HIRBlock* succ : block->succs) {
            if (idom[succ] != block) {
                DF[block].insert(succ);
            }
        }

        // Up DF
        for (HIRBlock* child : domTree[block]) {
            for (HIRBlock* y : DF[child]) {
                if (idom[y] != block) {
                    DF[block].insert(y);
                }
            }
        }
    }
}


void SSABuilder::collectDefs(HIRFunction* func) {
    for (const auto& block : func->blocks) {
        for (const auto& def : block->defs) {
            defBlocks[def.sym].insert(block);
        }
    }
}

/*
 * Insert phi functions using the Iterated Dominance Frontier (IDF).
 *
 * For each variable:
 *
 *   1. Start with all blocks that explicitly define the variable.
 *      These are stored in:
 *
 *          defBlocks[var]
 *
 *      Example:
 *
 *          B0: x = 1
 *          B2: x = 2
 *
 *          defBlocks[x] = { B0, B2 }
 *
 *   2. A definition in block X may reach a block Y through one path,
 *      while another path reaches Y without going through X.
 *      
 *      In this case, the definition of 'x' may come from X or somewhere else.
 *      Thus representing the merge point for different incoming var 'x'.
 *
 *      That is exactly what DF[X] represents.
 *
 *      Therefore, for every:
 *
 *          Y ∈ DF[X]
 *
 *      Y is a candidate merge point for definitions of the variable,
 *      so we insert:
 *
 *          x = phi(...)
 *
 *   3. IMPORTANT: a phi is itself a new definition of x.
 *
 *      Therefore, after inserting phi(x) at Y, we must treat Y as
 *      another definition block and continue through DF[Y].
 *
 *          original definition
 *                  |
 *                 DF
 *                  v
 *             insert phi
 *                  |
 *             phi is a new definition
 *                  |
 *                 DF
 *                  v
 *             insert more phi
 *
 *      This repeated propagation is why this is called the
 *      "Iterated Dominance Frontier". (With "worklist")
 *
 *   4. hasPhi prevents inserting the same phi more than once when
 *      multiple definition blocks reach the same frontier block.
 *
 * Note:
 *   This pass only decides WHERE phi functions are needed.
 *   It does not fill in their incoming SSA values.
 *   The later SSA-renaming pass will do that.
 */
void SSABuilder::computePhiPos(HIRFunction* func) {
    for (const auto& [var, defs] : defBlocks) {

        // Blocks where we have already inserted phi(var).
        BlockSet hasPhi;

        // Start propagation from the blocks that explicitly define var.
        std::vector<HIRBlock*> worklist;

        for (HIRBlock* defBlock : defs) {
            worklist.push_back(defBlock);
        }

        // By here, for each def block of `var` (say such blocks are X), we insert phi on every Y in DF[X]
        // Because that represents the "possible merge point" of var def that originated from X.

        while (!worklist.empty()) {
            HIRBlock* X = worklist.back();
            worklist.pop_back();

            // X's dominance frontier contains blocks where X's
            // definition may need to merge with another definition. (See explanation above.)
            for (HIRBlock* Y : DF[X]) {

                // Another definition may have already caused a phi(var)
                // to be inserted here.
                if (hasPhi.count(Y) > 0) {
                    continue;
                }

                // Y needs a phi for var.
                phiBlocks[Y].insert(var); // insert phi in block Y that needs phi def for var.
                hasPhi.insert(Y);

                // The newly inserted phi is itself a definition of var.
                // Therefore its dominance frontier must also be explored.
                worklist.push_back(Y);
            }
        }
    }
}

static IRValue makeValue(HIRFunction* func, Type* type) {
    return IRValue{.id = func->lastValueId++, .type = type};
}


// TODO: Make a dedicated phi zone instead of shoving to instr!
void SSABuilder::insertPhis(HIRFunction* func) {
    for (const auto& [block, vars] : phiBlocks) {

        // PHIs must be the first instructions in a block.
        auto pos = block->code.begin();

        for (VarSymbol* var : vars) {

            // A dominance-frontier candidate is not necessarily a real phi:
            // locals declared inside a loop body can appear in the frontier
            // of the loop header even though the header's entry edge has no
            // definition for that local. Such a phi would have an impossible
            // incoming value and would later underflow varStack during rename.
            auto defsIt = defBlocks.find(var);
            if (defsIt == defBlocks.end()) {
                continue;
            }

            bool everyPredecessorHasDefinition = true;
            for (HIRBlock* pred : block->preds) {
                bool hasDefinition = false;
                for (HIRBlock* defBlock : defsIt->second) {
                    if (dom.at(pred).count(defBlock) != 0) {
                        hasDefinition = true;
                        break;
                    }
                }

                if (!hasDefinition) {
                    everyPredecessorHasDefinition = false;
                    break;
                }
            }

            if (!everyPredecessorHasDefinition) {
                continue;
            }

            // Allocate the SSA result of the PHI.
            IRValue result = makeValue(func, var->type);

            IRInstr phi{
                .op = IROp::PHI,
                .dst = result,
                .phiVar = var // TODO TODO Hmm this should be an instr tho!
            };

            // Create one incoming slot for every predecessor.
            //
            // The value is only a temporary placeholder.
            // renameSSA() will replace it with the actual SSA value.
            for (HIRBlock* pred : block->preds) {
                phi.phis.push_back({
                    .pred = pred,
                    .value = VarRef{var} // temporary!!
                });
            }

            pos = block->code.insert(
                pos,
                std::move(phi)
            );

            ++pos;
        }
    }
}

// This is kinda like a starting wrapper for recursion since it needs stack.
static IRValue lookupVar(
    std::unordered_map<VarSymbol*, std::vector<IRValue>>& varStack,
    VarSymbol* var
) {
    auto it = varStack.find(var);

    if (it == varStack.end() || it->second.empty()) {
        throw KMYCompileError(
            "SSA variable '" + var->name +
            "' has no reaching definition"
        );
    }

    return it->second.back();
}

static void renameOperand(
    std::unordered_map<VarSymbol*, std::vector<IRValue>>& varStack,
    HIROperand& operand
) {
    if (auto* ref = std::get_if<VarRef>(&operand)) {
        operand = lookupVar(varStack, ref->sym);
    }
}

static void renameTerminator(
    std::unordered_map<VarSymbol*, std::vector<IRValue>>& varStack,
    HIRBlock* block
) {
    if (!block->term.has_value())
        return;

    std::visit(
        [&](auto& term) {
            using T = std::decay_t<decltype(term)>;

            if constexpr (
                std::is_same_v<T, BranchTerm<IRInstr>>
            ) {
                renameOperand(varStack, term.cond);
            }
            else if constexpr (
                std::is_same_v<T, ReturnTerm>
            ) {
                if (term.value.has_value())
                    renameOperand(varStack, term.value.value());
            }
            else if constexpr (
                std::is_same_v<T, JumpTerm<IRInstr>>
            ) {
            }
            else if constexpr (
                std::is_same_v<T, HaltTerm>
            ) {
            }
        },
        *block->term
    );
}

void SSABuilder::rename(
    std::unordered_map<VarSymbol*, std::vector<IRValue>>& varStack,
    HIRFunction* func,
    HIRBlock* block
) {
    (void)func;

    std::vector<VarSymbol*> pushed;

    for (IRInstr& instr : block->code) {
        if (instr.op != IROp::PHI)
            break;

        assert(instr.dst.has_value());

        if (instr.phiVar != nullptr) {
            VarSymbol* var = instr.phiVar;

            varStack[var].push_back(
                instr.dst.value()
            );

            pushed.push_back(var);
        }
        else if (instr.defSym != nullptr) {
            VarSymbol* var = instr.defSym;

            varStack[var].push_back(
                instr.dst.value()
            );

            pushed.push_back(var);
        }
    }

    for (IRInstr& instr : block->code) {
        if (instr.op == IROp::PHI)
            continue;

        for (HIROperand& operand : instr.args) {
            renameOperand(
                varStack,
                operand
            );
        }

        if (instr.defSym == nullptr)
            continue;

        VarSymbol* var = instr.defSym;

        if (instr.op == IROp::BIND) {
            assert(instr.args.size() == 1);

            auto* value =
                std::get_if<IRValue>(&instr.args[0]);

            assert(value);

            varStack[var].push_back(*value);
        } else {
            assert(instr.dst.has_value());

            varStack[var].push_back(
                instr.dst.value()
            );
        }

        pushed.push_back(var);
    }

    renameTerminator(
        varStack,
        block
    );

    for (HIRBlock* succ : block->succs) {

        for (IRInstr& phi : succ->code) {
            if (phi.op != IROp::PHI)
                break;

            if (phi.phiVar != nullptr) {

                IRValue incomingValue =
                    lookupVar(
                        varStack,
                        phi.phiVar
                    );

                bool found = false;

                for (IncomingPhi& incoming : phi.phis) {
                    if (incoming.pred == block) {
                        incoming.value = incomingValue;
                        found = true;
                        break;
                    }
                }

                assert(found);

                continue;
            }

            for (IncomingPhi& incoming : phi.phis) {
                if (incoming.pred != block)
                    continue;

                renameOperand(
                    varStack,
                    incoming.value
                );
            }
        }
    }

    auto it = domTree.find(block);

    if (it != domTree.end()) {
        for (HIRBlock* child : it->second) {
            rename(
                varStack,
                func,
                child
            );
        }
    }

    for (auto it = pushed.rbegin();
         it != pushed.rend();
         ++it) {

        VarSymbol* var = *it;

        auto stackIt = varStack.find(var);

        assert(
            stackIt != varStack.end() &&
            !stackIt->second.empty()
        );

        stackIt->second.pop_back();
    }
}

void SSABuilder::renameSSA(HIRFunction* func) {
    std::unordered_map<
        VarSymbol*,
        std::vector<IRValue>
    > varStack;

    rename(
        varStack,
        func,
        func->entry
    );
}
void SSABuilder::printDoms() const {
    for (const auto& [block, doms] : dom) {
        std::cout << "Dom(B"
                  << block->id
                  << ") = { ";

        for (HIRBlock* d : doms) {
            std::cout << "B"
                      << d->id
                      << " ";
        }

        std::cout << "}\n";
    }
}

void SSABuilder::printIDoms() const {
    for (const auto& [block, id] : idom) {
        std::cout << "IDom(B"
                  << block->id
                  << ") : ";

        if (id) {
            std::cout << "B"
                    << id->id
                    << "\n";
        } else {
            std::cout << "<none>\n";
        }
    }
}

void SSABuilder::printDomTree(HIRBlock* block, int depth) const {
    std::cout << std::string(depth * 2, ' ')
              << "B" << block->id
              << "\n";

    auto it = domTree.find(block);
    if (it == domTree.end())
        return;

    for (HIRBlock* child : it->second) {
        printDomTree(child, depth + 1);
    } 
}

void SSABuilder::printDF() const {
    for (const auto& [block, dfs] : DF) {
        std::cout << "DF(B"
                  << block->id
                  << ") = { ";

        for (HIRBlock* df : dfs) {
            std::cout << "B"
                      << df->id
                      << " ";
        }

        std::cout << "}\n";
    }
}

void SSABuilder::printDefBlocks() const {
    for (const auto& [sym, defBlock] : defBlocks) {
        std::cout << "defBlock("
                  << sym->name
                  << ") = { ";

        for (HIRBlock* df : defBlock) {
            std::cout << "B"
                      << df->id
                      << " ";
        }

        std::cout << "}\n";
    }
}

void SSABuilder::printPhiBlocks() const {
    for (const auto& [block, vars] : phiBlocks) {
        std::cout << "phiBlock("
                  << block->id
                  << ") contains phi definitions for { ";

        for (VarSymbol* sym : vars) {
            std::cout << sym->name << ", ";
        }

        std::cout << "}\n";
    }
}
/*
Example CFG

          entry/B0
              |
              v
             B1 <---------+
           /    \         |
          v      v        |
         B2      B3       |
        /  \      ^       |
       v    v     |       |
      B4    B5 ---+-------+
       |           |
       +---------->B3

*/
