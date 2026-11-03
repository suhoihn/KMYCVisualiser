#include "Ast.hpp"

#include <string>
#include <vector>
#include "operators.hpp"

// -----Expressions
Literal::Literal() : value(nullptr) {}
Literal::Literal(int i) : value(i) {}
Literal::Literal(double d) : value(d) {}
Literal::Literal(bool b) : value(b) {}
Literal::Literal(const std::string& s) : value(s) {}
Literal::Literal(std::nullptr_t) : value(nullptr) {}

ArrayLiteral::ArrayLiteral(std::vector<ExprPtr> elements) : elements(move(elements)) {}
RecordLiteral::RecordLiteral(std::vector<std::pair<std::string, ExprPtr>> fields) : fields(move(fields)) {}
Variable::Variable(const std::string& name) : name(name) {}
UnaryExpr::UnaryExpr(UnaryOp op, ExprPtr operand) : op(op), operand(move(operand)) {}
BinaryExpr::BinaryExpr(BinaryOp op, ExprPtr left, ExprPtr right) : op(op), left(move(left)), right(move(right)) {}
Assignment::Assignment(AssignmentOp op, ExprPtr left, ExprPtr right) : op(op), left(move(left)), right(move(right)) {}
Index::Index(ExprPtr obj, ExprPtr index) : obj(move(obj)), index(move(index)) {}
Call::Call(ExprPtr func, std::vector<ExprPtr> args) : func(move(func)), args(move(args)) {}
Get::Get(ExprPtr obj, const std::string& name) : obj(move(obj)), name(name) {}
ScopeAccessExpr::ScopeAccessExpr(std::vector<std::string> parts): parts(parts) {}
FunctionExpr::FunctionExpr(const std::vector<Parameter>& params, StmtPtr body, TypeNodePtr annotatedReturnType, bool isEntry) 
    : params(move(params)), body(move(body)), annotatedReturnType(std::move(annotatedReturnType)), isEntry(isEntry) {}
// Nothing for ThisLiteral.
NewExpr::NewExpr(std::string typeName, std::vector<ExprPtr> args, TypeNodePtr allocatedType)
    : typeName(move(typeName)), allocatedType(move(allocatedType)), args(move(args)) {}
NewExpr::NewExpr(TypeNodePtr arrayType)
    : arrayType(move(arrayType)) {}
NewExpr::NewExpr(TypeNodePtr arrayType, ExprPtr arraySize)
    : arrayType(move(arrayType)), arraySize(move(arraySize)) {}

// -----Statements
Print::Print(ExprPtr expr) : expr(move(expr)) {}
If::If(ExprPtr condition, StmtPtr thenbranch, StmtPtr elsebranch=nullptr) 
    : condition(move(condition)), thenbranch(move(thenbranch)), elsebranch(move(elsebranch)) {}
While::While(ExprPtr condition, StmtPtr body) 
    : condition(move(condition)), body(move(body)) {}
// Nothing for Break
// Nothing for Continue
Block::Block(std::vector<StmtPtr> statements) : statements(move(statements)) {}
Let::Let(TypeNodePtr annotatedType, const std::string& name, ExprPtr expr, bool isMutable, bool isFunctionDecl) 
    : annotatedType(std::move(annotatedType)), name(name), expr(move(expr)), isMutable(isMutable), isFunctionDecl(isFunctionDecl) {}
Return::Return(ExprPtr expr) : expr(move(expr)) {}
FieldMember::FieldMember(TypeNodePtr annotatedType, std::string name, ExprPtr initialiser, bool isMutable) 
    : annotatedType(move(annotatedType)), name(move(name)), initialiser(move(initialiser)), isMutable(isMutable) {}

MethodMember::MethodMember(std::string name, FunctionExprPtr methodExpr) 
    : name(move(name)), methodExpr(move(methodExpr)) {}

ConstructorMember::ConstructorMember(FunctionExprPtr initFuncExpr) 
    : initFuncExpr(move(initFuncExpr)) {}  

EnumMember::EnumMember(std::shared_ptr<Enum> customEnum) 
    : customEnum(std::move(customEnum)) {}

Aggregate::Aggregate (
    AggregateKind kind,
    std::string name,
    std::vector<FieldMember> fieldMembers, 
    std::vector<MethodMember> methodMembers,
    std::vector<ConstructorMember> constructorMembers,
    std::vector<EnumMember> enumMembers,
    FunctionExprPtr fieldInitFunc
) : 
kind(kind),
name(move(name)),
fieldMembers(move(fieldMembers)),
methodMembers(move(methodMembers)),
constructorMembers(move(constructorMembers)),
enumMembers(move(enumMembers)),
fieldInitFunc(move(fieldInitFunc)) {}

TypeAlias::TypeAlias(std::string name, TypeNodePtr aliasingType) : name(move(name)), aliasingType(move(aliasingType)) {}

Enum::Enum(std::string name, std::vector<std::string> variants) : name(move(name)), variants(move(variants)) {}

ExprStmt::ExprStmt(ExprPtr expr) : expr(move(expr)) {}
