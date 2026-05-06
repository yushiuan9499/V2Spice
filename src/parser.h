#ifndef PARSER_H
#define PARSER_H

#include <vector>

#include "defs.h"

std::vector<Ast *> parse(const std::vector<Token> &tokens);

#endif  // PARSER_H
