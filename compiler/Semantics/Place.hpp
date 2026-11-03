#pragma once

#include <variant>
#include <vector>

#include "../Core/AstBaseForward.hpp"

struct VarSymbol;
struct Type;

// Projections describe how to reach storage inside/from the root binding.
struct FieldProjection {
    int fieldIndex = -1;
    Type* resultType = nullptr;
};

struct IndexProjection {
    // Optional original index expression; it does not prove index equality
    // across evaluations. Ownership checking assumes possible overlap.
    ExprPtr indexExpression = nullptr;
    Type* resultType = nullptr;
};

struct DereferenceProjection {
    Type* resultType = nullptr;
};

// Each projection holds exactly one kind, with only that kind's metadata.
using Projection = std::variant<
    FieldProjection,
    IndexProjection,
    DereferenceProjection
>;

struct Place {
    // Null for storage reached through a computed pointer/temporary. A null
    // root cannot be used as a binding-state key or proof of disjoint storage.
    VarSymbol* sym = nullptr;
    Type* type = nullptr; // Type stored at the final location.
    std::vector<Projection> projections;
};
