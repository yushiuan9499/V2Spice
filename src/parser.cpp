#include <string>

#include "defs.h"
#include "log.h"
#include "parser.h"

static unsigned pos;

extern std::string s;
static void expect(const std::vector<Token> &tokens,
                   TokenType type,
                   bool is_critical = true)
{
    if (tokens[pos].type != type) {
        std::string s = "expected token type ";
        s += token_str[type];
        s += " but got ";
        if (tokens[pos].type == TOKEN_TYPE_IDENTIFIER) {
            s += " \"" +
                 std::string(
                     s.substr(tokens[pos].idx.start, tokens[pos].idx.len)) +
                 "\"";
        } else {
            s += token_str[tokens[pos].type];
        }
        if (is_critical) {
            critical(tokens[pos].idx, s.c_str());
        } else {
            error(tokens[pos].idx, s.c_str());
        }
    }
}

static void unexpected_token(const std::vector<Token> &tokens,
                             bool is_critical = true)
{
    std::string s = "unexpected token type ";
    if (tokens[pos].type == TOKEN_TYPE_IDENTIFIER) {
        s += " \"" +
             std::string(s.substr(tokens[pos].idx.start, tokens[pos].idx.len)) +
             "\"";
    } else {
        s += token_str[tokens[pos].type];
    }
    if (is_critical) {
        critical(tokens[pos].idx, s.c_str());
    } else {
        error(tokens[pos].idx, s.c_str());
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

static void parse_param_list(const std::vector<Token> &tokens,
                             std::vector<ParamAssignAst *> &params)
{
    while (1) {
        ParamAssignAst *ast = new ParamAssignAst;

        expect(tokens, TOKEN_TYPE_IDENTIFIER);
        ast->param_name = tokens[pos].idx;
        pos++;

        expect(tokens, TOKEN_TYPE_EQUAL);
        pos++;

        expect(tokens, TOKEN_TYPE_IDENTIFIER);
        ast->value = tokens[pos].idx;
        pos++;

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
    AssignAst *ast = new AssignAst;

    expect(tokens, TOKEN_TYPE_IDENTIFIER);
    ast->lhs = tokens[pos].idx;
    pos++;

    expect(tokens, TOKEN_TYPE_ASSIGN);
    pos++;

    expect(tokens, TOKEN_TYPE_IDENTIFIER);
    ast->rhs = tokens[pos].idx;
    pos++;

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

    asts.push_back(ast);

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
            expect(tokens, TOKEN_TYPE_IDENTIFIER);
            port.named.wire_name = tokens[pos].idx;
            pos++;
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
            port.positional = tokens[pos].idx;
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
        unexpected_token(tokens);
    }
    return asts;
}
