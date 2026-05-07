#include <iostream>
#include <map>

#include "defs.h"
#include "lexer.h"

static unsigned pos;

struct Trie {
    std::map<unsigned int, Trie *> children;
    TokenType type;

    Trie() : type(TOKEN_TYPE_IDENTIFIER) {}
};

static Trie *root;
__attribute__((constructor)) static Trie *build_keyword_trie()
{
    root = new Trie();
    const std::pair<const char *, TokenType> keywords[] = {
        {"assign", TOKEN_TYPE_ASSIGN},       {"module", TOKEN_TYPE_MODULE},
        {"endmodule", TOKEN_TYPE_ENDMODULE}, {"wire", TOKEN_TYPE_WIRE},
        {"parameter", TOKEN_TYPE_PARAM},
    };
    for (const auto &kw : keywords) {
        Trie *node = root;
        unsigned int v = 0;
        int i = 0;
        for (const char *c = kw.first; *c;) {
            v = (v << 8) | (unsigned char) *c;
            i++;
            c++;
            if (i % 4 == 0) {
                if (!node->children.count(v)) {
                    node->children[v] = new Trie();
                }
                node = node->children[v];
                v = 0;
            }
        }
        if (v) {
            if (!node->children.count(v)) {
                node->children[v] = new Trie();
            }
            node = node->children[v];
        }
        node->type = kw.second;
    }
    return root;
}

static void skip_whitespace(const std::string &input)
{
    while (pos < input.size() && isspace(input[pos])) {
        pos++;
    }
}

static void skip_cpp_comment(const std::string &input)
{
    while (pos < input.size() && input[pos] != '\n') {
        pos++;
    }
    if (pos < input.size()) {
        pos++;  // Skip the newline character
    }
}

static void skip_c_comment(const std::string &input)
{
    while (pos < input.size() &&
           !(input[pos] == '*' && pos + 1 < input.size() &&
             input[pos + 1] == '/')) {
        pos++;
    }
    if (pos < input.size()) {
        pos += 2;  // Skip the closing */
    }
}


std::vector<Token> lex(const std::string &input)
{
    pos = 0;
    std::vector<Token> tokens;
    while (pos < input.size()) {
        skip_whitespace(input);
        if (pos >= input.size()) {
            break;
        }
        if (input[pos] == '/' && pos + 1 < input.size()) {
            if (input[pos + 1] == '/') {
                skip_cpp_comment(input);
                continue;
            } else if (input[pos + 1] == '*') {
                skip_c_comment(input);
                continue;
            }
        }
        if (input[pos] == '(') {
            tokens.push_back({pos, 1, TOKEN_TYPE_LPAREN});
            pos++;
            continue;
        }
        if (input[pos] == ')') {
            tokens.push_back({pos, 1, TOKEN_TYPE_RPAREN});
            pos++;
            continue;
        }
        if (input[pos] == ';') {
            tokens.push_back({pos, 1, TOKEN_TYPE_SEMICOLON});
            pos++;
            continue;
        }
        if (input[pos] == '=') {
            tokens.push_back({pos, 1, TOKEN_TYPE_EQUAL});
            pos++;
            continue;
        }
        if (input[pos] == '#') {
            tokens.push_back({pos, 1, TOKEN_TYPE_HASHTAG});
            pos++;
            continue;
        }
        if (input[pos] == ',') {
            tokens.push_back({pos, 1, TOKEN_TYPE_COMMA});
            pos++;
            continue;
        }
        if (input[pos] == '.') {
            tokens.push_back({pos, 1, TOKEN_TYPE_DOT});
            pos++;
            continue;
        }
        unsigned start = pos;
        Trie *node = root;
        unsigned int v = 0;
        while (pos < input.size() && (isalnum(input[pos]) ||
                                      input[pos] == '_' || input[pos] == '.')) {
            v = (v << 8) | (unsigned char) input[pos];
            pos++;
            if ((pos - start) % 4 == 0) {
                if (node && node->children.count(v)) {
                    node = node->children[v];
                } else {
                    node = nullptr;
                }
                v = 0;
            }
        }
        if (v) {
            if (node && node->children.count(v)) {
                node = node->children[v];
            } else {
                node = nullptr;
            }
        }
        if (pos < input.size() && input[pos] != '(' && input[pos] != ')' &&
            input[pos] != ';' && input[pos] != '=' && input[pos] != '#' &&
            input[pos] != ',' && input[pos] != '.' && !isspace(input[pos])) {
            std::cerr << "Unexpected character '" << input[pos]
                      << "' at position " << pos << "\n";
            exit(1);
        }
        tokens.push_back({start, (unsigned short) (pos - start),
                          node ? node->type : TOKEN_TYPE_IDENTIFIER});
    }
    tokens.push_back({(unsigned) input.size(), 0, TOKEN_TYPE_EOF});
    return tokens;
}
