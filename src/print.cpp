#include <iostream>

#include "defs.h"
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
    [AST_TYPE_ASSIGN] = print_assign,
};

extern std::string s;
static bool in_global;

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
    for (const ParamAssignAst *param : ast->params) {
        std::string param_name =
            s.substr(param->param_name.start, param->param_name.len);
        std::string value = s.substr(param->value.start, param->value.len);
        std::cout << " " << param_name << "=" << value;
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
    for (const ParamAssignAst *param : ast->params) {
        std::string param_name =
            s.substr(param->param_name.start, param->param_name.len);
        std::string value = s.substr(param->value.start, param->value.len);
        std::cout << " " << param_name << "=" << value;
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

static void print_assign(const Ast *tmp_ast)
{
    AssignAst *ast = (AssignAst *) tmp_ast;
    std::string lhs = s.substr(ast->lhs.start, ast->lhs.len);
    std::string rhs = s.substr(ast->rhs.start, ast->rhs.len);
    std::cout << ".connect " << lhs << " " << rhs << "\n";
}

void print(const std::vector<Ast *> &asts)
{
    for (const Ast *ast : asts) {
        in_global = true;
        print_func[ast->type](ast);
    }
}
