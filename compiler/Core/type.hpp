#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <optional>

// Forward decls
struct VarSymbol;
struct TypeSymbol;

enum class TypeNodeKind {
    NAMED,
    FUNCTION,
    ARRAY,
    RECORD, // Annonymous records ({x: int} forms). Will eventually be StructualType
    SCOPED, // For types with a scope (e.g., Foo::Bar. Notice that this is different from enum access like Color::Black)
    POINTER,
    SHARED,
    NULLABLE
};

struct TypeNode {
    TypeNodeKind kind;
    TypeNode(TypeNodeKind kind) : kind(kind) {}
    virtual ~TypeNode() = default;
};

using TypeNodePtr = std::shared_ptr<TypeNode>;

struct NamedTypeNode : TypeNode {
    std::string name;

    NamedTypeNode(const std::string& name) 
        : TypeNode(TypeNodeKind::NAMED), name(name) {}
};

struct FunctionTypeNode : TypeNode {
    std::vector<TypeNodePtr> params;
    TypeNodePtr returnType;

    FunctionTypeNode(
        std::vector<TypeNodePtr> params_,
        TypeNodePtr returnType_
    )
        : TypeNode(TypeNodeKind::FUNCTION),
          params(std::move(params_)),
          returnType(std::move(returnType_))
    {}
};

struct RecordTypeNode : TypeNode {
    std::vector<std::pair<std::string, TypeNodePtr>> paramTypePairs;

    RecordTypeNode(
        std::vector<std::pair<std::string, TypeNodePtr>> paramTypePairs
    ) : TypeNode(TypeNodeKind::RECORD), paramTypePairs(std::move(paramTypePairs)) {}
};

struct ArrayTypeNode : TypeNode {
    TypeNodePtr elementType;
    int size;
    bool isSizeDetermined;
    bool isDynamic;

    ArrayTypeNode(TypeNodePtr elementType, int size, bool isSizeDetermined, bool isDynamic)
        : TypeNode(TypeNodeKind::ARRAY), elementType(std::move(elementType)), size(size), isSizeDetermined(isSizeDetermined), isDynamic(isDynamic) {}
};

struct ScopedTypeNode : TypeNode {
    std::vector<std::string> scopeParts;

    ScopedTypeNode(std::vector<std::string> scopeParts)
        : TypeNode(TypeNodeKind::SCOPED), scopeParts(std::move(scopeParts)) {}
};

struct PointerTypeNode : TypeNode {
    TypeNodePtr pointee;

    explicit PointerTypeNode(TypeNodePtr pointee)
        : TypeNode(TypeNodeKind::POINTER), pointee(std::move(pointee)) {}
};

// `shared T` is a real type wrapper, not a flag attached to arbitrary nodes.
// This keeps nested forms such as `shared Foo?` structurally unambiguous.
struct SharedTypeNode : TypeNode {
    TypeNodePtr innerType;

    explicit SharedTypeNode(TypeNodePtr inner)
        : TypeNode(TypeNodeKind::SHARED), innerType(std::move(inner)) {}
};

enum class TypeKind {
    INT,
    DOUBLE,
    BOOL,
    POINTER, // Source-level T* pointers and internal codegen pointers.
    SHARED, // RC-managed ownership wrapper around another semantic type.
    CELL, // NOTE: Only used in IR code gen! (TODO: Refactor to IRType or smth)
    STRING,
    NULLTYPE,
    ARRAY,
    STRUCTUAL, // annonymous records {...}
    INSTANCE, // record or class instances
    FUNCTION, // Just denotes that the type is a function. 
    VOID, // Denotes no type. Only used for functions.
    ENUM,
    ANY,
    UNKNOWN,
    UNINITIALISED, // TODO: not rly a type, should be separate bool flag?
    NULLABLE
};

struct Type {
    const TypeKind kind;
    virtual ~Type() = default;
    
    Type(TypeKind kind) : kind(kind) {}
};

struct PointerType : Type {
    Type* pointee;

    PointerType(Type* pointee)
        : Type(TypeKind::POINTER),
          pointee(pointee) {}
};

// Canonical semantic representation of `shared T`. The wrapper is distinct
// from T so one use can be shared without mutating T's interned type object.
struct SharedType : Type {
    Type *innerType;

    explicit SharedType(Type *innerType)
        : Type(TypeKind::SHARED),
          innerType(innerType) {}
};

// Temporary!
struct CellType : Type {
    Type* pointee;

    CellType(Type* pointee)
        : Type(TypeKind::CELL),
          pointee(pointee) {}
};

// TODO: to symbol maybe?
struct ParamTypeInfo {
    bool hasDefault;
    bool isVariadic;
    bool implicitThis;
    Type* type;
};



struct FunctionType : Type {
    std::vector<Type*> paramTypes;
    Type* returnType;
    bool isNative = false;

    // TODO: Move this to varsymbol*! not here!
    std::vector<ParamTypeInfo> info;
    bool infoExists = false;

    // No parameter info exists.
    FunctionType(std::vector<Type*> paramTypes, Type* returnType, bool isNative = false) 
        : Type(TypeKind::FUNCTION), paramTypes(move(paramTypes)), returnType(returnType), isNative(isNative), infoExists(false) {}
 
    // Parameter info exists.
    FunctionType(std::vector<Type*> paramTypes, std::vector<ParamTypeInfo> info, Type* returnType, bool isNative = false) 
        : Type(TypeKind::FUNCTION), paramTypes(move(paramTypes)), info(move(info)), returnType(returnType), isNative(isNative), infoExists(true) {}
};

struct ArrayType : Type {
    Type* elementType;
    std::optional<size_t> fixedLength;

    ArrayType(Type* elementType, std::optional<size_t> fixedLength = std::nullopt)
        : Type(TypeKind::ARRAY), elementType(elementType), fixedLength(fixedLength) {}
};

struct NullableType : Type {
    Type* innerType;
    explicit NullableType(Type* inner) : Type(TypeKind::NULLABLE), innerType(inner) {}
};

struct NullableTypeNode : TypeNode {
    TypeNodePtr innerType;
    explicit NullableTypeNode(TypeNodePtr inner)
        : TypeNode(TypeNodeKind::NULLABLE), innerType(std::move(inner)) {}
};


struct StructualType : Type {
    std::unordered_map<std::string, Type*> fieldTypes;
    std::unordered_map<std::string, int> layout;

    StructualType() : Type(TypeKind::STRUCTUAL) {}
    StructualType(std::unordered_map<std::string, Type*> fieldTypes, std::unordered_map<std::string, int> layout)
        : Type(TypeKind::STRUCTUAL), fieldTypes(move(fieldTypes)), layout(move(layout)) {}
};

// TODO: Use those instead of symbols later...
// RLY TODO i hate parallel vectors like there(^^^^^^^^)
struct FieldInfo {
    int offset;
    Type* type;
};

struct MethodInfo {
    int fnProtoIdx;
    Type* type;
};

struct InstanceType : Type {
    std::unordered_map<std::string, VarSymbol*> fieldMap;
    std::unordered_map<std::string, VarSymbol*> methodMap;
    std::vector<VarSymbol*> constructorVec;
    std::unordered_map<std::string, TypeSymbol*> enumMap;

    // Native code generation metadata.  These are assigned once by closure
    // analysis, then remain usable when another module constructs this type.
    FunctionType* fieldInitializerType = nullptr;
    // `type.hpp` is included before Ast.hpp declares INVALID_SLOT.
    int fieldInitializerFunctionId = -1;
    std::string name = "<UNDEFINED>"; // Purely for debug. trust me.

    InstanceType() : Type(TypeKind::INSTANCE) {}

    InstanceType(
        std::unordered_map<std::string, VarSymbol*> fieldMap,
        std::unordered_map<std::string, VarSymbol*> methodMap,
        std::vector<VarSymbol*> constructorVec,
        std::unordered_map<std::string, TypeSymbol*> enumMap
    ) : 
        Type(TypeKind::INSTANCE),
        fieldMap(move(fieldMap)),
        methodMap(move(methodMap)),
        constructorVec(move(constructorVec)),
        enumMap(move(enumMap)) {}
};

struct EnumType : Type {
    std::unordered_map<std::string, int> variantMap;

    EnumType() : Type(TypeKind::ENUM) {}

    EnumType(std::unordered_map<std::string, int> variantMap) 
        : Type(TypeKind::ENUM), variantMap(std::move(variantMap)) {}
};

namespace Types {
    inline Type INT_TYPE = { TypeKind::INT };
    inline Type DOUBLE_TYPE = { TypeKind::DOUBLE };
    inline Type BOOL_TYPE = { TypeKind::BOOL };
    inline Type STRING_TYPE = { TypeKind::STRING };
    inline Type NULL_TYPE = { TypeKind::NULLTYPE };
    inline Type VOID_TYPE = { TypeKind::VOID };
    inline Type ANY_TYPE = { TypeKind::ANY };
    inline Type UNINITIALISED = { TypeKind::UNINITIALISED };
};
