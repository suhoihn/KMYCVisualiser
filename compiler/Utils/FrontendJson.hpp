#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "../Core/Ast.hpp"
#include "../Core/newParser.hpp"
#include "../Core/tokens.hpp"

void writeFrontendJson(
    std::ostream& out,
    const std::string& source,
    const std::vector<Token>& tokens,
    const std::vector<ParserTraceEvent>& parserTrace,
    const FunctionExprPtr& program
);

void writeFrontendErrorJson(
    std::ostream& out,
    const std::string& source,
    const std::vector<Token>& tokens,
    const std::vector<ParserTraceEvent>& parserTrace,
    const std::string& message,
    int line,
    int start,
    int end
);
