#include <filesystem>
#include <map>

#include "defs.h"
#include "lexer.h"
#include "log.h"

struct Frame {
    int fd;
    unsigned pos;
};

const unsigned MAX_STACK_SIZE = 32;
class Stack
{
private:
    Frame frames[MAX_STACK_SIZE];
    unsigned top = 0;

public:
    void push(int fd, unsigned pos) { frames[top++] = {fd, pos}; }
    void pop() { top--; }
    Frame &current() { return frames[top - 1]; }
    int size() { return top; }
} stack;

static std::map<std::string, std::string> macros;

#define curr_char (get_file_content(stack.current().fd)[stack.current().pos])
#define next_char(x) \
    (get_file_content(stack.current().fd)[stack.current().pos + x])
#define is_curr_back() \
    (stack.current().pos == get_file_content(stack.current().fd).size() - 1)
#define advance()                                          \
    do {                                                   \
        stack.current().pos++;                             \
        if (stack.current().pos >=                         \
            get_file_content(stack.current().fd).size()) { \
            stack.pop();                                   \
        }                                                  \
    } while (0)

struct KeywordTrie {
    std::map<unsigned int, KeywordTrie *> children;
    TokenType type;

    KeywordTrie() : type(TOKEN_TYPE_IDENTIFIER) {}
};

static KeywordTrie *keyword_root;
__attribute__((constructor)) static void build_keyword_trie()
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

static void skip_whitespace()
{
    while (stack.size() != 0 && isspace(curr_char)) {
        advance();
    }
}

static void skip_cpp_comment()
{
    while (stack.size() != 0 && curr_char != '\n') {
        advance();
    }
    advance();
}

static void skip_c_comment()
{
    bool was_star = false;
    while (1) {
        was_star = curr_char == '*';
        if (is_curr_back()) {
            std::string msg = "Expect '*/' to close the comment";
            critical({stack.current().pos, 1}, msg.c_str());
        }
        advance();
        if (was_star && curr_char == '/') {
            break;
        }
    }
}

static std::string search_include_path(const Loc &filename)
{
    const std::string &curr_file = files[stack.current().fd].name;
    std::string curr_dir = curr_file.substr(0, curr_file.find_last_of('/'));
    std::string path = curr_dir + "/" + get_file_content(filename);
    if (std::filesystem::exists(path)) {
        return path;
    }
    std::string msg = "Included file '" + get_file_content(filename) +
                      "' not found in the same directory";
    critical(filename, msg.c_str());
}

static void process_include(std::vector<Token> &tokens)
{
    if (stack.size() == MAX_STACK_SIZE - 1) {
        std::string msg = "Too many nested includes (max " +
                          std::to_string(MAX_STACK_SIZE) + ")";
        critical({stack.current().pos, 1, stack.current().fd}, msg.c_str());
    }
    advance();
    while (curr_char != '\n' && isspace(curr_char)) {
        advance();
    }
    if (curr_char != '"') {
        std::string msg = "Expected '\"' after `include";
        critical({stack.current().pos, 1, stack.current().fd}, msg.c_str());
    }
    advance();
    unsigned start = stack.current().pos;
    while (stack.size() != 0 && curr_char != '"') {
        if (curr_char == '\\') {
            if (is_curr_back()) {
                std::string msg = "Unexpected end of file in include directive";
                critical({stack.current().pos, 1, stack.current().fd},
                         msg.c_str());
            }
            advance();
        }
        if (is_curr_back()) {
            std::string msg = "Unexpected end of file in include directive";
            critical({stack.current().pos, 1, stack.current().fd}, msg.c_str());
        }
        advance();
    }
    if (stack.size() == 0) {
        std::string msg = "Unexpected end of file in include directive";
        critical({stack.current().pos, 1, stack.current().fd}, msg.c_str());
    }
    int fd = open(search_include_path(
        {start, (unsigned) (stack.current().pos - start), stack.current().fd}));
    advance();
    stack.push(fd, 0);
}


static const std::map<std::string, void (*)(std::vector<Token> &)>
    directives_processors = {{"include", process_include}};
static void process_directive(std::vector<Token> &tokens)
{
    if (is_curr_back()) {
        std::string msg = "Unexpected end of file after '`'";
        critical({stack.current().pos, 1, stack.current().fd}, msg.c_str());
    }
    advance();
    if (!isalpha(curr_char)) {
        std::string msg = "Expected identifier after '`'";
        critical({stack.current().pos, 1, stack.current().fd}, msg.c_str());
    }
    unsigned start = stack.current().pos;
    while (stack.size() != 0 && isalpha(curr_char)) {
        advance();
    }
    std::string directive(get_file_content(
        {start, (unsigned) (stack.current().pos - start), stack.current().fd}));
    if (!directives_processors.count(directive)) {
        std::string msg = "Unknown directive '`" + directive + "'";
        critical({start, (unsigned) (stack.current().pos - start),
                  stack.current().fd},
                 msg.c_str());
    }
    if (!isspace(curr_char)) {
        std::string msg = "Expected space after directive '`" + directive + "'";
        critical({stack.current().pos, 1, stack.current().fd}, msg.c_str());
    }
    directives_processors.at(directive)(tokens);
}


std::vector<Token> lex(int fd)
{
    stack.push(fd, 0);
    std::vector<Token> tokens;
    while (stack.size() != 0) {
        skip_whitespace();
        if (stack.size() == 0) {
            break;
        }

        if (curr_char == '`') {
            process_directive(tokens);
            continue;
        }

        if (curr_char == '/' && !is_curr_back()) {
            if (next_char(1) == '/') {
                skip_cpp_comment();
                continue;
            } else if (next_char(1) == '*') {
                skip_c_comment();
                continue;
            }
        }

        if (curr_char == '"') {
            /* String literal */
            unsigned start = stack.current().pos;
            advance();  // Skip the opening quote
            while (stack.size() != 0) {
                if (curr_char == '\\') {
                    advance();
                    if (is_curr_back()) {
                        std::string msg =
                            "Unexpected end of file in string literal";
                        critical({stack.current().pos, 1}, msg.c_str());
                    }
                    advance();
                } else if (curr_char == '"') {
                    advance();
                    break;
                } else {
                    if (is_curr_back()) {
                        std::string msg =
                            "Unexpected end of file in string literal";
                        critical({stack.current().pos, 1}, msg.c_str());
                    }
                    advance();
                }
            }
            tokens.push_back({start,
                              (unsigned short) (stack.current().pos - start),
                              stack.current().fd, TOKEN_TYPE_STR});
            continue;
        }

        if (isdigit(curr_char)) {
            /* Number */
            unsigned start = stack.current().pos;
            while (stack.size() != 0 &&
                   (isalnum(curr_char) || curr_char == '.')) {
                advance();
            }
            tokens.push_back({start,
                              (unsigned short) (stack.current().pos - start),
                              stack.current().fd, TOKEN_TYPE_NUMBER});
            continue;
        }

        if (curr_char == '$') {
            unsigned start = stack.current().pos;
            advance();  // Skip the '$' character
            while (stack.size() != 0 &&
                   (isalnum(curr_char) || curr_char == '_')) {
                advance();
            }
            if (stack.current().pos == start + 1) {
                std::string msg = "Expected character after '$'";
                critical({start, 1, stack.current().fd}, msg.c_str());
            }
            tokens.push_back({start,
                              (unsigned short) (stack.current().pos - start),
                              stack.current().fd, TOKEN_TYPE_SYSTEM_FUNC});
            continue;
        }

        if (isalpha(curr_char) || curr_char == '_') {
            unsigned start = stack.current().pos;
            KeywordTrie *node = keyword_root;
            unsigned int v = 0;
            while (
                stack.size() != 0 &&
                (isalnum(curr_char) || curr_char == '_' || curr_char == '.')) {
                v = (v << 8) | (unsigned char) curr_char;
                advance();
                if ((stack.current().pos - start) % 4 == 0) {
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
            tokens.push_back({start,
                              (unsigned short) (stack.current().pos - start),
                              stack.current().fd,
                              node ? node->type : TOKEN_TYPE_IDENTIFIER});
            continue;
        }

        unsigned start = stack.current().pos;
        PunctTrie *node = punct_root;
        while (stack.size() != 0 && node->children.count(curr_char)) {
            node = node->children[curr_char];
            advance();
        }
        if (node->type == TOKEN_TYPE_NONE || stack.current().pos == start) {
            std::string msg = "Unexpected character '";
            msg += curr_char;
            msg += "'";
            critical({stack.current().pos, 1, stack.current().fd}, msg.c_str());
        }
        tokens.push_back({start, (unsigned short) (stack.current().pos - start),
                          stack.current().fd, node->type});
    }
    tokens.push_back({(unsigned) 0, 0, 0, TOKEN_TYPE_EOF});
    macros.clear();
    return tokens;
}
