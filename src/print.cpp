#include <iostream>

#include "defs.h"
#include "log.h"
#include "print.h"

static void print_module_decl(const Ast *ast);
static void print_module_inst(const Ast *ast);
static void print_wire_decl(const Ast *ast);
static void print_assign(const Ast *ast);

static void (*print_func[AST_TYPE_NR])(const Ast *) = {
    [AST_TYPE_NONE] = nullptr,
    [AST_TYPE_MODULE_DECL] = print_module_decl,
    [AST_TYPE_MODULE_INST] = print_module_inst,
    [AST_TYPE_WIRE_DECL] = print_wire_decl,
};

extern std::string s;
static bool in_global;


static char exp_to_char(signed char exp)
{
    if (exp == -12) {
        return 'p';
    } else if (exp == -9) {
        return 'n';
    } else if (exp == -6) {
        return 'u';
    } else if (exp == -3) {
        return 'm';
    } else if (exp == 0) {
        return ' ';
    } else if (exp == 3) {
        return 'k';
    } else if (exp == 6) {
        return 'M';
    } else if (exp == 9) {
        return 'G';
    } else if (exp == 12) {
        return 'T';
    } else if (exp == 15) {
        return 'P';
    } else if (exp == 18) {
        return 'E';
    }
    return '?';
}
static void print_number(NumberAst *ast)
{
    if (ast->is_float) {
        if (ast->float_value == 0) {
            std::cout << "0\n";
            return;
        }
        signed char exp = 0;
        while (ast->float_value < 1) {
            ast->float_value *= 1000;
            exp -= 3;
        }
        while (ast->float_value >= 1000) {
            ast->float_value /= 1000;
            exp += 3;
        }
        std::cout << ast->float_value << exp_to_char(exp);
    } else {
        std::cout << ast->int_value;
    }
}

static void print_module_decl(const Ast *tmp_ast)
{
    ModuleDeclAst *ast = (ModuleDeclAst *) tmp_ast;
    in_global = false;
    std::string name = s.substr(ast->name.start, ast->name.len);
    std::cout << ".subckt " << name;
    for (const Idx port : ast->ports) {
        std::string port_name = s.substr(port.start, port.len);
        std::cout << " " << port_name;
    }
    for (const BinaryOpAst *param : ast->params) {
        if (param->op.type != TOKEN_TYPE_EQUAL) {
            std::cerr << "Unexpected binary operator in print: "
                      << token_str[param->op.type] << "\n";
            return;
        }
        if (param->lhs->type != AST_TYPE_ID) {
            std::cerr << "Unexpected LHS in print: expected ID, got "
                      << param->lhs->type << "\n";
            return;
        }
        if (param->rhs->type != AST_TYPE_NUMBER) {
            std::cerr << "Unsupported RHS in print: expected number, got "
                      << param->rhs->type << "\n";
            return;
        }

        Idx param_name_pos = static_cast<IdAst *>(param->lhs)->name;

        std::string param_name =
            s.substr(param_name_pos.start, param_name_pos.len);
        std::cout << " " << param_name << "=";
        print_number(static_cast<NumberAst *>(param->rhs));
    }
    std::cout << "\n";
    for (const Ast *ast : ast->body) {
        print_func[ast->type](ast);
    }
    std::cout << ".ends\n\n";
}

static void print_module_inst(const Ast *tmp_ast)
{
    ModuleInstAst *ast = (ModuleInstAst *) tmp_ast;
    std::string module_name =
        s.substr(ast->module_name.start, ast->module_name.len);
    std::string instance_name =
        s.substr(ast->instance_name.start, ast->instance_name.len);
    std::cout << instance_name;
    for (const ModuleInstAst::Port port : ast->ports) {
        std::string wire_name =
            s.substr(port.positional.start, port.positional.len);
        std::cout << " " << wire_name;
    }
    std::cout << " " << module_name;
    for (const BinaryOpAst *param : ast->params) {
        if (param->op.type != TOKEN_TYPE_EQUAL) {
            std::cerr << "Unexpected binary operator in print: "
                      << token_str[param->op.type] << "\n";
            return;
        }
        if (param->lhs->type != AST_TYPE_ID) {
            std::cerr << "Unexpected LHS in print: expected ID, got "
                      << param->lhs->type << "\n";
            return;
        }
        if (param->rhs->type != AST_TYPE_NUMBER) {
            std::cerr << "Unsupported RHS in print: expected number, got "
                      << param->rhs->type << "\n";
            return;
        }

        Idx param_name_pos = static_cast<IdAst *>(param->lhs)->name;

        std::string param_name =
            s.substr(param_name_pos.start, param_name_pos.len);
        std::cout << " " << param_name << "=";
        print_number(static_cast<NumberAst *>(param->rhs));
    }
    std::cout << "\n";
}

static void print_wire_decl(const Ast *tmp_ast)
{
    WireDeclAst *ast = (WireDeclAst *) tmp_ast;
    if (in_global) {
        std::string name = s.substr(ast->name.start, ast->name.len);
        std::cout << ".global " << name << "\n";
    }
}

static void print_binary_op(const Ast *tmp_ast)
{
    BinaryOpAst *ast = (BinaryOpAst *) tmp_ast;
    if (ast->op.type != TOKEN_TYPE_EQUAL) {
        std::cerr << "Unexpected binary operator in print: "
                  << token_str[ast->op.type] << "\n";
        return;
    }
    if (ast->lhs->type != AST_TYPE_ID) {
        std::cerr << "Unexpected LHS in print: expected ID, got "
                  << ast->lhs->type << "\n";
        return;
    }
    if (ast->rhs->type != AST_TYPE_ID) {
        std::cerr << "Unsupported RHS in print: expected ID, got "
                  << ast->rhs->type << "\n";
        return;
    }
    Idx lhs_name = static_cast<IdAst *>(ast->lhs)->name;
    Idx rhs_name = static_cast<IdAst *>(ast->rhs)->name;
    std::string lhs = s.substr(lhs_name.start, lhs_name.len);
    std::string rhs = s.substr(rhs_name.start, rhs_name.len);
    std::cout << ".connect " << lhs << " " << rhs << "\n";
}

void print(const std::vector<Ast *> &asts)
{
    for (const Ast *ast : asts) {
        in_global = true;
        print_func[ast->type](ast);
    }
}
