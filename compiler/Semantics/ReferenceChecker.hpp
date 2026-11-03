#pragma once

#include <unordered_map>
#include <vector>

#include "../Utils/DefaultVisitor.hpp"
#include "Place.hpp"

struct Module;

// Availability belongs to a binding; ownership kind belongs to its value type.
enum class BindingAvailability {
    Uninitialized,
    Available,
    Moved,
    MaybeUnavailable // For branched lvalue
};

enum class OwnershipKind {
    Copy,
    Unique,
    Shared
};

using AvailabilityMap = std::unordered_map<const VarSymbol*, BindingAvailability>;

// Snapshots of paths targeting the nearest enclosing loop, not body state.
struct LoopOwnershipContext {
    std::vector<AvailabilityMap> breakStates;
    std::vector<AvailabilityMap> continueStates;
};

// Ownership-analysis state, separate from the closure/codegen FunctionContext.
// Save/restore at function boundaries and control-flow handling come next.
struct OwnershipFunctionContext {
    FunctionExpr* function = nullptr;
    AvailabilityMap availability;
    // Can the currently analysed path reach its next statement? This is not
    // whole-function reachability or a separate flag per lexical block.
    // return/break/continue set it false; branch/loop merges determine whether
    // another path can continue. break/continue terminate only this body path.
    bool reachable = true;
    // Function-local: nested functions must not inherit enclosing loop targets.
    std::vector<LoopOwnershipContext> loopStack;
};

// Traversal skeleton with state storage for rebuilding ownership rules.
class ReferenceChecker : public DefaultVisitor {
public:
    explicit ReferenceChecker(Module& module);
    void check();

private:
    Module& module;
    OwnershipFunctionContext currCtx;

    std::optional<Place> resolvePlace(const ExprPtr& expr);
    OwnershipKind getOwnershipKind(const ExprPtr& expr) const;
    // Call after visiting the expression to check its reads before transfer.
    void consumeValue(const ExprPtr& expr);
    void requireAvailable(const VarSymbol* symbol, const std::string& name) const;
    AvailabilityMap mergeAvailability(
        const AvailabilityMap& visible,
        const std::vector<AvailabilityMap>& incoming
    ) const;

    // Reads, writes, and transfers. Bodies initially only traverse the AST.
    void visit(Variable& e) override;
    void visit(ScopeAccessExpr& e) override;
    void visit(ThisExpr& e) override;
    void visit(UnaryExpr& e) override;
    void visit(BinaryExpr& e) override;
    void visit(Assignment& e) override;
    void visit(Index& e) override;
    void visit(Get& e) override;
    void visit(Call& e) override;
    void visit(FunctionExpr& e) override;
    void visit(NewExpr& e) override;
    void visit(ArrayLiteral& e) override;

    // Binding state, scopes, and control-flow merging.
    void visit(Let& s) override;
    void visit(Return& s) override;
    void visit(Block& s) override;
    void visit(If& s) override;
    void visit(While& s) override;
    void visit(Break& s) override;
    void visit(Continue& s) override;
};
