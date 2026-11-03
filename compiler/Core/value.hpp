#pragma once
#include <variant>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "AstBaseForward.hpp"
#include "../Core/operators.hpp"
#include "CodegenInfo.hpp"

// Pointers should be used to avoid circular dependencies...
// Unused
enum class ValueType_n {
    Int,
    Bool,
    Double,
    Null,
    String,
    Object // Heap allocated stuffs
};

// Unused
struct Value_n {
    ValueType_n type;
    
    union {
        int i;
        bool b;
        double d;
        void* obj;
    } data;
};


enum class ObjKind {
    Array,
    Function,
    NativeFunction,
    //BoundFn,
    Record,
    //Class, // Classes (or record definitions) are not runtime values anymore.
    Instance // TODO: used?
};

// This represents HEAP ALLOCATED VALUES (class instances, arrays, functions etc.)
struct Object {
    ObjKind kind;
    Object(ObjKind kind) : kind(kind) {}
    virtual ~Object() = default;
};

using ObjectPtr = std::shared_ptr<Object>;

using RecordPtr = std::shared_ptr<struct Record>;

using ValueType = std::variant<
    std::nullptr_t,
    int,
    double,
    bool,
    std::string,
    // ArrayPtr, // Really later.
    RecordPtr, // Internally, aggregates are passed by references. Only on usages they are copied.
    ObjectPtr, // Heap allocated values (including functions, lists, etc.)
    GarbageValue
>;

struct Value {
    ValueType data;
    bool isMutable=true; // Default is mutable.

    Value() : data(nullptr) {}
    Value(std::nullptr_t) : data(nullptr) {}
    Value(int i) : data(i) {}
    Value(double v) : data(v) {}
    Value(bool b) : data(b) {}
    Value(const std::string& s) : data(s) {}
    Value(const RecordPtr& r) : data(r) {}
    Value(const ObjectPtr& o) : data(o) {}
    Value(GarbageValue g) : data(g) {}

    std::string toString() const;
    Value clone() const; // For copying records (value semantics)
};

// Record is stack allocated, so it is not an object and copies happen when assigned.
struct Record {
    // Old.
    // std::unordered_map<std::string, Value> fields;

    std::vector<Value> fields;

    Record() = default;
    Record(const std::vector<Value>& fields) : fields(fields) {};
    RecordPtr clone() const {
        auto record = std::make_shared<Record>();
        for (const auto& field : fields) {
            if (std::holds_alternative<RecordPtr>(field.data)) {
                // Recursive clone happens inside Value constructor.

                record->fields.push_back(
                    Value(std::get<RecordPtr>(field.data)->clone())
                );
            } else {
                record->fields.push_back(field);
            }
        }
        return record;
    }
};

using ArrayPtr = std::shared_ptr<struct ArrayObj>;
using FunctionPtr = std::shared_ptr<struct FunctionObj>;
using ClassPtr = std::shared_ptr<struct ClassObj>;

struct ArrayObj : Object {
    std::vector<Value> array;

    ArrayObj() : Object(ObjKind::Array) {}

    ArrayObj(const std::vector<Value>& array): Object(ObjKind::Array), array(array) {}
};

// A shared mutable cell for closures
// Runtime object.
struct Upvalue {
    int stackSlot;
    Value closed; // The value in stack is copied.
    bool isClosed = false;

    Upvalue(int stackSlot, Value closed, bool isClosed)
        : stackSlot(stackSlot), closed(closed), isClosed(isClosed) {}
};

using UpvaluePtr = std::shared_ptr<Upvalue>;

enum class FunctionKind {
    User,
    Native
};

struct FunctionObj : Object {
    FunctionKind kind;

    // For user-defined functions/closures
    FunctionProto* proto = nullptr;
    
    // For closures
    std::vector<UpvaluePtr> upvalues; 
    
    // For native functions
    NativeFnPtr nativeFn = nullptr;
    
    /*
    // Legacy; for interpreters.
    std::vector<Parameter> params;
    StmtPtr body; // For interpreter.
    FunctionObj (
        const std::vector<Parameter>& params,
        StmtPtr body
    ) : 
    kind(FunctionKind::User), 
    Object(ObjKind::Function), 
    params(move(params)), 
    body(move(body)) {}
    */

    FunctionObj (
        NativeFnPtr nativeFn
    ) : 
    kind(FunctionKind::Native), 
    Object(ObjKind::Function), 
    nativeFn(nativeFn) {}


    FunctionObj (
        FunctionProto* proto,
        std::vector<UpvaluePtr> upvalues
    ) : 
    kind(FunctionKind::User),
    Object(ObjKind::Function),
    proto(proto),
    upvalues(move(upvalues)) {}
};

/*
// Legacy. For interpreters. Unused after moving to bytecode VM.
struct NativeFnObj : Object {

    // Legacy; for interpreters.
    // std::function<Value(const std::vector<Value>&)> func;

    int arity = -1;
    NativeFnPtr func;
    NativeFnObj(int arity, NativeFnPtr f) : 
        arity(arity),
        Object(ObjKind::NativeFunction),
        func(f) {}
};
*/

// Unused...?
struct InstanceObj : Object {
    std::unordered_map<std::string, Value> fields;
    ClassPtr cls = nullptr;

    InstanceObj() : Object(ObjKind::Instance) {}

    InstanceObj(
        std::unordered_map<std::string, Value> fields,
        ClassPtr cls = nullptr
    ) : Object(ObjKind::Instance), fields(std::move(fields)), cls(std::move(cls)) {}
};

/*
// NOTE: This is unused after moving to bytecode VM.
struct ClassObj : Object {
    // Used for instantiating objects.
    // When a new instance is created, all values in fieldDefaults are COPIED.
    // IMPORTANT: This does not copy functions. Only fields.
    
    // unordered_map<string, Value> fields; // TODO: static fields. 

    std::unordered_map<std::string, Value> fieldDefaults;
    std::unordered_map<std::string, FunctionPtr> methods;
    ClassPtr superclass = nullptr;

    ClassObj() : Object(ObjKind::Class) {}

    ClassObj(
        std::unordered_map<std::string, Value> fieldDefaults,
        std::unordered_map<std::string, FunctionPtr> methods,
        ClassPtr superclass
    ) :
        Object(ObjKind::Class),
        fieldDefaults(std::move(fieldDefaults)),
        methods(std::move(methods)),  
        superclass(std::move(superclass)) {}
};
*/

inline Value constValToVal(const ConstValue& v) {
    return std::visit([](auto&& arg) -> Value {
        return Value(arg);
    }, v);
}

Value applyBinary(BinaryOp op, const Value& left, const Value& right);

Value applyUnary(UnaryOp op, const Value& v);

bool isTruthy(const Value& v);
bool isEqual(const Value& a, const Value& b);
ObjKind typeToObjectKind(TypeKind t);
bool checkObjType(const Value& obj, ObjKind k);
bool typeMatches(const Value& v, const Type& t);
bool isNumber(const Value& v);
bool isDouble(const Value& v);
bool isInt(const Value& v);

ArrayPtr getArray(const Value& v);