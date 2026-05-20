#include <iostream>

#include "defs.h"
#include "log.h"
#include "print.h"

static void print_module_decl(const Ast *ast);
static void print_module_inst(const Ast *ast);
static void print_wire_decl(const Ast *ast);
static void print_assign(const Ast *ast);
static void print_subscript(const Ast *ast);
static void print_system_func_call(const Ast *ast);

static bool in_global;

const void print_generic(const Ast *ast)
{
    if (!ast) {
        return;
    }
    switch (ast->type) {
    case AST_TYPE_MODULE_DECL:
        print_module_decl(ast);
        break;
    case AST_TYPE_MODULE_INST:
        print_module_inst(ast);
        break;
    case AST_TYPE_WIRE_DECL:
        print_wire_decl(ast);
        break;
    case AST_TYPE_SUBSCRIPT:
        print_subscript(ast);
        break;
    case AST_TYPE_ID:
        std::cout << get_file_content(static_cast<const IdAst *>(ast)->name);
        break;
    case AST_TYPE_SYSTEM_FUNC_CALL:
        print_system_func_call(ast);
        break;
    default:
        std::cerr << "Unsupported AST type in print: " << ast->type << "\n";
        break;
    }
    return;
}

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

static void print_system_func_call(const Ast *tmp_ast)
{
    SystemFuncCallAst *ast = (SystemFuncCallAst *) tmp_ast;
    std::string func_name = get_file_content(ast->func_id);
    if (func_name == "$spice") {
        std::string code =
            get_file_content(static_cast<StrAst *>(ast->args[0])->str);
        for (int i = 1; i < code.size() - 1; i++) {
            if (code[i] == '\\') {
                switch (code[i + 1]) {
                case 'n':
                    std::cout << "\n";
                    break;
                case 't':
                    std::cout << "\t";
                    break;
                case '\\':
                    std::cout << "\\";
                    break;
                case '"':
                    std::cout << "\"";
                    break;
                default:
                    critical(
                        {static_cast<StrAst *>(ast->args[0])->str.start + i, 2},
                        "Invalid escape sequence in $spice string literal.");
                }
                i++;
            } else {
                std::cout << code[i];
            }
        }
        std::cout << "\n";
    }
}

static void print_subscript(const Ast *tmp_ast)
{
    SubscriptAst *ast = (SubscriptAst *) tmp_ast;
    if (ast->array->type != AST_TYPE_ID) {
        print_subscript(ast);
    } else {
        IdAst *id = (IdAst *) ast->array;
        std::string name = get_file_content(id->name);
        std::cout << name;
    }
    std::cout << "_arr";
    std::cout << ((NumberAst *) ast->index1)->int_value;
    if (ast->index2) {
        std::cout << "t" << ((NumberAst *) ast->index2)->int_value;
    }
}

static void print_module_decl(const Ast *tmp_ast)
{
    ModuleDeclAst *ast = (ModuleDeclAst *) tmp_ast;
    in_global = false;
    std::string name = get_file_content(ast->name);
    std::cout << ".subckt " << name;
    for (const Loc port : ast->ports) {
        std::string port_name = get_file_content(port);
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

        Loc param_name_pos = static_cast<IdAst *>(param->lhs)->name;

        std::string param_name = get_file_content(param_name_pos);
        std::cout << " " << param_name << "=";
        print_number(static_cast<NumberAst *>(param->rhs));
    }
    std::cout << "\n";
    for (const Ast *ast : ast->body) {
        print_generic(ast);
    }
    std::cout << ".ends\n\n";
}

static void print_module_inst(const Ast *tmp_ast)
{
    ModuleInstAst *ast = (ModuleInstAst *) tmp_ast;
    std::string module_name = get_file_content(ast->module_name);
    std::string instance_name = get_file_content(ast->instance_name);
    std::cout << instance_name;
    for (const ModuleInstAst::Port port : ast->ports) {
        std::cout << " ";
        print_generic(port.positional);
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

        Loc param_name_pos = static_cast<IdAst *>(param->lhs)->name;

        std::string param_name = get_file_content(param_name_pos);
        std::cout << " " << param_name << "=";
        print_number(static_cast<NumberAst *>(param->rhs));
    }
    std::cout << "\n";
}

static void print_wire_decl(const Ast *tmp_ast)
{
    WireDeclAst *ast = (WireDeclAst *) tmp_ast;
    if (in_global) {
        std::string name = get_file_content(ast->name);
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
    Loc lhs_name = static_cast<IdAst *>(ast->lhs)->name;
    Loc rhs_name = static_cast<IdAst *>(ast->rhs)->name;
    std::string lhs = get_file_content(lhs_name);
    std::string rhs = get_file_content(rhs_name);
    std::cout << ".connect " << lhs << " " << rhs << "\n";
}

void print(const std::vector<Ast *> &asts)
{
    for (const Ast *ast : asts) {
        in_global = true;
        print_generic(ast);
    }
}
