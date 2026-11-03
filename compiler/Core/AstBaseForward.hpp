#pragma once
#include <memory>
#include <variant>
#include <unordered_map>
#include "type.hpp"

struct ASTNode {
    int traceId = -1; // Assigned only while recording a front-end analysis.
    virtual ~ASTNode() = default;
};

using ASTNodePtr = std::shared_ptr<ASTNode>;

using ExprPtr = std::shared_ptr<struct BaseExpr>;
using StmtPtr = std::shared_ptr<struct BaseStmt>;

struct Value;
using NativeFnPtr = Value(*)(int argc, Value* args);

using ConstValue = std::variant<
    std::nullptr_t,
    int,
    double,
    bool,
    std::string
>;

// No runtime heap-allocated value. (For compiler's constantMap)
// Bascially, PRIMITIVES.
struct GarbageValue{};
