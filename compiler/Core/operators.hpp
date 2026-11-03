#pragma once

#include <stdexcept>
#include "tokens.hpp"

// Global
enum class BinaryOp {
    // Arithmetic
    Plus,
    Minus,
    Star,
    Slash,
    Percent,

    // Bitwise
    BitAnd,
    BitOr,
    BitXor,
    LShift,
    RShift,

    // Logical
    LogicalAnd,
    LogicalOr,
    NullCoalesce,

    // Comparison
    Greater,
    Less,
    GreaterEqual,
    LessEqual,
    EqualEqual,
    NotEqual,
};

enum class UnaryOp {
    // Arithmetic
    Plus,        
    Minus,

    // Logical
    LogicalNot, // !

    // Bitwise
    BitNot, // ~
    AddressOf, // &x
    Dereference, // *x
    ForceUnwrap, // !!

    // Mutation expressions. Prefix and postfix must stay distinct because
    // they produce different expression values even though both mutate once.
    PreIncrement,   // ++x
    PreDecrement,   // --x
    PostIncrement,  // x++
    PostDecrement   // x--
};

enum class AssignmentOp {
    Assign,
    PlusAssign,
    MinusAssign,
    StarAssign,
    SlashAssign,
    PercentAssign,
    BitAndAssign,
    BitOrAssign,
    BitNotAssign,
    BitXorAssign,
    LShiftAssign,
    RShiftAssign,
    LogicalAndAssign,
    LogicalOrAssign,
};

inline BinaryOp compoundToBinaryOp(AssignmentOp t) {
    switch (t) {
        case AssignmentOp::PlusAssign:    return BinaryOp::Plus;
        case AssignmentOp::MinusAssign:   return BinaryOp::Minus;
        case AssignmentOp::StarAssign:    return BinaryOp::Star;
        case AssignmentOp::SlashAssign:   return BinaryOp::Slash;
        case AssignmentOp::PercentAssign: return BinaryOp::Percent;

        case AssignmentOp::BitAndAssign:     return BinaryOp::BitAnd;
        case AssignmentOp::BitOrAssign:      return BinaryOp::BitOr;
        case AssignmentOp::BitXorAssign:     return BinaryOp::BitXor;
        case AssignmentOp::LShiftAssign:  return BinaryOp::LShift;
        case AssignmentOp::RShiftAssign:  return BinaryOp::RShift;

        case AssignmentOp::LogicalAndAssign:  return BinaryOp::LogicalAnd;
        case AssignmentOp::LogicalOrAssign:    return BinaryOp::LogicalOr;

        default:
            throw std::runtime_error("Invalid compound assignment token");
    }
}

inline BinaryOp toBinaryOp(TokenType t) {
    switch (t) {
        case TokenType::Plus:   return BinaryOp::Plus;
        case TokenType::Minus:  return BinaryOp::Minus;
        case TokenType::Star:   return BinaryOp::Star;
        case TokenType::Slash:  return BinaryOp::Slash;
        case TokenType::Percent:return BinaryOp::Percent;

        case TokenType::BitAnd: return BinaryOp::BitAnd;
        case TokenType::BitOr:  return BinaryOp::BitOr;
        case TokenType::BitXor: return BinaryOp::BitXor;
        case TokenType::LShift: return BinaryOp::LShift;
        case TokenType::RShift: return BinaryOp::RShift;

        case TokenType::LogicalAnd: return BinaryOp::LogicalAnd;
        case TokenType::LogicalOr:  return BinaryOp::LogicalOr;

        case TokenType::Greater:       return BinaryOp::Greater;
        case TokenType::Less:          return BinaryOp::Less;
        case TokenType::GreaterEqual:  return BinaryOp::GreaterEqual;
        case TokenType::LessEqual:     return BinaryOp::LessEqual;
        case TokenType::EqualEqual:    return BinaryOp::EqualEqual;
        case TokenType::NotEqual:      return BinaryOp::NotEqual;
        case TokenType::NullCoalesce:  return BinaryOp::NullCoalesce;

        default:
            throw std::runtime_error("Invalid TokenType for BinaryOp");
    }
}

inline UnaryOp toUnaryOp(TokenType t) {
    switch (t) {
        case TokenType::Plus:  return UnaryOp::Plus;
        case TokenType::Minus: return UnaryOp::Minus;
        case TokenType::Bang:  return UnaryOp::LogicalNot;
        case TokenType::BitNot:return UnaryOp::BitNot;
        // '&' and '*' share tokens with their binary operators; parse
        // context determines whether they reach this unary conversion.
        case TokenType::BitAnd: return UnaryOp::AddressOf;
        case TokenType::Star:   return UnaryOp::Dereference;
        case TokenType::ForceUnwrap:return UnaryOp::ForceUnwrap;
        // This helper is called only by parse_prefix(). parse_expression()
        // constructs the Post* variants directly after parsing its left side.
        case TokenType::PlusPlus: return UnaryOp::PreIncrement;
        case TokenType::MinusMinus: return UnaryOp::PreDecrement;

        default:
            throw std::runtime_error("Invalid TokenType for UnaryOp");
    }
}

inline AssignmentOp toAssignmentOp(TokenType t) {
    switch (t) {
        case TokenType::Assign: return AssignmentOp::Assign;

        case TokenType::PlusAssign:   return AssignmentOp::PlusAssign;
        case TokenType::MinusAssign:  return AssignmentOp::MinusAssign;
        case TokenType::StarAssign:   return AssignmentOp::StarAssign;
        case TokenType::SlashAssign:  return AssignmentOp::SlashAssign;
        case TokenType::PercentAssign:return AssignmentOp::PercentAssign;

        case TokenType::BitAndAssign: return AssignmentOp::BitAndAssign;
        case TokenType::BitOrAssign:  return AssignmentOp::BitOrAssign;
        case TokenType::BitXorAssign: return AssignmentOp::BitXorAssign;

        case TokenType::LShiftAssign: return AssignmentOp::LShiftAssign;
        case TokenType::RShiftAssign: return AssignmentOp::RShiftAssign;

        case TokenType::LogicalAndAssign: return AssignmentOp::LogicalAndAssign;
        case TokenType::LogicalOrAssign:  return AssignmentOp::LogicalOrAssign;

        default:
            throw std::runtime_error("Invalid TokenType for AssignmentOp");
    }
}
