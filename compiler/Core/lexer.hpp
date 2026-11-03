#pragma once

#include <string>
#include <vector>
#include "tokens.hpp"

class Lexer {
public:
    Lexer(const std::string& input);
    std::vector<Token> tokenise();

private:
    const std::string& source;
    int start = 0;
    int current = 0;
    int line = 1;
    int column = 1;

    char peek() const;
    char peekNext() const;
    char advance();
    bool is_at_end() const;

    void skip_whitespace();
    Token read_number();
    Token read_identifier_or_keyword();
    Token read_operator_or_symbol();
    Token read_string();
};
