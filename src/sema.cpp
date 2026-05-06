#include <algorithm>
#include <iostream>
#include <map>
#include <string>

#include "sema.h"

extern std::string s;

static std::map<std::string, ModuleDeclAst *> module_table;
static std::map<ModuleDeclAst *, std::map<std::string, short>>
    module_port_indices;

static Ast *sema_module_inst(ModuleInstAst *ast)
{
    std::string module_name =
        s.substr(ast->module_name.start, ast->module_name.len);
    if (!module_table.count(module_name)) {
        std::cerr << "Error: Module '" << module_name << "' is not defined.\n";
        exit(1);
    }
    if (!ast->use_named_port) {
        return ast;
    }
    std::vector<std::pair<short, Idx>> ports;
    for (const ModuleInstAst::Port &port : ast->ports) {
        std::string port_name =
            s.substr(port.named.port_name.start, port.named.port_name.len);
        if (!module_port_indices[module_table[module_name]].count(port_name)) {
            std::cerr << "Error: Port '" << port_name
                      << "' is not defined in module '" << module_name
                      << "'.\n";
            exit(1);
        }
        ports.push_back(
            {module_port_indices[module_table[module_name]][port_name],
             port.named.wire_name});
    }
    if (ports.size() != module_table[module_name]->ports.size()) {
        std::cerr << "Error: Module '" << module_name << "' expects "
                  << module_table[module_name]->ports.size() << " ports, but "
                  << ports.size() << " were provided.\n";
        exit(1);
    }
    sort(ports.begin(), ports.end(),
         [](const std::pair<short, Idx> &a, const std::pair<short, Idx> &b) {
             return a.first < b.first;
         });
    ast->use_named_port = false;
    for (size_t i = 0; i < ports.size(); i++) {
        ast->ports[i].positional = ports[i].second;
    }
    return ast;
}

static Ast *sema_module_decl(ModuleDeclAst *ast)
{
    std::string name = s.substr(ast->name.start, ast->name.len);
    if (module_table.count(name)) {
        std::cerr << "Error: Module '" << name << "' is already defined.\n";
        exit(1);
    }
    for (Ast *&stmt : ast->body) {
        if (stmt->type == AST_TYPE_MODULE_INST) {
            stmt = sema_module_inst(static_cast<ModuleInstAst *>(stmt));
        }
    }
    module_table[name] = ast;
    for (size_t i = 0; i < ast->ports.size(); i++) {
        std::string port_name =
            s.substr(ast->ports[i].start, ast->ports[i].len);
        if (module_port_indices[ast].count(port_name)) {
            std::cerr << "Error: Port '" << port_name
                      << "' is already defined in module '" << name << "'.\n";
            exit(1);
        }
        module_port_indices[ast][port_name] = i;
    }
    return ast;
}

std::vector<Ast *> sema(const std::vector<Ast *> &asts)
{
    module_table.clear();
    std::vector<Ast *> result;
    for (Ast *ast : asts) {
        if (ast->type == AST_TYPE_MODULE_DECL) {
            result.push_back(
                sema_module_decl(static_cast<ModuleDeclAst *>(ast)));
        } else if (ast->type == AST_TYPE_MODULE_INST) {
            result.push_back(ast);
        }
    }
    return result;
}
