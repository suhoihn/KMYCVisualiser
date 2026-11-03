#include "TypeChecker.hpp"

#include "../Core/errorhandler.hpp"
#include "TypeInterner.hpp"
#include <unordered_map>

static std::unordered_map<ASTNode*, Type*> typeInfo;
static std::unordered_map<ASTNode*, SymbolPtr> symbolInfo;

static Type* getType(ASTNode* n) {
    auto it = typeInfo.find(n);

    if (it == typeInfo.end())
        return nullptr;

    return it->second;
}

static void setType(ASTNode* n, Type* t) {
    typeInfo[n] = t;
}

TypeChecker::TypeChecker(const FunctionExprPtr program)
    : program(program) {}

void TypeChecker::check() {
    program->accept(*this);
}

static bool isAssignable(Type* from, Type* to) {
    if (to == &Types::ANY_TYPE) {
        return true;
    }
    
    if (from == &Types::INT_TYPE && to == &Types::DOUBLE_TYPE) {
        return true; // int can be assigned to double.
    }

    if (from == &Types::NULL_TYPE && to->kind == TypeKind::AGGREGATE) {
        return true; // null can be assigned to record types.
    }
    return from == to;
}

Type* typeSigToType(const TypeNodePtr& type) {
    switch (type->kind) {
        case TypeNodeKind::NAMED: {
            const NamedTypeNode& named = static_cast<NamedTypeNode&>(*type);
            if (named.name == "int") {
                return &Types::INT_TYPE;
            } else if (named.name == "double") {
                return &Types::DOUBLE_TYPE;
            } else if (named.name == "bool") {
                return &Types::BOOL_TYPE;
            } else if (named.name == "string") {
                return &Types::STRING_TYPE;
            } else if (named.name == "null") {
                return &Types::NULL_TYPE;
            } else if (named.name == "void") {
                return &Types::VOID_TYPE;
            } else if (named.name == "any") {
                return &Types::ANY_TYPE;
            } else {
                throw KMYCompileError("Unknown type name: " + named.name);
            }
        }
        case TypeNodeKind::FUNCTION: {
            const FunctionTypeNode& named = static_cast<FunctionTypeNode&>(*type);
            std::vector<Type*> paramTypes;
            for (const auto& param : named.params) {
                paramTypes.push_back(typeSigToType(param));
            }
            Type* returnType = typeSigToType(named.returnType);
            // TODO: Raw. new. memory leek guaraneed.
            return new FunctionType(paramTypes, returnType);
        }
        case TypeNodeKind::SHARED: {
            const SharedTypeNode& shared = static_cast<SharedTypeNode&>(*type);
            return TypeInterner::getSharedType(typeSigToType(shared.innerType));
        }
    }

    throw KMYCompileError("Unknown type node kind.");
}

void TypeChecker::visit(Literal& e) {
    e.type = std::visit([](auto&& value) -> Type* {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, int>)
            return &Types::INT_TYPE;
        else if constexpr (std::is_same_v<T, double>)
            return &Types::DOUBLE_TYPE;
        else if constexpr (std::is_same_v<T, bool>)
            return &Types::BOOL_TYPE;
        else if constexpr (std::is_same_v<T, std::string>)
            return &Types::STRING_TYPE;
        else if constexpr (std::is_same_v<T, std::nullptr_t>)
            return &Types::NULL_TYPE;
        else
            return nullptr;

    }, e.value);
}
void TypeChecker::visit(ArrayLiteral& e) {
    Type* baseType = nullptr;
    for (auto& elem : e.elements) {
        elem->accept(*this);
        if (baseType) {
            if (baseType != elem->type && elem->type != &Types::ANY_TYPE) {
                throw KMYCompileError("Unmatched type.");
            }
        } else {
            baseType = elem->type;
        }
    }
    // TODO: Disgusting memory leak.
    // nullptr type for array means empty array.
    if (e.elements.empty()) {
        if (expectedType->kind != TypeKind::ARRAY) {
            throw KMYCompileError("Expected array type!!!");
        }
        auto arrayType = static_cast<ArrayType*>(expectedType);
        e.type = new ArrayType(arrayType->elementType);
        return;
    }

    e.type = new ArrayType(baseType);
}
void TypeChecker::visit(RecordLiteral& e) {}

void TypeChecker::visit(Variable& e) {
    if (!e.symbol) {
        throw KMYCompileError("CRITICAL: Unresolved variable. Should be resolved in pass 2.");
    }

    if (false) {// !e.symbol->initialised) {
        // Uses of uninitialised variables.
        throw KMYCompileError("Uses of uninitialised variable.");
    }
    
    e.type = e.symbol->type;
}

Type* testBinary(BinaryOp op, Type* leftType, Type* rightType) {
    switch (op) {
        case BinaryOp::Plus:
        case BinaryOp::Minus:
        case BinaryOp::Star:
        case BinaryOp::Slash: {
            if (leftType == &Types::INT_TYPE && rightType == &Types::INT_TYPE) {
                return &Types::INT_TYPE;
            } else if (leftType == &Types::INT_TYPE && rightType == &Types::DOUBLE_TYPE) {
                return &Types::DOUBLE_TYPE;
            } else if (leftType == &Types::DOUBLE_TYPE && rightType == &Types::INT_TYPE) {
                return &Types::DOUBLE_TYPE;
            } else if (leftType == &Types::DOUBLE_TYPE && rightType == &Types::DOUBLE_TYPE) {
                return &Types::DOUBLE_TYPE;
            }
        }



        case BinaryOp::Percent: {
            if (leftType == &Types::DOUBLE_TYPE) {
                throw std::runtime_error("Divident cannot be double.");
            }

            if (rightType == &Types::DOUBLE_TYPE) {
                throw std::runtime_error("Divisor cannot be double.");
            }

            // C-style: integer modulo only
            return &Types::INT_TYPE;
        }
        // TODO: Combine?
        case BinaryOp::LShift: {
            if (leftType == &Types::DOUBLE_TYPE || rightType == &Types::DOUBLE_TYPE) {
                throw std::runtime_error("Invalid operand.");
            }
            return &Types::INT_TYPE;
        }

        case BinaryOp::RShift: {
            if (leftType == &Types::DOUBLE_TYPE || rightType == &Types::DOUBLE_TYPE) {
                throw std::runtime_error("Invalid operand.");
            }
            return &Types::INT_TYPE;
        }

        case BinaryOp::BitAnd: {
            if (leftType == &Types::DOUBLE_TYPE || rightType == &Types::DOUBLE_TYPE) {
                throw std::runtime_error("Invalid operand.");
            }
            return &Types::INT_TYPE;
        }

        case BinaryOp::BitOr: {
            if (leftType == &Types::DOUBLE_TYPE || rightType == &Types::DOUBLE_TYPE) {
                throw std::runtime_error("Invalid operand.");
            }
            return &Types::INT_TYPE;
        }

        case BinaryOp::BitXor:{
            if (leftType == &Types::DOUBLE_TYPE || rightType == &Types::DOUBLE_TYPE) {
                throw std::runtime_error("Invalid operand.");
            }
            return &Types::INT_TYPE;
        }


        // ========================
        // LOGICAL
        // ========================

        // Following C style permissivity.
        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr:
            return &Types::BOOL_TYPE;

        // ========================
        // COMPARISON
        // ========================

        // Equality following modern language strictness.
        case BinaryOp::EqualEqual:
        case BinaryOp::NotEqual:
        case BinaryOp::Greater:
        case BinaryOp::GreaterEqual:
        case BinaryOp::Less:
        case BinaryOp::LessEqual: {
            if (leftType != rightType) {
                throw std::runtime_error("Cannot compare different types.");
            }
            return &Types::BOOL_TYPE;
        }
        default:
            throw std::runtime_error("Unsupported binary operator");
    }
}



void TypeChecker::visit(BinaryExpr& e) {
    e.left->accept(*this);
    e.right->accept(*this);

    e.type = testBinary(e.op, e.left->type, e.right->type);
}

void TypeChecker::visit(UnaryExpr& e) {
    e.operand->accept(*this);
    if (e.operand->type != &Types::INT_TYPE) {
        throw std::runtime_error("Unary operator only supports int operand... for now.");
    }
    e.type = e.operand->type; // Int.
}

void TypeChecker::visit(Assignment& e) {   
    if (!e.left->isLValue()) {
        throw KMYCompileError("Left-hand side of assignment must be assignable.");
    }
    e.left->accept(*this);
    
    Type* oldET = expectedType;
    expectedType = e.left->type;

    e.right->accept(*this);

    expectedType = oldET;

    if (!isAssignable(e.right->type, e.left->type)) {
        // TODO: Covariant and contravariant type checking.
        throw KMYCompileError("Type mismatch in assignment.");
    }
}

void TypeChecker::visit(Index& e) {
    e.obj->accept(*this);
    e.index->accept(*this);

    if (e.obj->type->kind != TypeKind::ARRAY) {
        throw KMYCompileError("Only arrays can be indexed... for now.");
    }
    if (e.index->type != &Types::INT_TYPE) {
        throw KMYCompileError("Index must be an integer.");
    }

    e.type = static_cast<ArrayType*>(e.obj->type)->elementType;
}

void TypeChecker::visit(Call& e) {
    e.func->accept(*this);
    if (e.func->type->kind != TypeKind::FUNCTION) {
        throw KMYCompileError("Uncallable object.");
    }

    auto fn = std::static_pointer_cast<FunctionExpr>(e.func);

    int requiredCnt = 0;
    bool isVariadic = false;
    for (const auto& param : fn->params) {
        if (!param.defaultExists) {
            requiredCnt++;
        }
        if (param.isVariadic) {
            isVariadic = true;
        }
    }

    // Min check
    if (e.args.size() < static_cast<size_t>(requiredCnt)) {
        throw KMYCompileError("Not enough arguments provided.");
    }

    // Max check
    if (!isVariadic && e.args.size() > fn->params.size()) {
        throw KMYCompileError("Too many arguments provided.");
    }

    for (size_t i = 0; i < e.args.size(); ++i) {
        e.args[i]->accept(*this);
        if (i < fn->params.size()) {
            if (e.args[i]->type != fn->params[i].symbol->type) {
                throw KMYCompileError("Argument type mismatch.");
            }
        } else {
            // Variadic args.
            if (e.args[i]->type != fn->params.back().symbol->type) {
                throw KMYCompileError("Argument type mismatch.");
            }
        }
    }

    if (fn->annotatedReturnType) {
        e.type = typeSigToType(fn->annotatedReturnType);
    } else {
        e.type = &Types::ANY_TYPE; // No annotation means we don't know the return type. Assume any.
    }
}

void TypeChecker::visit(Get& e) {
    throw KMYCompileError("Property access not supported yet.");
    e.obj->accept(*this);
}


void TypeChecker::visit(FunctionExpr& e) {
    for (auto& param : e.params) {
        if (!param.type) {
            // No annotation means we don't know the type. Assume any.
            // We delegate this in runtime.
            param.symbol->type = &Types::ANY_TYPE;

            // If strict mode:
            // throw KMYCompileError("Parameter must have explicit type signature.");
        }

        param.symbol->type = typeSigToType(param.type);
        if (param.defaultExists) {
            param.defaultValue->accept(*this);
            if (param.defaultValue->type != param.symbol->type) {
                throw KMYCompileError("Default value type mismatch.");
            }
        }
    }
    e.body->accept(*this);
}

void TypeChecker::visit(ThisExpr& e) {
    throw KMYCompileError("Not yet.");
}

void TypeChecker::visit(NewExpr& e) {
    throw KMYCompileError("Object instantiation not supported yet.");
}

void TypeChecker::visit(Print& s) {
    s.expr->accept(*this);
}

void TypeChecker::visit(If& s) {
    s.condition->accept(*this);
    s.thenbranch->accept(*this);
    if (s.elsebranch) {
        s.elsebranch->accept(*this);
    }
}

void TypeChecker::visit(While& s) {
    s.condition->accept(*this);
    s.body->accept(*this);
}

void TypeChecker::visit(Block& s) {
    for (auto& stmt : s.statements) {
        stmt->accept(*this);
    }
}

void TypeChecker::visit(Break& s) {}
void TypeChecker::visit(Continue& s) {}

void TypeChecker::visit(Let& s) {

    Type* annotated = nullptr;

    if (s.annotatedType) {
        annotated = typeSigToType(s.annotatedType);

        if (annotated == &Types::VOID_TYPE) {
            throw KMYCompileError("Variable cannot be of type void.");
        }
    }

    Type* inferred = nullptr;

    if (s.expr) {
        s.expr->accept(*this);
        inferred = s.expr->type;
    }

    // CASE 1: annotated type exists
    if (annotated) {

        if (inferred && !isAssignable(inferred, annotated)) {
            throw KMYCompileError("Type mismatch in initializer.");
        }

        s.symbol->type = annotated;
    }

    // CASE 2: no annotation → infer
    else {
        if (!inferred) {
            throw KMYCompileError("Cannot infer type of uninitialised variable.");
        }

        s.symbol->type = inferred;
    }
}

void TypeChecker::visit(Return& s) {
    if (s.expr) {
        s.expr->accept(*this);
    }
}

void TypeChecker::visit(Aggregate& s) {
    throw KMYCompileError("Classes not supported yet. since it is damn hard.");
}

void TypeChecker::visit(ExprStmt& s) {
    s.expr->accept(*this);
}
