#include "lexer.hpp"

#include <string>
#include <unordered_map>

Lexer::Lexer(const std::string& input) : source(input) {}

char Lexer::peek() const {
    return is_at_end() ? '\0' : source[current];
}

char Lexer::peekNext() const {
    return (current + 1 >= source.length()) ? '\0' : source[current + 1];
}

char Lexer::advance() {
    char c = peek();
    ++current;
    ++column;
    return c;
}

bool Lexer::is_at_end() const {
    return current >= source.length();
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            advance();
            ++line;
            column = 1;
        } else {
            break;
        }
    }
}

Token Lexer::read_number() {
    int start = current;
    bool isDouble = false;
    while (isdigit(peek())) {
        advance();
    }
    // Check for floating-point numbers that follows the integer part.
    if (peek() == '.') {
        isDouble = true;
        advance();
        while (isdigit(peek())) {
            advance();
        }
    }

    std::string lexeme = source.substr(start, current - start);
    return Token{
        isDouble ? TokenType::Double : TokenType::Int,
        lexeme, line, column, start, current
    };
}

// Cannot use switch for strings...
static std::unordered_map<std::string, TokenType> keywords = {
    {"print", TokenType::KeywordPrint},
    {"if", TokenType::KeywordIf},
    {"else", TokenType::KeywordElse},
    {"while", TokenType::KeywordWhile},
    {"for", TokenType::KeywordFor},
    {"continue", TokenType::KeywordContinue},
    {"break", TokenType::KeywordBreak},
    {"true", TokenType::KeywordTrue},
    {"false", TokenType::KeywordFalse},
    {"null", TokenType::KeywordNull},
    {"let", TokenType::KeywordLet},
    {"var", TokenType::KeywordVar},
    //{"mut", TokenType::KeywordMut},
    {"const", TokenType::KeywordConst},
    {"fun", TokenType::KeywordFun},
    {"return", TokenType::KeywordReturn},
    {"this", TokenType::KeywordThis},
    {"class", TokenType::KeywordClass},
    {"new", TokenType::KeywordNew},
    {"shared", TokenType::KeywordShared},
    {"typealias", TokenType::KeywordTypealias},
    {"record", TokenType::KeywordRecord},
    {"init", TokenType::KeywordInit},
    {"enum", TokenType::KeywordEnum},
    {"import", TokenType::KeywordImport},
    {"as", TokenType::KeywordAs},
    {"from", TokenType::KeywordFrom},

    {"int", TokenType::KeywordInt},
    {"double", TokenType::KeywordDouble},
    {"bool", TokenType::KeywordBool},
    {"string", TokenType::KeywordString},
    {"void", TokenType::KeywordVoid},
    {"any", TokenType::KeywordAny},
    {"i64", TokenType::KeywordI64},
    {"u64", TokenType::KeywordU64},
    {"f64", TokenType::KeywordF64},
    {"byte", TokenType::KeywordByte},
};

Token Lexer::read_identifier_or_keyword() {
    int start = current;
    while (isalnum(peek()) || peek() == '_') {
        advance();
    }

    std::string lexeme = source.substr(start, current - start);
    TokenType type = TokenType::Identifier;
    if (keywords.find(lexeme) != keywords.end()) {
        type = keywords[lexeme];
    }
    return Token{type, lexeme, line, column, start, current};
}


constexpr int MAX_SYMBOL_LENGTH = 3;

static std::unordered_map<std::string, TokenType> symbols = {
    {"+", TokenType::Plus},
    {"-", TokenType::Minus},
    {"++", TokenType::PlusPlus},
    {"--", TokenType::MinusMinus},
    {"*", TokenType::Star},
    {"/", TokenType::Slash},
    {"%", TokenType::Percent},

    {"&", TokenType::BitAnd},
    {"|", TokenType::BitOr},
    {"~", TokenType::BitNot},
    {"^", TokenType::BitXor},
    {"<<", TokenType::LShift},
    {">>", TokenType::RShift},
    
    {"&&", TokenType::LogicalAnd},
    {"||", TokenType::LogicalOr},
    {"??", TokenType::NullCoalesce},
    {"?", TokenType::Nullable},

    {"=", TokenType::Assign},
    {"+=", TokenType::PlusAssign},
    {"-=", TokenType::MinusAssign},
    {"*=", TokenType::StarAssign},
    {"/=", TokenType::SlashAssign},
    {"%=", TokenType::PercentAssign},
    {"&=", TokenType::BitAndAssign},
    {"|=", TokenType::BitOrAssign},
    {"^=", TokenType::BitXorAssign},
    {"<<=", TokenType::LShiftAssign},
    {">>=", TokenType::RShiftAssign},
    {"&&=", TokenType::LogicalAndAssign},
    {"||=", TokenType::LogicalOrAssign},

    {">", TokenType::Greater},
    {"<", TokenType::Less},
    {">=", TokenType::GreaterEqual},
    {"<=", TokenType::LessEqual},
    {"==", TokenType::EqualEqual},
    {"!=", TokenType::NotEqual},

    {"(", TokenType::LeftParen},
    {")", TokenType::RightParen},
    {"{", TokenType::LeftBrace},
    {"}", TokenType::RightBrace},
    {";", TokenType::Semicolon},
    {"!", TokenType::Bang},
    {"!!", TokenType::ForceUnwrap},
    {"[", TokenType::LeftBracket},
    {"]", TokenType::RightBracket},
    {",", TokenType::Comma},
    {".", TokenType::Dot},
    {":", TokenType::Colon},
    {"...", TokenType::Ellipsis},
    {"->", TokenType::Arrow},
    {"::", TokenType::ColonColon}
};

Token Lexer::read_operator_or_symbol() {
    int start = current;

    // Prevents over-reading.
    int maxLen = std::min(MAX_SYMBOL_LENGTH, static_cast<int>(source.length() - current));

    for (int len = maxLen; len >= 1; --len) {
        std::string candidate = source.substr(current, len);

        auto it = symbols.find(candidate);
        if (it != symbols.end()) {
            for (int i = 0; i < len; i++) {
                advance();
            }

            return Token{it->second, candidate, line, column, start, current};
        }
    }

    std::string bad(1, advance());
    return Token{TokenType::Unknown, bad, line, column, start, current};
}

Token Lexer::read_string() {
    int tokenStart = current;
    advance(); // Consume the opening quote
    int contentStart = current;
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') {
            ++line;
            column = 1;
        }
        advance();
    }

    if (is_at_end()) {
        return Token{TokenType::Unknown, "", line, column, tokenStart, current}; // Unterminated string
    }

    advance(); // Consume the closing quote
    std::string lexeme = source.substr(contentStart, current - contentStart - 1); // Exclude quotes
    return Token{TokenType::StringLiteral, lexeme, line, column, tokenStart, current};
}

std::vector<Token> Lexer::tokenise() {
    std::vector<Token> tokens;
    while (!is_at_end()) {
        skip_whitespace();

        if (is_at_end()) break;


        char c = peek();
        // Comment handling
        if (c == '/' && peekNext() == '/') {
            // Consume both slashes
            advance();
            advance();

            // Skip until newline OR EOF
            while (!is_at_end() && peek() != '\n') {
                advance();
            }

            // NOTE: newline is consumed here!
            if (peek() == '\n') {
                advance();
                ++line;
                column = 1;
            }

            continue;
        }
        if (c == '"') {
            // Handle std::string literals
            tokens.push_back(read_string());
        } else if (isdigit(c) || (c == '.' && isdigit(peekNext()))) { 
            // Number literals (including support for floating-point)
            tokens.push_back(read_number());
        } else if (isalpha(c) || c == '_') {
            // Identifiers and keywords
            tokens.push_back(read_identifier_or_keyword());
        } else {
            // Operators and symbols
            // NOTE: Lexer doesn't read array literals here! (Just reads like '[' contents ']')
            tokens.push_back(read_operator_or_symbol());
        }
    }
    tokens.push_back(Token{TokenType::EndOfFile, "", line, column, current, current});
    return tokens;
}
