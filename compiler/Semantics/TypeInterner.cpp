#include "TypeInterner.hpp"

#include <string>
#include <unordered_map>
#include "../Core/type.hpp"
#include "../Core/AstBaseForward.hpp"

PointerType* TypeInterner::getPointerType(Type* pointee) {
    auto it = pointerCache.find(pointee);
    if (it != pointerCache.end()) return it->second;

    auto* pointer = new PointerType(pointee);
    pointerCache[pointee] = pointer;
    return pointer;
}

SharedType* TypeInterner::getSharedType(Type* innerType) {
    auto it = sharedCache.find(innerType);
    if (it != sharedCache.end()) return it->second;

    auto* shared = new SharedType(innerType);
    sharedCache[innerType] = shared;
    return shared;
}

ArrayType* TypeInterner::getArrayType(Type* elementType, std::optional<size_t> fixedLength) {
    ArrayKey key{elementType, fixedLength};
    auto it = arrayCache.find(key);
    if (it != arrayCache.end()) {
        return it->second;
    }

    // New element type for array.
    ArrayType* newArrType = new ArrayType(elementType, fixedLength);
    arrayCache[key] = newArrType;

    return newArrType;
}

FunctionType* TypeInterner::getFunctionType(
    std::vector<Type*> paramTypes, 
    Type* returnType
) {
    FunctionKey fnKey{paramTypes, returnType};

    auto it = fnCache.find(fnKey);
    if (it != fnCache.end()) {
        return it->second;
    }

    // New element type for function.
    FunctionType* newFnType = new FunctionType(paramTypes, returnType);
    fnCache[fnKey] = newFnType;

    return newFnType;
}

StructualType* TypeInterner::getStructualType(std::unordered_map<std::string, Type*> raw) {
    // IMPORTANT: {x:int, y:int} and {y:int, x:int} are considered the SAME types!
    RecordKey recordKey = makeKey(raw);

    auto it = recordCache.find(recordKey);
    if (it != recordCache.end()) {
        return it->second;
    }

    // New element type for record.
    StructualType* newRecordType = new StructualType();
    int offset = 0;
    for (auto& [name, type] : raw) {
        newRecordType->fieldTypes[name] = type;
        newRecordType->layout[name] = offset++;
    }
    recordCache[recordKey] = newRecordType;

    return newRecordType;
}


RecordKey TypeInterner::makeKey(std::unordered_map<std::string, Type*> raw) {
    std::vector<FieldKey> fields;

    for (auto& [name, type] : raw) {
        fields.push_back({name, type});
    }

    std::sort(fields.begin(), fields.end(),
        [](const FieldKey& a, const FieldKey& b) {
            return a.name < b.name;
        });

    return {fields};
}
