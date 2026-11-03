#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include "AstBaseForward.hpp"
#include "Scope.hpp"
#include "Symbol.hpp"
#include "VariableStorage.hpp"
#include "operators.hpp"
#include "visitor.hpp"
#include "FunctionContext.hpp"

enum class ExprKind {
    Literal,
    ArrayLiteral,
    RecordLiteral,
    Variable,
    UnaryExpr,
    BinaryExpr,
    Index,
    Call,
    Get,
    ScopeAccess,
    Assignment,
    FunctionExpr,
    ThisExpr,
    NewExpr
};


struct BaseExpr : public ASTNode {
    // This allows the memory management of recursive types since destructor is called
    // when derived objects are destroyed.
    ExprKind kind;
    Type* type = nullptr;
    
    BaseExpr(ExprKind kind) : kind(kind) {}
    
    virtual bool isLValue() const { return false; }
    virtual void accept(Visitor& v) = 0;
    // virtual ExprPtr clone() const = 0;
};

// Double dispatch for visitors.
template <typename Derived, ExprKind k>
struct ExprHelper : public BaseExpr {
    
    ExprHelper() : BaseExpr(k) {}

    void accept(Visitor& v) override {
        traceAstVisit(astNodeName<Derived>);
        v.visit(static_cast<Derived&>(*this));
    }
};

// -----AST nodes for expressions

// Literals for int, double, bool, string, null
struct Literal : ExprHelper<Literal, ExprKind::Literal> {
    ConstValue value;

    Literal();
    Literal(int i);
    Literal(double d);
    Literal(bool b);
    Literal(const std::string& s);
    Literal(std::nullptr_t);

    // ExprPtr clone() const override;
};

struct ArrayLiteral : ExprHelper<ArrayLiteral, ExprKind::ArrayLiteral> {
    std::vector<ExprPtr> elements;

    ArrayLiteral(std::vector<ExprPtr> elements);

    // ExprPtr clone() const override;
};

struct RecordLiteral : ExprHelper<RecordLiteral, ExprKind::RecordLiteral> {
    std::vector<std::pair<std::string, ExprPtr>> fields;
    std::unordered_map<std::string, int> layout; // field name to index mapping for codegen

    RecordLiteral(std::vector<std::pair<std::string, ExprPtr>> fields);
    // ExprPtr clone() const override;
};

struct Variable : ExprHelper<Variable, ExprKind::Variable> { 
    std::string name;
    
    bool resolved = false;
    ResolvedVar resolution;

    VarSymbol* symbol = nullptr; // Semantic info. Initially empty.

    // A variable names a storage location, so it is addressable.
    bool isLValue() const override { return true; }
    
    Variable(const std::string& name);

    // ExprPtr clone() const override;
};

struct UnaryExpr : ExprHelper<UnaryExpr, ExprKind::UnaryExpr>{
    UnaryOp op;
    ExprPtr operand;   

    // Dereferencing produces a storage location; other unary operators
    // produce temporary values.
    bool isLValue() const override { return op == UnaryOp::Dereference; }

    UnaryExpr(UnaryOp op, ExprPtr operand);

    // ExprPtr clone() const override;
};

struct BinaryExpr : ExprHelper<BinaryExpr, ExprKind::BinaryExpr> {
    BinaryOp op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpr(BinaryOp op, ExprPtr left, ExprPtr right);

    // ExprPtr clone() const override;
};

// Format: obj '[' index ']'
struct Index : ExprHelper<Index, ExprKind::Index> {
    ExprPtr obj;
    ExprPtr index; 

    // Indexing designates an element in the underlying array storage.
    bool isLValue() const override { return true; }

    Index(ExprPtr obj, ExprPtr index);

    // ExprPtr clone() const override;
};

// Format: func '(' args ')'
struct Call : ExprHelper<Call, ExprKind::Call> {
    ExprPtr func;
    std::vector<ExprPtr> args; 

    Call(ExprPtr func, std::vector<ExprPtr> args);

    // ExprPtr clone() const override;
};


// Format: obj '.' name
struct Get : ExprHelper<Get, ExprKind::Get> {
    ExprPtr obj;
    std::string name; // TODO change all these string fields to TOKENS for debugging.

    int fieldIdx; // For codegen, set by Resolver. Only for fields.
    int methodIdx; // For codegen and lowering, set by Resolver. Only for methods.
    
    // For lowering. Check if its a form of obj.f (obj is aggregate, f is method)
    bool resolvedMethod = false;

    // Property access can designate a field storage location. Semantic
    // analysis will reject method access when it is used as an lvalue.
    bool isLValue() const override { return true; }

    Get(ExprPtr obj, const std::string& name);

    // ExprPtr clone() const override;
};

// Format: obj '::' name
struct ScopeAccessExpr : ExprHelper<ScopeAccessExpr, ExprKind::ScopeAccess> {
    std::vector<std::string> parts;
    // Set for an imported value such as `math::sqrt`. Enum variants still use
    // accessIdx because they lower to compile-time integer ordinals.
    VarSymbol* symbol = nullptr;
    int accessIdx = INVALID_SLOT;

    // TODO: later extend this for static fields.

    ScopeAccessExpr(std::vector<std::string> parts);

    // ExprPtr clone() const override;
};

// Surprisingly, assignment can be an expression.
struct Assignment : ExprHelper<Assignment, ExprKind::Assignment> {
    AssignmentOp op; // e.g., +=, -=, etc.
    ExprPtr left;
    ExprPtr right;
    // True only for compiler-generated stores of declaration initializers,
    // which may write an immutable field exactly once during construction.
    bool isInitialisation = false;

    Assignment(AssignmentOp op, ExprPtr left, ExprPtr right);

    // ExprPtr clone() const override;
};

// Parameter struct is in ASTBaseForward.hpp
struct Parameter {
    TypeNodePtr type;
    std::string name;
    bool isVariadic = false;
    bool isMutable = false; // Immutable unless declared with `var`.
    bool defaultExists = false;

    bool implicitThis = false; // For methods and arg check

    ExprPtr defaultValue;
    VarSymbol* symbol = nullptr;

    Parameter(TypeNodePtr type, std::string name, bool isVariadic, bool isMutable, bool defaultExists, ExprPtr defaultValue=nullptr)
        : 
        type(std::move(type)),
        name(move(name)),
        isVariadic(isVariadic),
        isMutable(isMutable), 
        defaultExists(defaultExists),
        defaultValue(move(defaultValue)) {}
};

// Kinda like lambda fun
struct FunctionExpr : ExprHelper<FunctionExpr, ExprKind::FunctionExpr> {
    std::vector<Parameter> params;
    StmtPtr body;
    TypeNodePtr annotatedReturnType;

    Scope* scope = nullptr;

    // Closure analysis result
    FunctionContext* functionContext = nullptr;

    // For stack VM codegen
    int fnProtoIdx = INVALID_SLOT;

    // For IR codegen
    int functionId = INVALID_SLOT; 
    bool isEntry = false;

    // Set for the compiler-generated function behind an aggregate init(...).
    // Constructors return their implicit `this` when control reaches the end.
    bool isConstructor = false;

    FunctionExpr(const std::vector<Parameter>& params, StmtPtr body, TypeNodePtr annotatedReturnType, bool isEntry=false);

    // ExprPtr clone() const override;
};

using FunctionExprPtr = std::shared_ptr<FunctionExpr>;

struct ThisExpr : ExprHelper<ThisExpr, ExprKind::ThisExpr> {
    bool resolved = false;
    ResolvedVar resolution; // WARNING: Will contain garbage when uninitialised!!!
    VarSymbol* symbol = nullptr;
    // ExprPtr clone() const override;
};

struct NewExpr : ExprHelper<NewExpr, ExprKind::NewExpr> {
    std::string typeName;
    // Complete parsed type for named allocations. `typeName` remains as the
    // legacy lookup key until NewExpr semantic resolution is migrated.
    TypeNodePtr allocatedType;
    TypeNodePtr arrayType;
    ExprPtr arraySize;
    std::vector<ExprPtr> args;

    NewExpr(std::string typeName, std::vector<ExprPtr> args, TypeNodePtr allocatedType = nullptr);
    NewExpr(TypeNodePtr arrayType);
    NewExpr(TypeNodePtr arrayType, ExprPtr arraySize);

    // ExprPtr clone() const override;
};

// -----AST nodes for statements
struct BaseStmt : public ASTNode {
    virtual ~BaseStmt() = default;
    virtual void accept(Visitor& v) = 0;

    // virtual StmtPtr clone() const = 0;
};

template <typename Derived>
struct StmtHelper : public BaseStmt {
    void accept(Visitor& v) override {
        traceAstVisit(astNodeName<Derived>);
        v.visit(static_cast<Derived&>(*this));
    }
};

struct Print : StmtHelper<Print> { 
    ExprPtr expr; 

    Print(ExprPtr expr);

    // StmtPtr clone() const override;
};

struct If : StmtHelper<If> {
    ExprPtr condition;
    StmtPtr thenbranch;
    StmtPtr elsebranch; // Optional else branch

    If(ExprPtr condition, StmtPtr thenbranch, StmtPtr elsebranch);

    // StmtPtr clone() const override;
};

struct While : StmtHelper<While> {
    ExprPtr condition;
    StmtPtr body;

    While(ExprPtr condition, StmtPtr body);

    // StmtPtr clone() const override;
};

struct Break : StmtHelper<Break> {
    // StmtPtr clone() const override;
};
struct Continue : StmtHelper<Continue> {
    // StmtPtr clone() const override;
};

struct Block : StmtHelper<Block> {
    std::vector<StmtPtr> statements;

    Scope* scope = nullptr;

    Block(std::vector<StmtPtr> statements);

    // StmtPtr clone() const override;
};

struct Let : StmtHelper<Let> {
    TypeNodePtr annotatedType;
    std::string name;
    ExprPtr expr;
    bool isMutable;
    bool isFunctionDecl;
    
    VarSymbol* symbol = nullptr;

    Let(
        TypeNodePtr annotatedType,
        const std::string& name, 
        ExprPtr expr, 
        bool isMutable,
        bool isFunctionDecl
    );
    // StmtPtr clone() const override;
};

struct Return : StmtHelper<Return> {
    ExprPtr expr; // Optional.
    
    Return(ExprPtr expr);
    // StmtPtr clone() const override; 
};

struct Member {
    virtual ~Member() = default;
    VarSymbol* symbol = nullptr;
};

struct FieldMember : Member {
    std::string name;
    ExprPtr initialiser; // Optional. but wont be used from now on (2026-06-62)
    bool isMutable;
    TypeNodePtr annotatedType;

    FieldMember(TypeNodePtr annotatedType, std::string name, ExprPtr initialiser, bool isMutable);
};

struct MethodMember : Member {
    std::string name;
    FunctionExprPtr methodExpr;

    MethodMember(std::string name, FunctionExprPtr methodExpr);    
};

struct ConstructorMember : Member {
    FunctionExprPtr initFuncExpr;

    ConstructorMember(FunctionExprPtr initFuncExpr);    
};

struct EnumMember : Member {
    std::shared_ptr<Enum> customEnum;

    //TypeSymbol* typeSymbol = nullptr;

    EnumMember(std::shared_ptr<Enum> customEnum);
};


// Same syntax and semantics for records and classes.
enum class AggregateKind {
    RECORD,
    CLASS
};

struct Aggregate : StmtHelper<Aggregate> {
    AggregateKind kind;
    
    std::string name;
    std::vector<FieldMember> fieldMembers; // fields 
    std::vector<MethodMember> methodMembers; // methods
    std::vector<ConstructorMember> constructorMembers; // constructor functions
    std::vector<EnumMember> enumMembers; // Enum definitions
    
    FunctionExprPtr fieldInitFunc; // field initialisers.

    VarSymbol* thisSymbol = nullptr;
    TypeSymbol* typeSymbol = nullptr;

    Scope* scope = nullptr;
    int fieldCount = 0;
    Aggregate (
        AggregateKind kind,
        std::string name,
        std::vector<FieldMember> fieldMembers, 
        std::vector<MethodMember> methodMembers,
        std::vector<ConstructorMember> constructorMembers,
        std::vector<EnumMember> enumMembers,
        FunctionExprPtr fieldInitFunc
    );
};

struct TypeAlias : StmtHelper<TypeAlias> {
    std::string name;
    TypeSymbol* typeSymbol = nullptr;
    TypeNodePtr aliasingType;

    TypeAlias(std::string name, TypeNodePtr aliasingType);
};

struct Enum : StmtHelper<Enum> {
    std::string name;
    std::vector<std::string> variants;
    
    TypeSymbol* typeSymbol = nullptr;

    Enum(std::string name, std::vector<std::string> variants);
};

struct ExprStmt : StmtHelper<ExprStmt> {
    ExprPtr expr;
    ExprStmt(ExprPtr expr);
    // StmtPtr clone() const override;
};

// -----AST nodes for declarations
struct BaseDecl : ASTNode {
    virtual ~BaseDecl() = default;
    virtual void accept(Visitor& v) = 0;
};

template <typename Derived>
struct DeclHelper : BaseDecl {
    void accept(Visitor& v) override {
        v.visit(static_cast<Derived&>(*this));
    }
};

// TO BE CONSIDERED: Move decls to separate wrappers. DeclStmt (Why? just for semantic categorisation.)

// std::variant is a type-safe union that can hold one of several types.

