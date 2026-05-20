#include <algorithm>
#include <iostream>
#include <map>
#include <string>

#include "defs.h"
#include "log.h"
#include "sema.h"

static std::map<std::string, ModuleDeclAst *> module_table;
static std::map<ModuleDeclAst *, std::map<std::string, short>>
    module_port_indices;
union Number {
    long long int_value;
    double float_value;
};
static std::map<std::string, std::pair<bool, Number>> constant_table;
static std::map<std::string, Type> type_table;

static Ast *sema_generic(Ast *ast, bool may_be_genvar = false);

static signed char parse_exp(const Loc &pos)
{
    const char c = get_file_content(pos)[0];
    if (c == 'p') {
        return -12;
    } else if (c == 'n') {
        return -9;
    } else if (c == 'u') {
        return -6;
    } else if (c == 'm') {
        return -3;
    } else if (c == 'k') {
        return 3;
    } else if (c == 'M') {
        return 6;
    } else if (c == 'G') {
        return 9;
    } else if (c == 'T') {
        return 12;
    } else if (c == 'P') {
        return 15;
    } else if (c == 'E') {
        return 18;
    }
    critical(pos, "Invalid unit suffix in number literal.");
    return 0;  // Unreachable
}
static double mul_pow10(double x, signed char exp)
{
    for (signed char i = 0; i < exp; i++) {
        x *= 10;
    }
    for (signed char i = 0; i > exp; i--) {
        x /= 10;
    }
    return x;
}
static Ast *sema_number(NumberAst *ast, bool is_genvar)
{
    std::string ss = std::to_string(is_genvar);
    Loc pos = ast->pos;
    ast->is_float = !is_genvar;
    bool after_dot = false;
    bool has_exp = false;
    for (unsigned short i = 0; i < pos.len; i++) {
        if (get_file_content(pos)[i] == '.') {
            if (is_genvar) {
                critical({pos.start + i, 1},
                         "Genvar cannot have a decimal point.");
            }
            if (after_dot) {
                critical(pos, "Multiple dots in number literal.");
            }
            after_dot = true;
            continue;
        }
        if (get_file_content(pos)[i] < '0' || get_file_content(pos)[i] > '9') {
            if (i == pos.len - 1 && !is_genvar) {
                /* This is a number with a unit suffix */
                has_exp = true;
                break;
            }
            critical({pos.start + i, 1},
                     "Invalid character in number literal.");
        }
    }
    if (ast->is_float) {
        ast->float_value = std::stod(
            get_file_content({pos.start, pos.len - (has_exp ? 1 : 0), pos.fd}));

        if (has_exp) {
            ast->float_value =
                mul_pow10(ast->float_value,
                          parse_exp({pos.start + pos.len - 1, 1, pos.fd}));
        }
    } else {
        ast->int_value = std::stoll(get_file_content(pos));
    }
    return ast;
}

static Ast *sema_id(IdAst *ast)
{
    std::string name = get_file_content(ast->name);
    Type type;
    if (type_table.count(name)) {
        type = type_table[name];
    } else {
        type = DEFAULT_TYPE;
    }

    if (type.type == TYPE_LOCALPARAM) {
        NumberAst *number_ast = new NumberAst;
        number_ast->is_float = constant_table[name].first;
        if (number_ast->is_float) {
            number_ast->float_value = constant_table[name].second.float_value;
        } else {
            number_ast->int_value = constant_table[name].second.int_value;
        }
        delete ast;
        return number_ast;
    }
    ast->var_type = type;
    return ast;
}

static Ast *sema_system_func_call(SystemFuncCallAst *ast, bool may_be_genvar)
{
    std::string func_name = get_file_content(ast->func_id);
    if (func_name == "$spice") {
        if (ast->args.size() == 0) {
            critical(ast->func_id, "$spice requires at least one argument.");
        }
        if (ast->args[0]->type != AST_TYPE_STR) {
            critical(((StrAst *) ast->args[0])->str,
                     "First argument of $spice must be a string literal.");
        }
        return ast;
    }
    warning(ast->func_id, "Unknown system function, ignored");
    return nullptr;
}


static Ast *sema_subscript(SubscriptAst *ast, bool may_be_genvar)
{
    ast->array = sema_generic(ast->array, may_be_genvar);
    if (ast->array->type != AST_TYPE_ID &&
        ast->array->type != AST_TYPE_SUBSCRIPT) {
        /* TODO: */
        exit(2);
    }
    ast->index1 = sema_generic(ast->index1, true);
    if (ast->index1->type != AST_TYPE_NUMBER) {
        /* TODO: */
        exit(2);
    }
    if (ast->index2) {
        ast->index2 = sema_generic(ast->index2, true);
        if (ast->index1->type != AST_TYPE_NUMBER) {
            /* TODO: */
            exit(2);
        }
    }
    return ast;
}

static Ast *sema_unary_op(UnaryAst *ast, bool may_be_genvar)
{
    ast->operand = sema_generic(ast->operand, may_be_genvar);
    if (ast->operand->type == AST_TYPE_BINARY_OP) {
        /* If the operand is a binary operator, it means that is an assign
         * expression */
        critical(ast->op.loc,
                 "Operand of unary operator cannot be an assign expression.");
    } else if (ast->operand->type == AST_TYPE_ID ||
               ast->operand->type == AST_TYPE_SUBSCRIPT) {
        critical(ast->op.loc, "Operand of unary operator cannot be wire.");
    }
    NumberAst *operand = static_cast<NumberAst *>(ast->operand);
    switch (ast->op.type) {
    case TOKEN_TYPE_PLUS:
        return operand;
    case TOKEN_TYPE_MINUS:
        if (operand->is_float) {
            operand->float_value = -operand->float_value;
        } else {
            operand->int_value = -operand->int_value;
        }
        return operand;
    default:
        internal_error(ast->op.loc, "Unexpected unary operator.");
    }
    return nullptr;  // Unreachable
}

static Ast *sema_binary_op(BinaryOpAst *ast, bool may_be_genvar)
{
    ast->lhs = sema_generic(ast->lhs, may_be_genvar);
    ast->rhs = sema_generic(ast->rhs, may_be_genvar);
    bool has_error = false;
    if (ast->lhs->type == AST_TYPE_BINARY_OP) {
        has_error = true;
        /* If the LHS is a binary operator, it means that is an assign
         * expression */
        error(ast->op.loc,
              "Left-hand side expression cannot be a assign expression.");
    }
    if (ast->lhs->type == AST_TYPE_NUMBER && ast->op.type == TOKEN_TYPE_EQUAL) {
        has_error = true;
        error(ast->op.loc, "Cannot assign to a number.");
    }
    if ((ast->lhs->type == AST_TYPE_ID ||
         ast->lhs->type == AST_TYPE_SUBSCRIPT) &&
        ast->op.type != TOKEN_TYPE_EQUAL) {
        has_error = true;
        error(ast->op.loc,
              "Only assignment operator is allowed for identifiers.");
    }

    if (ast->rhs->type == AST_TYPE_BINARY_OP) {
        has_error = true;
        /* If the RHS is a binary operator, it means that is an assign
         * expression */
        error(ast->op.loc,
              "Right-hand side expression cannot be a assign expression.");
    }
    if ((ast->rhs->type == AST_TYPE_ID ||
         ast->rhs->type == AST_TYPE_SUBSCRIPT) &&
        ast->op.type != TOKEN_TYPE_EQUAL) {
        has_error = true;
        error(ast->op.loc,
              "Only assignment operator is allowed for identifiers.");
    }
    if (ast->rhs->type != AST_TYPE_NUMBER && ast->op.type != TOKEN_TYPE_EQUAL) {
        std::string msg =
            "Unexpected right-hand side expression for operator '";
        msg += token_str[ast->op.type];
        internal_error(ast->op.loc, msg.c_str());
    }
    if (has_error) {
        exit(1);
    }
    if (ast->op.type == TOKEN_TYPE_EQUAL)
        return ast;

    NumberAst *lhs = static_cast<NumberAst *>(ast->lhs);
    NumberAst *rhs = static_cast<NumberAst *>(ast->rhs);
    switch (ast->op.type) {
    case TOKEN_TYPE_PLUS:
        if (lhs->is_float || rhs->is_float) {
            NumberAst *result = new NumberAst;
            result->is_float = true;
            result->float_value =
                (lhs->is_float ? lhs->float_value : lhs->int_value) +
                (rhs->is_float ? rhs->float_value : rhs->int_value);
            delete ast;
            return result;
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = lhs->int_value + rhs->int_value;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_MINUS:
        if (lhs->is_float || rhs->is_float) {
            NumberAst *result = new NumberAst;
            result->is_float = true;
            result->float_value =
                (lhs->is_float ? lhs->float_value : lhs->int_value) -
                (rhs->is_float ? rhs->float_value : rhs->int_value);
            delete ast;
            return result;
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = lhs->int_value - rhs->int_value;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_ASTERISK:
        if (lhs->is_float || rhs->is_float) {
            NumberAst *result = new NumberAst;
            result->is_float = true;
            result->float_value =
                (lhs->is_float ? lhs->float_value : lhs->int_value) *
                (rhs->is_float ? rhs->float_value : rhs->int_value);
            delete ast;
            return result;
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = lhs->int_value * rhs->int_value;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_SLASH:
        if (lhs->is_float || rhs->is_float) {
            NumberAst *result = new NumberAst;
            result->is_float = true;
            result->float_value =
                (lhs->is_float ? lhs->float_value : lhs->int_value) /
                (rhs->is_float ? rhs->float_value : rhs->int_value);
            delete ast;
            return result;
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = lhs->int_value / rhs->int_value;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_PERCENT:
        if (lhs->is_float || rhs->is_float) {
            critical(ast->op.loc,
                     "Modulo operator is not supported for float.");
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = lhs->int_value % rhs->int_value;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_OR:
        if (lhs->is_float || rhs->is_float) {
            critical(ast->op.loc,
                     "Logical OR operator is not supported for float.");
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = (lhs->int_value || rhs->int_value) ? 1 : 0;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_AND:
        if (lhs->is_float || rhs->is_float) {
            critical(ast->op.loc,
                     "Logical AND operator is not supported for float.");
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = (lhs->int_value && rhs->int_value) ? 1 : 0;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_EQUAL2:
        if (lhs->is_float || rhs->is_float) {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value =
                ((lhs->is_float ? lhs->float_value : lhs->int_value) ==
                 (rhs->is_float ? rhs->float_value : rhs->int_value))
                    ? 1
                    : 0;
            delete ast;
            return result;
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = (lhs->int_value == rhs->int_value) ? 1 : 0;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_NOT_EQUAL:
        if (lhs->is_float || rhs->is_float) {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value =
                ((lhs->is_float ? lhs->float_value : lhs->int_value) !=
                 (rhs->is_float ? rhs->float_value : rhs->int_value))
                    ? 1
                    : 0;
            delete ast;
            return result;
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = (lhs->int_value != rhs->int_value) ? 1 : 0;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_LT:
        if (lhs->is_float || rhs->is_float) {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value =
                ((lhs->is_float ? lhs->float_value : lhs->int_value) <
                 (rhs->is_float ? rhs->float_value : rhs->int_value))
                    ? 1
                    : 0;
            delete ast;
            return result;
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = (lhs->int_value < rhs->int_value) ? 1 : 0;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_GT:
        if (lhs->is_float || rhs->is_float) {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value =
                ((lhs->is_float ? lhs->float_value : lhs->int_value) >
                 (rhs->is_float ? rhs->float_value : rhs->int_value))
                    ? 1
                    : 0;
            delete ast;
            return result;
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = (lhs->int_value > rhs->int_value) ? 1 : 0;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_LE:
        if (lhs->is_float || rhs->is_float) {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value =
                ((lhs->is_float ? lhs->float_value : lhs->int_value) <=
                 (rhs->is_float ? rhs->float_value : rhs->int_value))
                    ? 1
                    : 0;
            delete ast;
            return result;
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = (lhs->int_value <= rhs->int_value) ? 1 : 0;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_GE:
        if (lhs->is_float || rhs->is_float) {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value =
                ((lhs->is_float ? lhs->float_value : lhs->int_value) >=
                 (rhs->is_float ? rhs->float_value : rhs->int_value))
                    ? 1
                    : 0;
            delete ast;
            return result;
        } else {
            NumberAst *result = new NumberAst;
            result->is_float = false;
            result->int_value = (lhs->int_value >= rhs->int_value) ? 1 : 0;
            delete ast;
            return result;
        }
    case TOKEN_TYPE_COMMA:
        ast->rhs = nullptr;
        delete ast->lhs;
        return rhs;
    default:
        internal_error(ast->op.loc,
                       "This operator should have been handled in the parser.");
    }
    return ast;  // Unreachable
}

/* This is for R, L, C */
static const std::map<std::string, short> passive_2port = {{"n1", 0},
                                                           {"n2", 1}};
static const std::map<std::string, short> mj_port = {{"D", 0},
                                                     {"G", 1},
                                                     {"S", 2},
                                                     {"B", 3}};
static const std::map<std::string, short> b_port = {{"C", 0},
                                                    {"B", 1},
                                                    {"E", 2},
                                                    {"S", 3}};
static const std::map<std::string, short> d_port = {{"P", 0}, {"N", 1}};

static Ast *sema_module_inst(ModuleInstAst *ast)
{
    const std::map<std::string, short> *port_map = nullptr;
    const std::string module_name = get_file_content(ast->module_name);
    switch (toupper(get_file_content(ast->instance_name)[0])) {
    case 'R':
    case 'L':
    case 'C':
        port_map = &passive_2port;
        break;
    case 'M':
    case 'J':
        port_map = &mj_port;
        break;
    case 'B':
        port_map = &b_port;
        break;
    case 'D':
        port_map = &d_port;
        break;
    case 'X':
        if (!module_table.count(module_name)) {
            std::string msg = "Module '" + module_name + "' is not defined.";
            error(ast->module_name, msg.c_str());
            return ast;
        }
        port_map = &module_port_indices[module_table[module_name]];
        break;
    default:
        error(ast->instance_name,
              "Instance name must start with R, L, C, M, J, B, D or X.");
        return ast;
    }
    if (!ast->use_named_port) {
        for (ModuleInstAst::Port &port : ast->ports) {
            port.positional = sema_generic(port.positional);
            if (port.positional->type != AST_TYPE_ID &&
                port.positional->type != AST_TYPE_SUBSCRIPT) {
                std::string msg = "Port " +
                                  std::to_string(&port - &ast->ports[0]) +
                                  " must be connected to a wire.";
                std::cerr << "Port " << msg << "\n";
                exit(1);
            }
        }
        return ast;
    }
    std::vector<std::pair<short, Ast *>> ports;
    for (ModuleInstAst::Port &port : ast->ports) {
        std::string port_name = get_file_content(port.named.port_name);
        if (!port_map->count(port_name)) {
            std::string msg = "Port '" + port_name +
                              "' is not defined in module '" + module_name +
                              "'.";
            error(port.named.port_name, msg.c_str());
            return ast;
        }
        port.named.expr = sema_generic(port.named.expr);
        if (port.named.expr->type != AST_TYPE_ID &&
            port.named.expr->type != AST_TYPE_SUBSCRIPT) {
            std::string msg =
                "Port '" + port_name + "' must be connected to a wire.";
            std::cerr << "Port " << msg << "\n";
            exit(1);
        }
        ports.push_back({(*port_map).at(port_name), port.named.expr});
    }
    if (ports.size() != port_map->size()) {
        std::string msg =
            "Module '" + module_name + "' expects " +
            std::to_string(module_table[module_name]->ports.size()) +
            " ports, but " + std::to_string(ports.size()) + " were provided.";
        error(ast->module_name, msg.c_str());
        return ast;
    }
    sort(ports.begin(), ports.end(),
         [](const std::pair<short, Ast *> &a,
            const std::pair<short, Ast *> &b) { return a.first < b.first; });
    ast->use_named_port = false;
    for (size_t i = 0; i < ports.size(); i++) {
        ast->ports[i].positional = ports[i].second;
    }
    for (size_t i = 0; i < ast->params.size(); i++) {
        ast->params[i] = (BinaryOpAst *) sema_generic(ast->params[i]);
    }
    return ast;
}

static Ast *sema_module_decl(ModuleDeclAst *ast)
{
    std::string name = get_file_content(ast->name);
    if (module_table.count(name)) {
        std::string msg = "Module '" + name + "' is already defined.";
        error(ast->name, msg.c_str());
        return ast;
    }
    for (Ast *&stmt : ast->body) {
        stmt = sema_generic(stmt);
    }
    module_table[name] = ast;
    for (size_t i = 0; i < ast->ports.size(); i++) {
        std::string port_name = get_file_content(ast->ports[i]);
        if (module_port_indices[ast].count(port_name)) {
            std::string msg = "Port '" + port_name +
                              "' is already defined in module '" + name + "'.";
            error(ast->ports[i], msg.c_str());
            return ast;
        }
        module_port_indices[ast][port_name] = i;
    }
    for (size_t i = 0; i < ast->params.size(); i++) {
        ast->params[i] = (BinaryOpAst *) sema_generic(ast->params[i]);
    }
    return ast;
}

inline static Ast *sema_generic(Ast *ast, bool may_be_genvar)
{
    switch (ast->type) {
    case AST_TYPE_BINARY_OP:
        return sema_binary_op(static_cast<BinaryOpAst *>(ast), may_be_genvar);
    case AST_TYPE_ID:
        return sema_id(static_cast<IdAst *>(ast));
    case AST_TYPE_NUMBER:
        return sema_number(static_cast<NumberAst *>(ast), may_be_genvar);
    case AST_TYPE_MODULE_DECL:
        return sema_module_decl(static_cast<ModuleDeclAst *>(ast));
    case AST_TYPE_UNARY_OP:
        return sema_unary_op(static_cast<UnaryAst *>(ast), may_be_genvar);
    case AST_TYPE_SUBSCRIPT:
        return sema_subscript(static_cast<SubscriptAst *>(ast), may_be_genvar);
    case AST_TYPE_MODULE_INST:
        return sema_module_inst(static_cast<ModuleInstAst *>(ast));
    case AST_TYPE_WIRE_DECL:
        return ast;
    case AST_TYPE_SYSTEM_FUNC_CALL:
        return sema_system_func_call(static_cast<SystemFuncCallAst *>(ast),
                                     may_be_genvar);
    default:
        return ast;
    }
}

std::vector<Ast *> sema(const std::vector<Ast *> &asts)
{
    module_table.clear();
    std::vector<Ast *> result;
    for (Ast *ast : asts) {
        result.push_back(sema_generic(ast));
    }
    return result;
}
