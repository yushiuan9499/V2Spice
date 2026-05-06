#include <fstream>
#include <iostream>

#include "lexer.h"
#include "parser.h"
#include "print.h"
#include "sema.h"

extern std::string s;

#define VERBOSE 0

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>\n";
        return 1;
    }
    const char *input_file = argv[1];
    getline(std::ifstream(input_file), s, '\0');
    std::vector<Token> tokens = lex(s);
    std::vector<Ast *> asts = parse(tokens);
#if VERBOSE
    std::cerr << asts.size() << " ASTs generated.\n";
#endif
    asts = sema(asts);
#if VERBOSE
    std::cerr << asts.size() << " ASTs generated.\n";
#endif
    print(asts);
    return 0;
}
