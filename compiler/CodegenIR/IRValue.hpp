#pragma once

#include "../Core/type.hpp"
#include "../Utils/SymbolPrinter.hpp"

// Represents ANYTHING (int, double, function, array, etc. just anything.)
// Don't worry. Its type is encoded as well.
struct IRValue {
    int id;
    Type* type = nullptr;
};

struct IRCapture {
    bool isLocal;
    IRValue value;
};

inline std::ostream& operator<<(std::ostream& os, const IRValue& value) {
    return os << "v" << value.id << " (" << typeToString(value.type) << ")";
}

// Unused?
inline std::ostream& operator<<(std::ostream& os, const IRCapture& capture) {
    return os << "capture of " << capture.value 
              << " (local: " << (capture.isLocal? std::string("true") : std::string("false")) << ")";
}
