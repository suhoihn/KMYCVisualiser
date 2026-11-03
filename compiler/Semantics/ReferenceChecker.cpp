#include "ReferenceChecker.hpp"

#include "../Core/errorhandler.hpp"
#include "../Core/newParser.hpp"
#include <memory>
#include <utility>

ReferenceChecker::ReferenceChecker(Module& module)
    : module(module) {}

void ReferenceChecker::check() {
    for (const StmtPtr& statement : module.topLevelStatements) {
        statement->accept(*this);
    }
}

OwnershipKind ReferenceChecker::getOwnershipKind(const ExprPtr& expr) const {
    // Resolver supplies the type. Classification does not visit the expression
    // or change binding availability; the consuming context does that later.
    const Type* type = expr ? expr->type : nullptr;
    while (type && type->kind == TypeKind::NULLABLE) {
        type = static_cast<const NullableType*>(type)->innerType;
    }

    if (!type) {
        throw KMYCompileError("Cannot classify ownership of an unresolved expression type.");
    }

    switch (type->kind) {
        case TypeKind::INT:
        case TypeKind::DOUBLE:
        case TypeKind::BOOL:
        case TypeKind::POINTER:
        case TypeKind::CELL:
        case TypeKind::NULLTYPE:
        case TypeKind::ENUM:
            return OwnershipKind::Copy;

        case TypeKind::STRING:
            // Temporary policy: static strings and allocated runtime strings
            // currently share a type, so owned-string cleanup is deferred.
            return OwnershipKind::Copy;

        case TypeKind::SHARED:
            return OwnershipKind::Shared;

        case TypeKind::FUNCTION:
            // All callable values are shareable. Ownership lowering must retain/
            // release the RC environment; plain functions have a null environment.
            return OwnershipKind::Shared;

        case TypeKind::INSTANCE:
        case TypeKind::ARRAY:
        case TypeKind::STRUCTUAL:
        case TypeKind::ANY:
            return OwnershipKind::Unique;

        default:
            throw KMYCompileError("Expression type has no supported value ownership kind.");
    }
}

void ReferenceChecker::requireAvailable(
    const VarSymbol* symbol,
    const std::string& name
) const {
    if (!symbol) {
        throw KMYCompileError(
            "[SERIOUS] ReferenceChecker requires a resolved symbol for \"" + name + "\"."
        );
    }

    auto found = currCtx.availability.find(symbol);
    if (found == currCtx.availability.end()) {
        // Transitional: declaration/parameter registration is not implemented
        // yet, and built-ins/imported values also need a defined entry policy.
        // Missing state means untracked, not proven Available.
        return;
    }

    switch (found->second) {
        case BindingAvailability::Available:
            return;
        case BindingAvailability::Uninitialized:
            throw KMYCompileError("Use of uninitialized value \"" + name + "\".");
        case BindingAvailability::Moved:
            throw KMYCompileError("Use of moved value \"" + name + "\".");
        case BindingAvailability::MaybeUnavailable:
            throw KMYCompileError(
                "Value \"" + name + "\" is not available on every control-flow path."
            );
    }
}

void ReferenceChecker::visit(Variable& e) {
    requireAvailable(e.symbol, e.name);
}

void ReferenceChecker::visit(ScopeAccessExpr& e) {
    // If there is no symbol, this is enum constant (e.g. Color::Red)
    if (!e.symbol) return;

    // Preserve the qualified spelling in diagnostics, e.g. math::something.
    // name here is just used for the error message :)
    std::string name;
    for (const std::string& part : e.parts) {
        if (!name.empty()) name += "::";
        name += part;
    }
    requireAvailable(e.symbol, name);
}

void ReferenceChecker::visit(ThisExpr& e) {
    // TODO: Check the implicit receiver binding.
}

void ReferenceChecker::visit(UnaryExpr& e) {
    e.operand->accept(*this);
    // TODO: Check place writes for ++/-- and future borrow operations.
}

void ReferenceChecker::visit(BinaryExpr& e) {
    if (!currCtx.reachable) return;
    e.left->accept(*this); // Always evaluated, before deciding whether RHS runs.
    if (!currCtx.reachable) return;

    const bool shortCircuit = e.op == BinaryOp::LogicalAnd ||
        e.op == BinaryOp::LogicalOr || e.op == BinaryOp::NullCoalesce;
    if (!shortCircuit) {
        e.right->accept(*this); // Ordinary operators evaluate both sides.
        return;
    }

    // Fork after LHS: one path skips RHS, the other evaluates it.
    // Conservatively keep both paths, including for constant operands.
    const auto skipped = currCtx.availability;
    e.right->accept(*this);
    if (currCtx.reachable) {
        currCtx.availability = mergeAvailability(
            skipped, {skipped, currCtx.availability});
    } else {
        currCtx.availability = skipped; // Only the skipped path survives.
    }
    currCtx.reachable = true; // The skipped-RHS path still reaches the result.
    // RHS loop-jump records, if any, remain in currCtx; do not erase them.
}

/*
==================== TODO: NEXT SESSION ====================
DONE: Rooted writes, Copy compound assignments, parameter registration,
function-state isolation, call/return consumption, branch and loop merges.

NEXT (in order):
1. Preserve hoisting; define globals/captures/defaults/receiver entry policy.
2. Cover constructor and aggregate-literal transfers, qualified places,
   short-circuit expressions, and ownership-compatible parameter types.
3. Validate positive/negative checker cases and constant-condition precision.
4. IR lowering: destroy/release overwritten values and add scope-exit cleanup;
   insert shared retain/release. Checker state alone does NOT manage memory.

LIMITS: Unique partial moves, computed-base writes, and raw-pointer writes
remain unsupported here. Projected writes leave root availability unchanged.
===========================================================
*/
void ReferenceChecker::visit(Assignment& e) {
    auto destination = resolvePlace(e.left);
    if (!destination || !destination->sym) {
        throw KMYCompileError(
            "Ownership checking requires an assignment place with a resolved root binding."
        );
    }

    const bool projected = !destination->projections.empty();
    const bool compound = e.op != AssignmentOp::Assign;
    for (const Projection& projection : destination->projections) {
        if (std::holds_alternative<DereferenceProjection>(projection)) {
            throw KMYCompileError(
                "Ownership checking for writes through raw pointers is not implemented."
            );
        }
    }

    if (compound &&
        (getOwnershipKind(e.left) != OwnershipKind::Copy ||
         getOwnershipKind(e.right) != OwnershipKind::Copy)) {
        throw KMYCompileError("Compound assignment currently requires Copy values.");
    }

    // Field/index writes need a live container and evaluate base/index reads.
    // Compound assignment also reads the old destination value.
    // A plain whole-variable overwrite does not read its old value.
    if (projected || compound) {
        e.left->accept(*this);
    }
    e.right->accept(*this);

    // Reject unique self-move until we define its cleanup behaviour.
    if (getOwnershipKind(e.right) == OwnershipKind::Unique) {
        auto source = resolvePlace(e.right);
        if (source && source->sym == destination->sym) {
            throw KMYCompileError("Unique self-move assignment is not supported.");
        }
    }

    // Transfer ownership from the RHS, then restore the destination.
    consumeValue(e.right);
    if (projected) {
        // Replacing one slot does not move or reinitialize the whole container.
        // The RHS must not have consumed the container we are writing into.
        requireAvailable(destination->sym, "assignment destination root");
    } else {
        currCtx.availability[destination->sym] = BindingAvailability::Available;
    }
}

void ReferenceChecker::visit(Index& e) {
    e.obj->accept(*this);
    e.index->accept(*this);
    // TODO: Validate the base and index reads; moving an element is partial.
}

void ReferenceChecker::visit(Get& e) {
    e.obj->accept(*this);
    // TODO: Validate the receiver read; distinguish fields from methods.
}

void ReferenceChecker::visit(Call& e) {
    e.func->accept(*this);

    for (const ExprPtr& argument : e.args) {
        argument->accept(*this);
        consumeValue(argument);
    }
}

void ReferenceChecker::visit(FunctionExpr& e) {

    auto oldCtx = std::move(currCtx);
    currCtx = OwnershipFunctionContext {
        .function = &e,
        .availability = {},
        .reachable = true,
        .loopStack = {}
    };

    for (const Parameter& parameter : e.params) {
        if (!parameter.symbol) {
            throw KMYCompileError(
                "[SERIOUS] ReferenceChecker: unresolved parameter symbol."
            );
        }
        currCtx.availability[parameter.symbol] = BindingAvailability::Available;
        if (parameter.defaultValue) {
            parameter.defaultValue->accept(*this);
        }
    }
    e.body->accept(*this);

    currCtx = std::move(oldCtx);
}

void ReferenceChecker::visit(NewExpr& e) {
    if (e.arraySize) {
        e.arraySize->accept(*this); // Read the size, not an ownership transfer.
    }
    // Current constructor parameters are by value; consume in argument order.
    for (const ExprPtr& argument : e.args) {
        argument->accept(*this);
        consumeValue(argument);
    }
}

void ReferenceChecker::visit(ArrayLiteral& e) {
    // Each slot receives its value before checking the next element.
    // Thus [x, x] rejects the second read when x is Unique.
    for (const ExprPtr& element : e.elements) {
        element->accept(*this);
        consumeValue(element);
    }
}

void ReferenceChecker::consumeValue(const ExprPtr& expr) {
    // The caller visits first; this helper only applies the ownership transfer.
    switch (getOwnershipKind(expr)) {
        // RHS is a copyable/shared kind.
        case OwnershipKind::Copy: // Nothing happens
        case OwnershipKind::Shared: break; // Future IR lowering handles retain on copy and release on cleanup.

        // RHS is a unique kind
        case OwnershipKind::Unique: {
            if (expr->kind == ExprKind::BinaryExpr) {
                auto binary = std::static_pointer_cast<BinaryExpr>(expr);
                if (binary->op == BinaryOp::NullCoalesce) {
                    // Consuming ?? transfers only the selected arm. Use the
                    // already-merged evaluation state conservatively; do not
                    // revisit operands or count their side effects twice.
                    const auto before = currCtx.availability;
                    consumeValue(binary->left);
                    auto leftSelected = std::move(currCtx.availability);
                    currCtx.availability = before;
                    consumeValue(binary->right);
                    auto rightSelected = std::move(currCtx.availability);
                    currCtx.availability = mergeAvailability(
                        before, {leftSelected, rightSelected});
                    break;
                }
            }
            auto place = resolvePlace(expr);
            if (place && place->sym) {
                if (!place->projections.empty()) {
                    throw KMYCompileError("Partial moves not supported now... I am so sorry :(");
                }
                currCtx.availability[place->sym] = BindingAvailability::Moved;
            }
            // No resolved place means no source binding is marked moved.
            // Computed storage is not yet represented; this is not proof of a temporary.
            break;
        }

        default: throw KMYCompileError("SERIOUS ISSUE: UNDEFINED OWNERSHIP KIND! MEMORY CORRUPTION SUSPECTED!");
    }
}

void ReferenceChecker::visit(Let& s) {
    currCtx.availability[s.symbol] = BindingAvailability::Uninitialized;
    
    if (s.expr) {
        s.expr->accept(*this);
    } else { return; }

    // TODO: Preserve hoisted/recursive function declarations.
    consumeValue(s.expr);

    currCtx.availability[s.symbol] = BindingAvailability::Available;
}

void ReferenceChecker::visit(Return& s) {
    if (s.expr) {
        s.expr->accept(*this);
    }

    // Terminate the control flow path for this function context.
    if (s.expr) {
        consumeValue(s.expr);
    }
    currCtx.reachable = false;
}

void ReferenceChecker::visit(Block& s) {
    for (const StmtPtr& statement : s.statements) {
        statement->accept(*this);
        // If the current path is unreachable, subsequent statements are dead code.
        // e.g. plain return, both branches on an if returning
        if (!currCtx.reachable) return;
    }
}

void ReferenceChecker::visit(If& s) {
    if (!currCtx.reachable) return;
    s.condition->accept(*this);

    // Fork: both branches start AFTER condition evaluation, in this function.
    // Copy the snapshot because both branch analyses need the same input.
    auto before = currCtx;
    s.thenbranch->accept(*this);
    auto thenCtx = std::move(currCtx);

    // Analyze else independently; no else means the unchanged input path.
    currCtx = before;
    if (s.elsebranch) {
        s.elsebranch->accept(*this);
    }
    auto elseCtx = std::move(currCtx);

    /*
    ==================== Branch merge examples ====================
    then: Available    else: Moved        -> MaybeUnavailable
    then: Moved        else: Moved        -> Moved
    then: returned     else: Available    -> Available
    then: returned     else: returned     -> unreachable
    ===============================================================
    */
    // Join only surviving paths. Returned paths cannot reach the next statement.
    currCtx = before;
    // Loop jumps terminate a branch, but their recorded edges must survive it.
    // Each branch includes the original snapshots; append only its new suffix.
    if (thenCtx.loopStack.size() != before.loopStack.size() ||
        elseCtx.loopStack.size() != before.loopStack.size()) {
        throw KMYCompileError("[SERIOUS] Unbalanced loop stack during branch merge.");
    }
    for (size_t i = 0; i < before.loopStack.size(); ++i) {
        auto& target = currCtx.loopStack[i];
        const auto& original = before.loopStack[i];
        for (const auto* branch : {&thenCtx.loopStack[i], &elseCtx.loopStack[i]}) {
            if (branch->breakStates.size() < original.breakStates.size() ||
                branch->continueStates.size() < original.continueStates.size()) {
                throw KMYCompileError("[SERIOUS] Lost loop edges during branch merge.");
            }
            target.breakStates.insert(target.breakStates.end(),
                branch->breakStates.begin() + original.breakStates.size(),
                branch->breakStates.end());
            target.continueStates.insert(target.continueStates.end(),
                branch->continueStates.begin() + original.continueStates.size(),
                branch->continueStates.end());
        }
    }
    currCtx.reachable = thenCtx.reachable || elseCtx.reachable;
    if (!currCtx.reachable) return;

    // Merge only pre-existing bindings; branch-local declarations do not escape.
    // Untracked globals/captures still require explicit registration/policy.
    for (auto& [symbol, state] : currCtx.availability) {
        auto thenIt = thenCtx.availability.find(symbol);
        auto elseIt = elseCtx.availability.find(symbol);

        if ((thenCtx.reachable && thenIt == thenCtx.availability.end()) ||
            (elseCtx.reachable && elseIt == elseCtx.availability.end())) {
            throw KMYCompileError(
                "[SERIOUS] Outer binding missing during branch merge! How can a symbol disappear!!! This must be a memory corruption..."
            );
        }

        if (!thenCtx.reachable) {
            state = elseIt->second;
        } else if (!elseCtx.reachable) {
            state = thenIt->second;
        } else {
            // Disagreement means a later read cannot rely on availability.
            state = thenIt->second == elseIt->second
                ? thenIt->second
                : BindingAvailability::MaybeUnavailable;
        }
    }
}

void ReferenceChecker::visit(While& s) {
    // Dead incoming paths cannot execute this loop.
    if (!currCtx.reachable) return;
    const auto entry = currCtx;      // First-iteration state and enclosing loops.
    auto head = entry.availability;  // Current estimate at condition entry.
    size_t passes = 0;               // Defensive convergence counter.

    /*
    ==================== Loop ownership flow ====================
    head = availability estimate BEFORE the condition, not after the body.
    condition-true -> check body; continue + body fallthrough -> back edges.
    nextHead = merge(entry, previous head, back edges).
    Changed head? Recheck condition/body; unchanged? Finish the analysis.
    Then merge break + condition-false paths into availability AFTER the loop.
    return feeds neither merge. This for-loop analyses states, not KMY execution.
    Keeping the previous head only widens states, so analysis converges.
    =============================================================
    */
    for (;;) {
        // N fixed bindings can each widen once; allow extra settling passes.
        if (++passes > entry.availability.size() + 2) {
            throw KMYCompileError("[SERIOUS] Loop ownership analysis failed to converge.");
        }
        currCtx = entry;                   // Reset path and enclosing jump records.
        currCtx.availability = head;        // Include effects of earlier iterations.
        s.condition->accept(*this);         // Check condition reads/transfers.
        auto afterCondition = currCtx;      // State on condition-to-body/exit edges.

        // Each analysis pass gets fresh jump snapshots, including nested loops.
        currCtx.loopStack.emplace_back();   // break/continue target this loop.
        // A terminating condition has no body path to analyse.
        if (currCtx.reachable) s.body->accept(*this);
        auto loop = std::move(currCtx.loopStack.back()); // Take this pass's jumps.
        currCtx.loopStack.pop_back();       // Restore the enclosing loop target.

        // Back edges: continue paths and reachable normal body completion.
        auto backEdges = std::move(loop.continueStates);
        // Do not count a body ending in break/continue/return as fallthrough.
        if (currCtx.reachable) backEdges.push_back(currCtx.availability);
        backEdges.push_back(entry.availability); // First iteration.
        backEdges.push_back(head);              // Preserve previous widening.
        auto nextHead = mergeAvailability(entry.availability, backEdges); // Outer keys only.
        // Changed estimate: recheck reads that may fail on later iterations.
        if (nextHead != head) {
            head = std::move(nextHead);
            continue;
        }

        // Exit edges: condition-false and break paths; returns never participate.
        // Conservatively allow the false edge even for literal true conditions.
        auto exits = std::move(loop.breakStates);
        // Condition may be false before any iteration; terminating conditions cannot exit here.
        if (afterCondition.reachable) exits.push_back(afterCondition.availability);
        currCtx = entry;                    // Restore function/outer-loop context.
        currCtx.reachable = !exits.empty(); // No exit edge means no following statement.
        if (currCtx.reachable) {
            // Publish availability after the loop, excluding body-local bindings.
            currCtx.availability = mergeAvailability(entry.availability, exits);
        }
        return; // Stable estimate: analysis is complete, not another source iteration.
    }
}

AvailabilityMap ReferenceChecker::mergeAvailability(
    const AvailabilityMap& visible,
    const std::vector<AvailabilityMap>& incoming
) const {
    // No incoming path is an unreachable destination, handled by the caller.
    if (incoming.empty()) {
        throw KMYCompileError("[SERIOUS] Availability merge has no incoming path.");
    }

    AvailabilityMap result;
    result.reserve(visible.size()); // Avoid rehashing while inserting fixed keys.
    // Fixed keys: loop-body locals cannot escape or grow the loop-head map.
    for (const auto& binding : visible) {
        const auto* symbol = binding.first; // Semantic identity, not source spelling.
        // Every participating path must contain each expected outer binding.
        auto readState = [&](const AvailabilityMap& map) {
            auto found = map.find(symbol);
            if (found == map.end()) { // Missing is an invariant failure, not Uninitialized.
                throw KMYCompileError("[SERIOUS] Expected binding missing during availability merge.");
            }
            return found->second;
        };
        auto state = readState(incoming.front()); // Seed from the first path.
        for (size_t i = 1; i < incoming.size(); ++i) {
            // Keep identical states; disagreement conservatively rejects later reads.
            if (state != readState(incoming[i])) {
                state = BindingAvailability::MaybeUnavailable;
            }
        }
        result.emplace(symbol, state); // One merged result per visible binding.
    }
    return result;
}

void ReferenceChecker::visit(Break&) {
    if (currCtx.loopStack.empty()) {
        throw KMYCompileError("[SERIOUS] ReferenceChecker: break has no enclosing loop.");
    }
    if (!currCtx.reachable) return;
    currCtx.loopStack.back().breakStates.push_back(currCtx.availability);
    currCtx.reachable = false; // This path jumps to the loop exit.
}

void ReferenceChecker::visit(Continue&) {
    if (currCtx.loopStack.empty()) {
        throw KMYCompileError("[SERIOUS] ReferenceChecker: continue has no enclosing loop.");
    }
    if (!currCtx.reachable) return;
    currCtx.loopStack.back().continueStates.push_back(currCtx.availability);
    currCtx.reachable = false; // This path jumps to the next iteration.
}

std::optional<Place> ReferenceChecker::resolvePlace(const ExprPtr& expr) {
    switch (expr->kind) {
        case ExprKind::Variable: {
            auto variable = std::static_pointer_cast<Variable>(expr);
            return Place {
                .sym = variable->symbol,
                .type = variable->type,
                .projections = {}
            };
        }
        case ExprKind::ScopeAccess: {
            auto access = std::static_pointer_cast<ScopeAccessExpr>(expr);
            if (!access->symbol) return std::nullopt; // Enum constant, not storage.
            // Imported aliases share the same binding identity as ordinary reads.
            return Place{.sym = access->symbol, .type = access->type, .projections = {}};
        }
        case ExprKind::Index: {
            // e.g. obj.x[3]
            auto index = std::static_pointer_cast<Index>(expr);

            // e.g. Place {.sym = obj, .proj = {FieldProj(...)}}
            auto objPlace = resolvePlace(index->obj);
            
            // The base may be a temporary, such as makeArray(), which this
            // helper does not yet represent as a place.
            if (!objPlace) return std::nullopt;

            // Append this projection info.
            // e.g. Place {.sym = obj, .proj = {FieldProj(...), IndexProj(...)}}
            objPlace->projections.push_back(IndexProjection{
                .indexExpression = index->index, 
                .resultType = index->type
            });

            objPlace->type = index->type;
            return objPlace;
        }
        case ExprKind::Get: {
            // e.g. objs[2].x
            auto get = std::static_pointer_cast<Get>(expr);

            // Methods do not designate field storage and have no fieldIdx.
            if (get->resolvedMethod) return std::nullopt;

            // e.g. Place {.sym = objs, .proj = {IndexProj(...)}}
            auto objPlace = resolvePlace(get->obj);
            
            // Computed object bases, such as makeObject(), are not yet supported.
            if (!objPlace) return std::nullopt;

            // Append this projection info.
            // e.g. Place {.sym = objs, .proj = {IndexProj(...), FieldProj(...)}}
            objPlace->projections.push_back(FieldProjection{
                .fieldIndex = get->fieldIdx, 
                .resultType = get->type
            });

            objPlace->type = get->type;
            return objPlace;
        }
        case ExprKind::UnaryExpr: {
            auto unaryExpr = std::static_pointer_cast<UnaryExpr>(expr);
            if (unaryExpr->op != UnaryOp::Dereference)
                return std::nullopt;
            // e.g. *(arr[2].x)
            // e.g. Place {.sym = arr, .proj = {IndexProj(...), FieldProj(...)}}
            auto opPlace = resolvePlace(unaryExpr->operand);
            
            // Computed pointer bases, such as getPointer(), are not yet supported.
            if (!opPlace) return std::nullopt;

            // Append this projection info.
            // e.g. Place {.sym = arr, .proj = {IndexProj(...), FieldProj(...), DerefProj(...)}}
            opPlace->projections.push_back(DereferenceProjection{
                .resultType = unaryExpr->type
            });

            opPlace->type = unaryExpr->type;
            return opPlace;
        }
        default:
            return std::nullopt;
    }
}
