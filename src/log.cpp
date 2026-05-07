#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "log.h"

#define HEADER "\033[95m"
#define OKBLUE "\033[94m"
#define OKCYAN "\033[96m"
#define OKGREEN "\033[92m"
#define WARNING "\033[93m"
#define PURPLE "\033[95m"
#define FAIL "\033[91m"
#define ENDC "\033[0m"
#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"

std::string s;

static LogLevel log_level = LOG_LEVEL_INFO;
static std::vector<int> newlines;
static int _error_count = 0;

/*
 * This function must be called before any logging function is called and after
 * s is set.
 */
void init_log()
{
    _error_count = 0;
    newlines.clear();
    newlines.push_back(-1);
    /* Compute the positions of newlines in s for error reporting. */
    for (unsigned i = 0; i < s.size(); i++) {
        if (s[i] == '\n') {
            newlines.push_back(i);
        }
    }
    newlines.push_back(s.size());
}

static void log(const char *title,
                const char *title_color,
                const char *msg,
                Idx idx)
{
    int line = upper_bound(newlines.begin(), newlines.end(), idx.start) -
               newlines.begin();
    /* <Line number>:<Column number>: <@title_color><@title>: ENDC <@msg>*/
    std::cerr << line << ":" << idx.start - newlines[line - 1] + 1 << ": "
              << title_color << title << ENDC << " " << msg << "\n";

    /* Suppose line number won't greater than 9999. */
    std::cerr << std::setw(5) << line << " | ";

    /* Print the line with highlight. */
    std::cerr << s.substr(line == 0 ? 0 : newlines[line - 1] + 1,
                          idx.start - (newlines[line - 1] + 1));
    std::cerr << title_color << s.substr(idx.start, idx.len) << ENDC;
    std::cerr << s.substr(idx.start + idx.len,
                          newlines[line] - (idx.start + idx.len))
              << "\n";

    /* Print a pointer to the error position. */
    std::cerr << "      | ";
    for (unsigned i = line == 0 ? 0 : newlines[line - 1] + 1; i < idx.start;
         i++) {
        std::cerr << " ";
    }
    std::cerr << title_color;
    for (unsigned i = 0; i < idx.len; i++) {
        std::cerr << "^";
    }
    std::cerr << ENDC << "\n";
}

void set_log_level(LogLevel level)
{
    log_level = level;
}

void internal_error(Idx idx, const char *msg)
{
    log("INTERNAL ERROR", FAIL, msg, idx);
    exit(1);
}

void critical(Idx idx, const char *msg)
{
    log("error", FAIL, msg, idx);
    exit(1);
}

void error(Idx idx, const char *msg)
{
    _error_count++;
    log("error", FAIL, msg, idx);
}

void warning(Idx idx, const char *msg)
{
    log("warning", WARNING, msg, idx);
}

void info(Idx idx, const char *msg)
{
    if (log_level <= LOG_LEVEL_INFO) {
        log("info", OKBLUE, msg, idx);
    }
}

void debug(Idx idx, const char *msg)
{
    if (log_level <= LOG_LEVEL_DEBUG) {
        log("debug", PURPLE, msg, idx);
    }
}

int error_count()
{
    return _error_count;
}
