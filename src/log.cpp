#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
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

static LogLevel log_level = LOG_LEVEL_INFO;
static int _error_count = 0;
static std::map<std::string, int> fd;
std::vector<File> files;

int open(const std::string &filename, const Loc &loc)
{
    static int next_fd = 0;
    std::ifstream ifs(filename);
    if (!ifs) {
        if (loc.len != 0) {
            critical(loc, ("cannot open file: " + filename).c_str());
        } else {
            std::cerr << "cannot open file: " << filename << "\n";
            exit(1);
        }
    }
    fd[filename] = next_fd++;
    files.push_back(
        {filename, std::string((std::istreambuf_iterator<char>(ifs)),
                               std::istreambuf_iterator<char>()) +
                       "\n"});
    files.back().newlines.push_back(0);
    /* Compute the positions of newlines in s for error reporting. */
    for (unsigned i = 0; i < files.back().content.size(); i++) {
        if (files.back().content[i] == '\n') {
            files.back().newlines.push_back(i);
        }
    }
    files.back().newlines.push_back(files.back().content.size());
    return fd[filename];
}

static void log(const char *title,
                const char *title_color,
                const char *msg,
                const Loc &loc)
{
    const File &file = files[loc.fd];
    int line =
        upper_bound(file.newlines.begin(), file.newlines.end(), loc.start) -
        file.newlines.begin();
    unsigned line_start = line == 1 ? 0 : file.newlines[line - 1] + 1;
    /* <File name>:<Line number>:<Column number>: <@title_color><@title>: ENDC
     * <@msg>*/
    std::cerr << file.name << ":" << line << ":" << loc.start - line_start + 1
              << ": " << title_color << title << ":" ENDC " " << msg << "\n";

    /* Suppose line number won't greater than 9999. */
    std::cerr << std::setw(5) << line << " | ";

    /* Print the line with highlight. */
    std::cerr << file.content.substr(line_start, loc.start - line_start);
    std::cerr << title_color << file.content.substr(loc.start, loc.len) << ENDC;
    std::cerr << file.content.substr(
                     loc.start + loc.len,
                     file.newlines[line] - (loc.start + loc.len))
              << "\n";

    /* Print a pointer to the error position. */
    std::cerr << "      | ";
    for (unsigned i = line_start; i < loc.start; i++) {
        std::cerr << " ";
    }
    std::cerr << title_color;
    for (unsigned i = 0; i < loc.len; i++) {
        std::cerr << "^";
    }
    std::cerr << ENDC << "\n";
}

void set_log_level(LogLevel level)
{
    log_level = level;
}

void internal_error(const Loc &loc, const char *msg)
{
    log("INTERNAL ERROR", FAIL, msg, loc);
    exit(1);
}

void critical(const Loc &loc, const char *msg)
{
    log("error", FAIL, msg, loc);
    exit(1);
}

void error(const Loc &loc, const char *msg)
{
    _error_count++;
    log("error", FAIL, msg, loc);
}

void warning(const Loc &loc, const char *msg)
{
    log("warning", WARNING, msg, loc);
}

void info(const Loc &loc, const char *msg)
{
    if (log_level <= LOG_LEVEL_INFO) {
        log("info", OKBLUE, msg, loc);
    }
}

void debug(const Loc &loc, const char *msg)
{
    if (log_level <= LOG_LEVEL_DEBUG) {
        log("debug", PURPLE, msg, loc);
    }
}

int error_count()
{
    return _error_count;
}
