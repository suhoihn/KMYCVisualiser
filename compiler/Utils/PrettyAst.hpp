#pragma once

#include <iosfwd>
#include "../Core/AstBaseForward.hpp"

// Human-readable compiler debug view. Unlike the frontend JSON format, this
// intentionally emphasizes source names, operators, and child roles.
void printPrettyAST(std::ostream& out, const ExprPtr& root);
