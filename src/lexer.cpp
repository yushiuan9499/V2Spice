#include <map>

#include "defs.h"
#include "lexer.h"
#include "log.h"

static unsigned pos;

struct KeywordTrie {
    std::map<unsigned int, KeywordTrie *> children;
    TokenType type;

    KeywordTrie() : type(TOKEN_TYPE_IDENTIFIER) {}
};

static KeywordTrie *keyword_root;
__attribute__((constructor)) static KeywordTrie *build_keyword_trie()
{
    keyword_root = new KeywordTrie();
    std::pair<const char *, TokenType>
        keywords[TOKEN_TYPE_KEYWORD_END - TOKEN_TYPE_PUNCT_END];
    for (int i = 0; i < TOKEN_TYPE_KEYWORD_END - TOKEN_TYPE_PUNCT_END; i++) {
        keywords[i] = {token_str[TOKEN_TYPE_PUNCT_END + 1 + i],
                       (TokenType) (TOKEN_TYPE_PUNCT_END + 1 + i)};
    }
    for (const auto &kw : keywords) {
        KeywordTrie *node = keyword_root;
        unsigned int v = 0;
        int i = 0;
        for (const char *c = kw.first; *c;) {
            v = (v << 8) | (unsigned char) *c;
            i++;
            c++;
            if (i % 4 == 0) {
                if (!node->children.count(v)) {
                    node->children[v] = new KeywordTrie();
                }
                node = node->children[v];
                v = 0;
            }
        }
        if (v) {
            if (!node->children.count(v)) {
                node->children[v] = new KeywordTrie();
            }
            node = node->children[v];
        }
        node->type = kw.second;
    }
    return keyword_root;
}

struct PunctTrie {
    std::map<char, PunctTrie *> children;
    TokenType type;

    PunctTrie() : type(TOKEN_TYPE_NONE) {}
};

PunctTrie *punct_root;
__attribute__((constructor)) static void build_punct_trie()
{
    punct_root = new PunctTrie();
    std::pair<const char *, TokenType>
        puncts[TOKEN_TYPE_PUNCT_END - TOKEN_TYPE_EOF];
    for (int i = 0; i < TOKEN_TYPE_PUNCT_END - TOKEN_TYPE_EOF; i++) {
        puncts[i] = {token_str[TOKEN_TYPE_EOF + 1 + i],
                     (TokenType) (TOKEN_TYPE_EOF + 1 + i)};
    }
    for (const auto &p : puncts) {
        PunctTrie *node = punct_root;
        for (const char *c = p.first; *c; c++) {
            if (!node->children.count(*c)) {
                node->children[*c] = new PunctTrie();
            }
            node = node->children[*c];
        }
        node->type = p.second;
    }
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

        if (isdigit(input[pos])) {
            /* Number */
            unsigned start = pos;
            while (pos < input.size() &&
                   (isalnum(input[pos]) || input[pos] == '.')) {
                pos++;
            }
            tokens.push_back(
                {start, (unsigned short) (pos - start), TOKEN_TYPE_NUMBER});
            continue;
        }

        if (isalpha(input[pos]) || input[pos] == '_') {
            unsigned start = pos;
            KeywordTrie *node = keyword_root;
            unsigned int v = 0;
            while (pos < input.size() &&
                   (isalnum(input[pos]) || input[pos] == '_' ||
                    input[pos] == '.')) {
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
            tokens.push_back({start, (unsigned short) (pos - start),
                              node ? node->type : TOKEN_TYPE_IDENTIFIER});
            continue;
        }

        unsigned start = pos;
        PunctTrie *node = punct_root;
        while (pos < input.size() && node->children.count(input[pos])) {
            node = node->children[input[pos]];
            pos++;
        }
        if (node->type == TOKEN_TYPE_NONE || pos == start) {
            std::string msg = "Unexpected character '";
            msg += input[pos];
            msg += "' at position " + std::to_string(pos);
            critical({pos, 1}, msg.c_str());
        }
        tokens.push_back({start, (unsigned short) (pos - start), node->type});
    }
    tokens.push_back({(unsigned) input.size(), 0, TOKEN_TYPE_EOF});
    return tokens;
}
