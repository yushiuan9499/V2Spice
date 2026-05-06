#ifndef SEMA_H
#define SEMA_H

#include <vector>

#include "defs.h"

std::vector<Ast *> sema(const std::vector<Ast *> &asts);

#endif  // SEMA_H
