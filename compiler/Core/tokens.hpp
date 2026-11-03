#pragma once
#include <string>

// Tokens are atomic units of meaning.
// We break those down as narrowly as possible to make parsing easier. 
enum class TokenType {
    // Literals
    Int,
    Double,
    Identifier,
    StringLiteral,

    // Arithmetic Operators
    Plus,
    Minus,
    PlusPlus,   // ++
    MinusMinus, // --
    Star,
    Slash,
    Percent,
    
    // Bitwise Operators
    BitAnd,
    BitOr,
    BitNot, // ~
    BitXor, // ^
    LShift, // <<
    RShift, // >>
    
    // Logical Operators
    LogicalAnd, 
    LogicalOr,
    NullCoalesce,
    Nullable,
    
    // Assignment Operators
    Assign, // = (overloaded with declaration as well)
    PlusAssign,
    MinusAssign,
    StarAssign,
    SlashAssign,
    PercentAssign,
    BitAndAssign,
    BitOrAssign,
    BitXorAssign,
    LShiftAssign,
    RShiftAssign,
    LogicalAndAssign,
    LogicalOrAssign,
    
    // Relational Operators
    Greater,
    Less,
    GreaterEqual,
    LessEqual,
    EqualEqual,
    NotEqual,
    
    // Keywords
    KeywordPrint,
    KeywordIf,
    KeywordElse,
    KeywordWhile,
    KeywordFor,
    KeywordContinue,
    KeywordBreak,
    KeywordTrue,
    KeywordFalse,
    KeywordNull,
    KeywordLet,
    KeywordVar,
    KeywordConst,// Old: KeywordMut,
    KeywordFun,
    KeywordReturn,
    KeywordThis,
    KeywordClass,
    KeywordNew,
    KeywordShared,
    KeywordTypealias,
    KeywordRecord,
    KeywordInit,
    KeywordEnum,
    KeywordImport,
    KeywordAs,
    KeywordFrom,

    // Type keywords
    KeywordInt, // Not that this is different from just Int
    KeywordDouble, 
    KeywordBool,
    KeywordString,
    KeywordVoid,
    KeywordAny,
    KeywordI64,
    KeywordU64,
    KeywordF64,
    KeywordByte,

    // Symbols
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    Semicolon,
    Bang,
    ForceUnwrap,
    LeftBracket,
    RightBracket,
    Comma,
    Dot,
    Colon,
    Ellipsis, // ...
    Arrow, // ->
    ColonColon, // ::

    // Special
    EndOfFile,
    Unknown
};

// Token struct includes start and end indexes.
struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column; // Unused. Same purpose as startIdx but left as compatability feature.
    int startIdx;
    int endIdx;
};
