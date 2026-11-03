#include "value.hpp"

#include <string>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include "tokens.hpp"
#include "errorhandler.hpp"
#include <iostream>

ObjKind typeToObjectKind(TypeKind t) {
    switch(t) {
        case TypeKind::ARRAY: return ObjKind::Array;
        case TypeKind::FUNCTION : return ObjKind::Function;
        case TypeKind::INSTANCE: return ObjKind::Instance;
        //case TypeKind::   : return ObjKind::Array;
    }

    throw std::runtime_error("Unknown type.");
}

bool checkObjType(const Value& obj, ObjKind k) {
    if (!std::holds_alternative<ObjectPtr>(obj.data)) {
        return false;
    }
    return std::get<ObjectPtr>(obj.data)->kind == k;
}

bool typeMatches(const Value& v, const Type& t) {
    switch (t.kind) {
        case TypeKind::INT:
            return std::holds_alternative<int>(v.data);

        case TypeKind::DOUBLE:
            return std::holds_alternative<double>(v.data);

        case TypeKind::BOOL:
            return std::holds_alternative<bool>(v.data);

        case TypeKind::STRING:
            return std::holds_alternative<std::string>(v.data);

            //TODO?
        case TypeKind::STRUCTUAL:
            return std::holds_alternative<RecordPtr>(v.data);

        case TypeKind::ARRAY:
        case TypeKind::FUNCTION:
        case TypeKind::INSTANCE:
            return std::holds_alternative<ObjectPtr>(v.data) && 
                   checkObjType(v, typeToObjectKind(t.kind));

        case TypeKind::VOID:
            // TODO: This should be only for functions.
            return false;

        case TypeKind::ANY:
            return true;
    }

    return false;
}


std::string Value::toString() const {
    if (std::holds_alternative<int>(data)) {
        return std::to_string(std::get<int>(data));
    }
    if (std::holds_alternative<double>(data)) {
        return std::to_string(std::get<double>(data));
    }
    if (std::holds_alternative<bool>(data)) {
        return std::get<bool>(data) ? "true" : "false";
    }
    if (std::holds_alternative<std::string>(data)) {
        return "\"" + std::get<std::string>(data) + "\"";
    }
    if (std::holds_alternative<RecordPtr>(data)) {
        const RecordPtr& rec = std::get<RecordPtr>(data);
        std::string result = "{";
        for (size_t i = 0; i < rec->fields.size(); ++i) {
            result += rec->fields[i].toString();
            if (i + 1 < rec->fields.size()) {
                result += ", ";
            }
        }
        result += "}";
        return result;
    }
    if (std::holds_alternative<std::nullptr_t>(data)) {
        return "null";
    }
    if (std::holds_alternative<ObjectPtr>(data)) {
        auto obj = std::get<ObjectPtr>(data);

        if (checkObjType(*this, ObjKind::Array)) {
            auto arr = std::static_pointer_cast<ArrayObj>(obj);
            std::string result = "[";
    
            if (!arr->array.empty()) {
                for (size_t i = 0; i < arr->array.size(); i++) {
                    result += arr->array.at(i).toString();
                    if (i + 1 < arr->array.size()) result += ", ";
                }
            }
    
            result += "]";
            return result;
        }

        if (checkObjType(*this, ObjKind::Function)) {
            return "FunctionRef";
        }
        if (checkObjType(*this, ObjKind::NativeFunction)) {
            return "NativeFunction";
        }
        
        /*
        if (checkObjType(*this, ObjKind::Class)) {
            return "Class";
        }
        */
    }
    if (std::holds_alternative<GarbageValue>(data)) {
        return "<GARBAGE>";
    }
    return "unknown";
}

Value Value::clone() const {
    if (std::holds_alternative<RecordPtr>(data)) {
        return Value(std::get<RecordPtr>(data)->clone());
    }
    // For other types, the default copy is fine (they are either primitive or shared_ptr).
    return *this;
}

static bool isString(const Value& v) {
    return std::holds_alternative<std::string>(v.data);
}

bool isTruthy(const Value& v) {
    if (std::holds_alternative<std::nullptr_t>(v.data))
        return false;
    
    if (std::holds_alternative<int>(v.data))
        return std::get<int>(v.data) != 0;
    
    if (std::holds_alternative<double>(v.data))
        return std::get<double>(v.data) != 0;
    
    if (std::holds_alternative<bool>(v.data))
        return std::get<bool>(v.data);

    if (std::holds_alternative<std::string>(v.data))
        return !std::get<std::string>(v.data).empty();

        
    if (std::holds_alternative<ObjectPtr>(v.data)) 
        return true;
    
    /*
    if (std::holds_alternative<Array>(v.data)) 
        return std::get<Array>(v.data)->size() != 0;

    if (std::holds_alternative<ObjectPtr>(v.data)) 
        // TODO: Change maybe?
        return std::get<ObjectPtr>(v.data)->fields.size() != 0;
    */

    return false;
}

bool isInt(const Value& v) {
    return std::holds_alternative<int>(v.data);
}

bool isDouble(const Value& v) {
    return std::holds_alternative<double>(v.data);
}

bool isNumber(const Value& v) {
    return isInt(v) || isDouble(v) || std::holds_alternative<bool>(v.data);
}

static double toDouble(const Value& v) {
    if (std::holds_alternative<int>(v.data))
        return std::get<int>(v.data);

    if (std::holds_alternative<double>(v.data))
        return std::get<double>(v.data);

    if (std::holds_alternative<bool>(v.data))
        return std::get<bool>(v.data) ? 1.0 : 0.0;

    throw std::runtime_error("Not a number");
}

static int toInt(const Value& v) {
    if (std::holds_alternative<int>(v.data))
        return std::get<int>(v.data);

    if (std::holds_alternative<double>(v.data))
        return static_cast<int>(std::get<double>(v.data));

    if (std::holds_alternative<bool>(v.data))
        return std::get<bool>(v.data) ? 1 : 0;

    throw std::runtime_error("Not an int");
}

bool isEqual(const Value& a, const Value& b) {
    // Fast path: same variant index
    if (a.data.index() != b.data.index()) {

        // allow numeric coercion between int/double/bool
        if ((std::holds_alternative<int>(a.data)     || std::holds_alternative<double>(a.data) || std::holds_alternative<bool>(a.data)) &&
            (std::holds_alternative<int>(b.data)     || std::holds_alternative<double>(b.data) || std::holds_alternative<bool>(b.data))) {

            double x = toDouble(a);
            double y = toDouble(b);

            return (std::isnan(x) && std::isnan(y)) || x == y;
        }

        return false;
    }

    // nullptr
    if (std::holds_alternative<std::nullptr_t>(a.data))
        return true;

    // int
    if (std::holds_alternative<int>(a.data))
        return std::get<int>(a.data) == std::get<int>(b.data);

    // double
    if (std::holds_alternative<double>(a.data)) {
        double x = std::get<double>(a.data);
        double y = std::get<double>(b.data);
        return (std::isnan(x) && std::isnan(y)) || x == y;
    }

    // bool
    if (std::holds_alternative<bool>(a.data))
        return std::get<bool>(a.data) == std::get<bool>(b.data);

    // string
    if (std::holds_alternative<std::string>(a.data))
        return std::get<std::string>(a.data) == std::get<std::string>(b.data);

    // object (by reference)
    if (std::holds_alternative<ObjectPtr>(a.data))
        return std::get<ObjectPtr>(a.data) == std::get<ObjectPtr>(b.data);

    return false;
}

ArrayPtr getArray(const Value& v) {
    // PRE: v is actually an ObjectPtr with ObjKind::Array.
    // No assertion here. Can add later. TODO()
    return std::static_pointer_cast<ArrayObj>(
        std::get<ObjectPtr>(v.data)
    );
}

Value applyBinary(BinaryOp op, const Value& left, const Value& right) {
    std::cout << (int)op << std::endl;
    std::cout << "Applying binary operator to " << left.toString() << " and " << right.toString() << std::endl;
    
    // ========================
    // STRING CONCAT
    // ========================
    if (op == BinaryOp::Plus &&
        isString(left) && isString(right)) {

        return Value(std::get<std::string>(left.data) +
                     std::get<std::string>(right.data));
    }

    // ========================
    // NUMERIC OPERATIONS
    // ========================
    if (isNumber(left) && isNumber(right)) {

        bool leftIsInt = isInt(left);
        bool rightIsInt = isInt(right);

        // decide result type
        bool resultIsInt = leftIsInt && rightIsInt;

        switch (op) {

            case BinaryOp::Plus: {
                std::cout << "Applying + to " << left.toString() << " and " << right.toString() << std::endl;
                if (resultIsInt)
                    return Value(toInt(left) + toInt(right));
                return Value(toDouble(left) + toDouble(right));
            }

            case BinaryOp::Minus: {
                if (resultIsInt)
                    return Value(toInt(left) - toInt(right));
                return Value(toDouble(left) - toDouble(right));
            }

            case BinaryOp::Star: {
                if (resultIsInt)
                    return Value(toInt(left) * toInt(right));
                return Value(toDouble(left) * toDouble(right));
            }

            case BinaryOp::Slash: {
                if (resultIsInt)
                    return Value(toInt(left) / toInt(right));
                return Value(toDouble(left) / toDouble(right));
            }

            case BinaryOp::Percent: {
                if (toDouble(right) == 0.0)
                    throw std::runtime_error("Modulo by zero");
                
                if (isDouble(left)) {
                    throw std::runtime_error("Divident cannot be double.");
                }

                if (isDouble(right)) {
                    throw std::runtime_error("Divisor cannot be double.");
                }

                // C-style: integer modulo only
                return Value(toInt(left) % toInt(right));
            }
                
            case BinaryOp::LShift: {
                if (isDouble(left) || isDouble(right)) {
                    throw std::runtime_error("Invalid operand.");
                }
                return Value(toInt(left) << toInt(right));
            }

            case BinaryOp::RShift: {
                if (isDouble(left) || isDouble(right)) {
                    throw std::runtime_error("Invalid operand.");
                }
                return Value(toInt(left) >> toInt(right));
            }

            case BinaryOp::BitAnd: {
                if (isDouble(left) || isDouble(right)) {
                    throw std::runtime_error("Invalid operand.");
                }
                return Value(toInt(left) & toInt(right));
            }

            case BinaryOp::BitOr: {
                if (isDouble(left) || isDouble(right)) {
                    throw std::runtime_error("Invalid operand.");
                }
                return Value(toInt(left) | toInt(right));
            }

            case BinaryOp::BitXor:{
                if (isDouble(left) || isDouble(right)) {
                    throw std::runtime_error("Invalid operand.");
                }
                return Value(toInt(left) ^ toInt(right));
            }

            default:
                break;
        }
    }

    // ========================
    // LOGICAL
    // ========================
    if (op == BinaryOp::LogicalAnd)
        return Value(isTruthy(left) && isTruthy(right));

    if (op == BinaryOp::LogicalOr)
        return Value(isTruthy(left) || isTruthy(right));

    // ========================
    // COMPARISON
    // ========================
    if (op == BinaryOp::EqualEqual)
        return Value(isEqual(left, right));

    if (op == BinaryOp::NotEqual)
        return Value(!isEqual(left, right));

    if (op == BinaryOp::Greater ||
        op == BinaryOp::GreaterEqual ||
        op == BinaryOp::Less ||
        op == BinaryOp::LessEqual) {

        // numeric comparison
        if (isNumber(left) && isNumber(right)) {
            double a = toDouble(left);
            double b = toDouble(right);

            switch (op) {
                case BinaryOp::Greater: return Value(a > b);
                case BinaryOp::GreaterEqual: return Value(a >= b);
                case BinaryOp::Less: return Value(a < b);
                case BinaryOp::LessEqual: return Value(a <= b);
                default: break;
            }
        }

        // string comparison
        if (isString(left) && isString(right)) {
            const auto& a = std::get<std::string>(left.data);
            const auto& b = std::get<std::string>(right.data);

            switch (op) {
                case BinaryOp::Greater: return Value(a > b);
                case BinaryOp::GreaterEqual: return Value(a >= b);
                case BinaryOp::Less: return Value(a < b);
                case BinaryOp::LessEqual: return Value(a <= b);
                default: break;
            }
        }

        throw std::runtime_error("Invalid operands for comparison");
    }

    throw std::runtime_error("Unsupported binary operator");
}

Value applyUnary(UnaryOp op, const Value& v) {
    switch(op) {
        case UnaryOp::LogicalNot: return Value(!isTruthy(v));
        case UnaryOp::Minus: {
            if (!isNumber(v)) 
                throw std::runtime_error("Cannot apply unary - to non-number.");
            
            if (isInt(v)) {
                return Value(-toInt(v));
            }

            return Value(-toDouble(v));
        }
        case UnaryOp::Plus: {
            if (!isNumber(v)) 
                throw std::runtime_error("Cannot apply unary + to non-number.");
            
            return v; // Nothing changes.
        }
        case UnaryOp::BitNot: {
            if (!isInt(v)) 
                throw std::runtime_error("Cannot apply unary ~ to non-int.");
            
            return Value( ~toInt(v) );
        }
        default:
            throw std::runtime_error("Unknown unary operator");
    }
}
