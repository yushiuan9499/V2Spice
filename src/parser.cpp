#include <string>

#include "defs.h"
#include "log.h"
#include "parser.h"

static unsigned pos;

static Ast *parse_binary_op(const std::vector<Token> &tokens);

extern std::string s;
static void expect(const std::vector<Token> &tokens,
                   TokenType type,
                   bool is_critical = true)
{
    if (tokens[pos].type != type) {
        std::string msg = "expected token type ";
        msg += token_str[type];
        msg += " but got ";
        if (tokens[pos].type == TOKEN_TYPE_IDENTIFIER ||
            tokens[pos].type == TOKEN_TYPE_NUMBER) {
            msg += " \"" +
                   std::string(
                       s.substr(tokens[pos].idx.start, tokens[pos].idx.len)) +
                   "\"";
        } else {
            msg += token_str[tokens[pos].type];
        }
        if (is_critical) {
            critical(tokens[pos].idx, msg.c_str());
        } else {
            error(tokens[pos].idx, msg.c_str());
        }
    }
}

static void unexpected_token(const std::vector<Token> &tokens,
                             bool is_critical = true)
{
    std::string msg = "unexpected token type ";
    if (tokens[pos].type == TOKEN_TYPE_IDENTIFIER ||
        tokens[pos].type == TOKEN_TYPE_NUMBER) {
        msg +=
            " \"" +
            std::string(s.substr(tokens[pos].idx.start, tokens[pos].idx.len)) +
            "\"";
    } else {
        msg += token_str[tokens[pos].type];
    }
    if (is_critical) {
        critical(tokens[pos].idx, msg.c_str());
    } else {
        error(tokens[pos].idx, msg.c_str());
    }
}

static bool accept(const std::vector<Token> &tokens, TokenType type)
{
    if (tokens[pos].type == type) {
        pos++;
        return true;
    }
    return false;
}

static SystemFuncCallAst *parse_system_func_call(
    const std::vector<Token> &tokens)
{
    SystemFuncCallAst *ast = new SystemFuncCallAst;
    ast->func_id = tokens[pos].idx;
    pos++;

    expect(tokens, TOKEN_TYPE_LPAREN);
    pos++;  // Skip '('
    while (tokens[pos].type != TOKEN_TYPE_RPAREN) {
        ast->args.push_back(parse_binary_op(tokens));
        if (tokens[pos].type == TOKEN_TYPE_COMMA) {
            pos++;  // Skip ','
            continue;
        }
        break;
    }
    expect(tokens, TOKEN_TYPE_RPAREN);
    pos++;  // Skip ')'
    return ast;
}


static Ast *parse_primary(const std::vector<Token> &tokens)
{
    if (accept(tokens, TOKEN_TYPE_LPAREN)) {
        Ast *e = parse_binary_op(tokens);
        expect(tokens, TOKEN_TYPE_RPAREN);
        pos++;  // Skip ')'
        return e;
    }
    if (tokens[pos].type == TOKEN_TYPE_IDENTIFIER) {
        debug(tokens[pos].idx, "parse identifier");
        IdAst *ast = new IdAst;
        ast->name = tokens[pos].idx;
        pos++;
        return ast;
    }
    if (tokens[pos].type == TOKEN_TYPE_NUMBER) {
        debug(tokens[pos].idx, "parse number");
        NumberAst *ast = new NumberAst;
        ast->pos = tokens[pos].idx;
        pos++;
        return ast;
    }
    if (tokens[pos].type == TOKEN_TYPE_STR) {
        StrAst *ast = new StrAst;
        ast->str = tokens[pos].idx;
        pos++;
        return ast;
    }
    if (tokens[pos].type == TOKEN_TYPE_SYSTEM_FUNC) {
        return parse_system_func_call(tokens);
    }
    unexpected_token(tokens);
    return nullptr;  // Unreachable
}

static Ast *parse_subscript(const std::vector<Token> &tokens)
{
    Ast *e = parse_primary(tokens);
    while (accept(tokens, TOKEN_TYPE_LBRACKET)) {
        SubscriptAst *ast = new SubscriptAst;
        ast->array = e;
        ast->index1 = parse_binary_op(tokens);
        if (accept(tokens, TOKEN_TYPE_COLON)) {
            ast->index2 = parse_binary_op(tokens);
        } else {
            ast->index2 = nullptr;
        }
        expect(tokens, TOKEN_TYPE_RBRACKET);
        pos++;  // Skip ']'
        e = ast;
    }
    return e;
}

static Ast *parse_unary_op(const std::vector<Token> &tokens)
{
    Ast *e;
    Ast **now = &e;
    while (accept(tokens, TOKEN_TYPE_PLUS) ||
           accept(tokens, TOKEN_TYPE_MINUS) || accept(tokens, TOKEN_TYPE_NOT)) {
        UnaryAst *ast = new UnaryAst;
        ast->op = tokens[pos - 1];
        *now = ast;
        now = &ast->operand;
    }
    *now = parse_subscript(tokens);
    return e;
}

enum OpPri {
    OP_PRI_NONE,
    OP_PRI_OR,        // ||
    OP_PRI_AND,       // &&
    OP_PRI_EQUALITY,  // ==, !=
    OP_PRI_REL,       // <, >, <=, >=
    OP_PRI_ADD,       // +, -
    OP_PRI_MUL,       // *, /, %
};
inline OpPri get_op_pri(TokenType op)
{
    switch (op) {
    case TOKEN_TYPE_OR:
        return OP_PRI_OR;
    case TOKEN_TYPE_AND:
        return OP_PRI_AND;
    case TOKEN_TYPE_EQUAL2:
    case TOKEN_TYPE_NOT_EQUAL:
        return OP_PRI_EQUALITY;
    case TOKEN_TYPE_LT:
    case TOKEN_TYPE_GT:
    case TOKEN_TYPE_LE:
    case TOKEN_TYPE_GE:
        return OP_PRI_REL;
    case TOKEN_TYPE_PLUS:
    case TOKEN_TYPE_MINUS:
        return OP_PRI_ADD;
    case TOKEN_TYPE_ASTERISK:
    case TOKEN_TYPE_SLASH:
    case TOKEN_TYPE_PERCENT:
        return OP_PRI_MUL;
    default:
        return OP_PRI_NONE;
    }
}

static Ast *parse_binary_op(const std::vector<Token> &tokens)
{
    std::vector<std::pair<Ast *, Token>> ast_stack;
    bool done = false;
    while (!done) {
        Ast *e = parse_unary_op(tokens);
        ast_stack.emplace_back(e, tokens[pos]);
        if (get_op_pri(tokens[pos].type) != OP_PRI_NONE) {
            /* We only consume binary operators */
            pos++;
        } else {
            done = true;
        }
        while (ast_stack.size() > 1) {
            Token rhs_op = ast_stack.back().second;
            Token lhs_op = ast_stack[ast_stack.size() - 2].second;
            if (get_op_pri(lhs_op.type) >= get_op_pri(rhs_op.type)) {
                BinaryOpAst *ast = new BinaryOpAst;
                ast->op = lhs_op;
                ast->lhs = ast_stack[ast_stack.size() - 2].first;
                ast->rhs = ast_stack.back().first;
                ast_stack.pop_back();
                ast_stack.back().first = ast;
                ast_stack.back().second = rhs_op;
            } else {
                break;
            }
        }
    }
    debug(tokens[pos].idx, "parsed binary op");
    return ast_stack[0].first;
}

static void parse_param_list(const std::vector<Token> &tokens,
                             std::vector<BinaryOpAst *> &params)
{
    while (1) {
        BinaryOpAst *ast = new BinaryOpAst;

        expect(tokens, TOKEN_TYPE_IDENTIFIER);
        IdAst *id_ast = new IdAst;
        id_ast->name = tokens[pos].idx;
        ast->lhs = id_ast;
        pos++;

        expect(tokens, TOKEN_TYPE_EQUAL);
        ast->op = tokens[pos];
        pos++;

        ast->rhs = parse_binary_op(tokens);

        params.push_back(ast);
        if (tokens[pos].type == TOKEN_TYPE_COMMA) {
            pos++;  // Skip ','
            if (tokens[pos].type == TOKEN_TYPE_PARAM) {
                break;
            }
            continue;
        }
        break;
    }
}

static void parse_assign(const std::vector<Token> &tokens,
                         std::vector<Ast *> &asts)
{
    BinaryOpAst *ast = new BinaryOpAst;

    expect(tokens, TOKEN_TYPE_IDENTIFIER);
    IdAst *id_ast = new IdAst;
    id_ast->name = tokens[pos].idx;
    ast->lhs = id_ast;
    pos++;

    expect(tokens, TOKEN_TYPE_EQUAL);
    ast->op = tokens[pos];
    pos++;

    ast->rhs = parse_binary_op(tokens);

    asts.push_back(ast);

    expect(tokens, TOKEN_TYPE_SEMICOLON);
    pos++;
}

static void parse_wire_decl(const std::vector<Token> &tokens,
                            std::vector<Ast *> &asts)
{
    expect(tokens, TOKEN_TYPE_IDENTIFIER);
    WireDeclAst *ast = new WireDeclAst;
    ast->name = tokens[pos].idx;
    pos++;

    expect(tokens, TOKEN_TYPE_SEMICOLON);
    asts.push_back(ast);
    pos++;
}

static void parse_module_inst(const std::vector<Token> &tokens,
                              std::vector<Ast *> &asts)
{
    ModuleInstAst *ast = new ModuleInstAst;
    ast->module_name = tokens[pos].idx;
    pos++;

    expect(tokens, TOKEN_TYPE_IDENTIFIER);
    ast->instance_name = tokens[pos].idx;
    pos++;
    if (accept(tokens, TOKEN_TYPE_HASHTAG)) {
        expect(tokens, TOKEN_TYPE_LPAREN);
        pos++;  // Skip '('
        while (tokens[pos].type != TOKEN_TYPE_RPAREN) {
            parse_param_list(tokens, ast->params);
        }
        pos++;  // Skip ')'
    }
    expect(tokens, TOKEN_TYPE_LPAREN);
    pos++;  // Skip '('
    ast->use_named_port = tokens[pos].type == TOKEN_TYPE_DOT;
    if (ast->use_named_port) {
        while (1) {
            expect(tokens, TOKEN_TYPE_DOT);
            pos++;  // Skip '.'
            expect(tokens, TOKEN_TYPE_IDENTIFIER);
            ModuleInstAst::Port port;
            port.named.port_name = tokens[pos].idx;
            pos++;
            expect(tokens, TOKEN_TYPE_LPAREN);
            pos++;  // Skip '('
            port.named.expr = parse_binary_op(tokens);
            expect(tokens, TOKEN_TYPE_RPAREN);
            pos++;  // Skip ')'
            ast->ports.push_back(port);
            if (tokens[pos].type == TOKEN_TYPE_COMMA) {
                pos++;  // Skip ','
                continue;
            }
            break;
        }
        expect(tokens, TOKEN_TYPE_RPAREN);
        pos++;  // Skip ')'
    } else {
        while (1) {
            expect(tokens, TOKEN_TYPE_IDENTIFIER);
            ModuleInstAst::Port port;
            port.positional = parse_binary_op(tokens);
            ast->ports.push_back(port);
            if (tokens[pos].type == TOKEN_TYPE_COMMA) {
                pos++;  // Skip ','
                continue;
            }
            break;
        }
        expect(tokens, TOKEN_TYPE_RPAREN);
        pos++;  // Skip ')'
    }
    expect(tokens, TOKEN_TYPE_SEMICOLON);
    asts.push_back(ast);
    pos++;  // Skip ';'
}

static void parse_module_decl(const std::vector<Token> &tokens,
                              std::vector<Ast *> &asts)
{
    expect(tokens, TOKEN_TYPE_IDENTIFIER);
    ModuleDeclAst *ast = new ModuleDeclAst;
    ast->name = tokens[pos].idx;
    pos++;

    if (accept(tokens, TOKEN_TYPE_HASHTAG)) {
        expect(tokens, TOKEN_TYPE_LPAREN);
        pos++;  // Skip '('
        while (tokens[pos].type != TOKEN_TYPE_RPAREN) {
            accept(tokens, TOKEN_TYPE_PARAM);
            parse_param_list(tokens, ast->params);
        }
        pos++;  // Skip ')'
    }

    if (tokens[pos].type == TOKEN_TYPE_LPAREN) {
        pos++;  // Skip '('
        while (!accept(tokens, TOKEN_TYPE_RPAREN)) {
            expect(tokens, TOKEN_TYPE_IDENTIFIER);
            ast->ports.push_back(tokens[pos].idx);
            pos++;
            if (tokens[pos].type == TOKEN_TYPE_COMMA) {
                pos++;  // Skip ','
                continue;
            }
            expect(tokens, TOKEN_TYPE_RPAREN);
        }
    }

    expect(tokens, TOKEN_TYPE_SEMICOLON);
    pos++;  // Skip ';'
    while (tokens[pos].type != TOKEN_TYPE_ENDMODULE) {
        if (accept(tokens, TOKEN_TYPE_WIRE)) {
            parse_wire_decl(tokens, ast->body);
            continue;
        }
        if (tokens[pos].type == TOKEN_TYPE_IDENTIFIER) {
            parse_module_inst(tokens, ast->body);
            continue;
        }
        if (accept(tokens, TOKEN_TYPE_ASSIGN)) {
            parse_assign(tokens, ast->body);
            continue;
        }
        unexpected_token(tokens);
    }
    asts.push_back(ast);
    pos++;  // Skip 'endmodule'
}

std::vector<Ast *> parse(const std::vector<Token> &tokens)
{
    pos = 0;
    std::vector<Ast *> asts;
    while (tokens[pos].type != TOKEN_TYPE_EOF) {
        if (accept(tokens, TOKEN_TYPE_MODULE)) {
            parse_module_decl(tokens, asts);
            continue;
        }
        if (accept(tokens, TOKEN_TYPE_WIRE)) {
            parse_wire_decl(tokens, asts);
            continue;
        }
        if (tokens[pos].type == TOKEN_TYPE_SYSTEM_FUNC) {
            /*
             * This does not comply with verilog 2005 standard, but we allow
             * system function call as a statement for simplicity.
             */
            asts.push_back(parse_system_func_call(tokens));
            expect(tokens, TOKEN_TYPE_SEMICOLON);
            pos++;  // Skip ';'
            continue;
        }
        unexpected_token(tokens);
    }
    return asts;
}
