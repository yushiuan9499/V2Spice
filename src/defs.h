#ifndef DEFS_H
#define DEFS_H

#include <vector>

enum TokenType {
    TOKEN_TYPE_NONE,
    TOKEN_TYPE_EOF,
    TOKEN_TYPE_LPAREN,     // (
    TOKEN_TYPE_RPAREN,     // )
    TOKEN_TYPE_LBRACKET,   // [
    TOKEN_TYPE_RBRACKET,   // ]
    TOKEN_TYPE_SEMICOLON,  // ;
    TOKEN_TYPE_HASHTAG,    // #
    TOKEN_TYPE_DOT,        // .
    /* Following are binary operators. */
    TOKEN_TYPE_COMMA,      // ,
    TOKEN_TYPE_EQUAL,      // =
    TOKEN_TYPE_EQUAL2,     // ==
    TOKEN_TYPE_NOT_EQUAL,  // !=
    TOKEN_TYPE_LT,         // <
    TOKEN_TYPE_GT,         // >
    TOKEN_TYPE_LE,         // <=
    TOKEN_TYPE_GE,         // >=
    TOKEN_TYPE_NOT,        // !
    TOKEN_TYPE_AND,        // &&
    TOKEN_TYPE_OR,         // ||
    TOKEN_TYPE_QUESTION,   // ?
    TOKEN_TYPE_COLON,      // :
    TOKEN_TYPE_PLUS,       // +
    TOKEN_TYPE_MINUS,      // -
    TOKEN_TYPE_ASTERISK,   // *
    TOKEN_TYPE_SLASH,      // /
    TOKEN_TYPE_PERCENT,    // %
    TOKEN_TYPE_PUNCT_END = TOKEN_TYPE_PERCENT,

    TOKEN_TYPE_ASSIGN,     // assign
    TOKEN_TYPE_MODULE,     // module
    TOKEN_TYPE_ENDMODULE,  // endmodule
    TOKEN_TYPE_WIRE,       // wire
    TOKEN_TYPE_PARAM,      // parameter
    TOKEN_TYPE_FOR,        // for
    TOKEN_TYPE_GENVAR,     // genvar
    TOKEN_TYPE_KEYWORD_END = TOKEN_TYPE_GENVAR,

    TOKEN_TYPE_NUMBER,      // number literal
    TOKEN_TYPE_IDENTIFIER,  // identifier
    TOKEN_TYPE_NR
};

static const char *token_str[TOKEN_TYPE_NR] = {
    [TOKEN_TYPE_NONE] = "",
    [TOKEN_TYPE_EOF] = "<EOF>",
    [TOKEN_TYPE_LPAREN] = "(",
    [TOKEN_TYPE_RPAREN] = ")",
    [TOKEN_TYPE_LBRACKET] = "[",
    [TOKEN_TYPE_RBRACKET] = "]",
    [TOKEN_TYPE_SEMICOLON] = ";",
    [TOKEN_TYPE_HASHTAG] = "#",
    [TOKEN_TYPE_DOT] = ".",
    [TOKEN_TYPE_COMMA] = ",",
    [TOKEN_TYPE_EQUAL] = "=",
    [TOKEN_TYPE_EQUAL2] = "==",
    [TOKEN_TYPE_NOT_EQUAL] = "!=",
    [TOKEN_TYPE_LT] = "<",
    [TOKEN_TYPE_GT] = ">",
    [TOKEN_TYPE_LE] = "<=",
    [TOKEN_TYPE_GE] = ">=",
    [TOKEN_TYPE_NOT] = "!",
    [TOKEN_TYPE_AND] = "&&",
    [TOKEN_TYPE_OR] = "||",
    [TOKEN_TYPE_QUESTION] = "?",
    [TOKEN_TYPE_COLON] = ":",
    [TOKEN_TYPE_PLUS] = "+",
    [TOKEN_TYPE_MINUS] = "-",
    [TOKEN_TYPE_ASTERISK] = "*",
    [TOKEN_TYPE_SLASH] = "/",
    [TOKEN_TYPE_PERCENT] = "%",
    [TOKEN_TYPE_ASSIGN] = "assign",
    [TOKEN_TYPE_MODULE] = "module",
    [TOKEN_TYPE_ENDMODULE] = "endmodule",
    [TOKEN_TYPE_WIRE] = "wire",
    [TOKEN_TYPE_PARAM] = "parameter",
    [TOKEN_TYPE_FOR] = "for",
    [TOKEN_TYPE_GENVAR] = "genvar",
    [TOKEN_TYPE_NUMBER] = "<number>",
    [TOKEN_TYPE_IDENTIFIER] = "<identifier>"};

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
    AST_TYPE_SUBSCRIPT,
    AST_TYPE_BINARY_OP,
    AST_TYPE_ID,
    AST_TYPE_NUMBER,
    AST_TYPE_UNARY_OP,
    AST_TYPE_NR
};
struct Ast {
    const AstType type;
    Ast(AstType type) : type(type) {}
    virtual ~Ast() = default;
};

struct IdAst : public Ast {
    Idx name;
    IdAst() : Ast{AST_TYPE_ID} {}
};

struct NumberAst : public Ast {
    bool is_float;  // genvar is not float. other number literals are float.
    union {
        long long int_value;
        double float_value;
    };
    Idx pos;
    NumberAst() : Ast{AST_TYPE_NUMBER} {}
};

struct UnaryAst : public Ast {
    Token op;
    Ast *operand;
    UnaryAst() : Ast{AST_TYPE_UNARY_OP}, operand(nullptr) {}
    ~UnaryAst() { delete operand; }
};

struct SubscriptAst : public Ast {
    Ast *array;
    Ast *index1;
    Ast *index2;
    SubscriptAst()
        : Ast{AST_TYPE_SUBSCRIPT},
          array(nullptr),
          index1(nullptr),
          index2(nullptr)
    {
    }
    ~SubscriptAst()
    {
        delete array;
        delete index1;
        delete index2;
    }
};

struct BinaryOpAst : public Ast {
    Token op;
    Ast *lhs;
    Ast *rhs;
    BinaryOpAst() : Ast{AST_TYPE_BINARY_OP}, lhs(nullptr), rhs(nullptr) {}
    ~BinaryOpAst()
    {
        delete lhs;
        delete rhs;
    }
};

struct WireDeclAst : public Ast {
    Idx name;
    WireDeclAst() : Ast{AST_TYPE_WIRE_DECL} {}
};

struct ModuleDeclAst : public Ast {
    Idx name;
    std::vector<BinaryOpAst *> params;
    std::vector<Idx> ports;
    std::vector<Ast *> body;
    ModuleDeclAst() : Ast{AST_TYPE_MODULE_DECL} {}
    ~ModuleDeclAst()
    {
        for (auto param : params) {
            delete param;
        }
        for (auto stmt : body) {
            delete stmt;
        }
    }
};

struct ModuleInstAst : public Ast {
    Idx module_name;
    Idx instance_name;
    bool use_named_port;
    std::vector<BinaryOpAst *> params;
    union Port {
        Ast *positional;
        struct {
            Idx port_name;
            Ast *expr;
        } named;
    };
    std::vector<Port> ports;
    ModuleInstAst() : Ast{AST_TYPE_MODULE_INST} {}
    ~ModuleInstAst()
    {
        for (auto param : params) {
            delete param;
        }
    }
};

#endif  // DEFS_H
