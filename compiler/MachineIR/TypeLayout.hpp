#pragma once

#include "../Core/Symbol.hpp"
#include "../Core/type.hpp"
#include "../Core/errorhandler.hpp"
#include <cstddef>


constexpr size_t PTR_SIZE = 8;


struct TypeLayout {
    static size_t sizeOf(Type* type) {
        switch (type->kind) {
            case TypeKind::INT:
                return 8;

            case TypeKind::DOUBLE:
                return 8;

            case TypeKind::BOOL:
                return 1;


            // All pointers (inclding cells) are same size
            case TypeKind::POINTER:
            case TypeKind::SHARED:
            case TypeKind::CELL:
                return PTR_SIZE;


            // Runtime objects are pointers
            case TypeKind::STRING:
                return PTR_SIZE;

            case TypeKind::FUNCTION:
                return PTR_SIZE;


            // null is represented as pointer
            case TypeKind::NULLTYPE:
                return PTR_SIZE;


            case TypeKind::ARRAY:
                return PTR_SIZE;


            case TypeKind::ENUM:
                return 4; // Int32 for now. TODO: Make it configurable



            case TypeKind::STRUCTUAL: {
                auto* st = static_cast<StructualType*>(type);

                size_t size = 0;

                for (auto& [name, fieldType] : st->fieldTypes)
                {
                    size += sizeOf(fieldType);
                }

                return size;
            }


            case TypeKind::INSTANCE: {
                // Instances are represented by pointers to heap records in the
                // native backend, not by inline aggregate values.
                return PTR_SIZE;
            }


            case TypeKind::VOID:
                throw KMYCompileError(
                    "void has no size"
                );


            case TypeKind::ANY:
                throw KMYCompileError(
                    "any has no static size"
                );


            case TypeKind::UNKNOWN:
            case TypeKind::UNINITIALISED:
                throw KMYCompileError(
                    "panic: invalid type size!!!"
                );
        }


        throw KMYCompileError(
            "unhandled type size"
        );
    }
};
