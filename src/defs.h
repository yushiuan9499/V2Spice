#ifndef DEFS_H
#define DEFS_H

#include <vector>

enum TokenType {
    TOKEN_TYPE_NONE,
    TOKEN_TYPE_EOF,
    TOKEN_TYPE_LPAREN,      // (
    TOKEN_TYPE_RPAREN,      // )
    TOKEN_TYPE_SEMICOLON,   // ;
    TOKEN_TYPE_EQUAL,       // =
    TOKEN_TYPE_HASHTAG,     // #
    TOKEN_TYPE_COMMA,       // ,
    TOKEN_TYPE_DOT,         // ,
    TOKEN_TYPE_ASSIGN,      // assign
    TOKEN_TYPE_MODULE,      // module
    TOKEN_TYPE_ENDMODULE,   // endmodule
    TOKEN_TYPE_WIRE,        // wire
    TOKEN_TYPE_PARAM,       // parameter
    TOKEN_TYPE_IDENTIFIER,  // identifier & number
};

struct Idx {
    unsigned start;
    unsigned len;
};

struct Token {
    Idx idx;
    TokenType type;
};

enum AstType {
    AST_TYPE_NONE,
    AST_TYPE_MODULE_DECL,
    AST_TYPE_MODULE_INST,
    AST_TYPE_WIRE_DECL,
    AST_TYPE_ASSIGN,
    AST_TYPE_PARAM_ASSIGN,
    AST_TYPE_NR
};
struct Ast {
    const AstType type;
};

struct WireDeclAst : public Ast {
    Idx name;
    WireDeclAst() : Ast{AST_TYPE_WIRE_DECL} {}
};

struct AssignAst : public Ast {
    Idx lhs;
    Idx rhs;
    AssignAst() : Ast{AST_TYPE_ASSIGN} {}
};

struct ParamAssignAst : public Ast {
    Idx param_name;
    Idx value;
    ParamAssignAst() : Ast{AST_TYPE_PARAM_ASSIGN} {}
};

struct ModuleDeclAst : public Ast {
    Idx name;
    std::vector<ParamAssignAst *> params;
    std::vector<Idx> ports;
    std::vector<Ast *> body;
    ModuleDeclAst() : Ast{AST_TYPE_MODULE_DECL} {}
};

struct ModuleInstAst : public Ast {
    Idx module_name;
    Idx instance_name;
    bool use_named_port;
    std::vector<ParamAssignAst *> params;
    union Port {
        Idx positional;
        struct {
            Idx port_name;
            Idx wire_name;
        } named;
    };
    std::vector<Port> ports;
    ModuleInstAst() : Ast{AST_TYPE_MODULE_INST} {}
};

#endif  // DEFS_H
