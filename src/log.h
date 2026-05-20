#ifndef LOG_H
#define LOG_H

#include "defs.h"

#include <string>

enum LogLevel {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_CRITIAL,
    LOG_LEVEL_INTERNAL_ERROR
};

int open(const std::string &filename, const Loc &loc = Loc{0, 0});


struct File {
    std::string name;
    std::string content;
    std::vector<unsigned> newlines;
};
extern std::vector<File> files;
static inline const std::string &get_file_content(int fd)
{
    return files[fd].content;
}
static inline std::string get_file_content(const Loc &loc)
{
    return get_file_content(loc.fd).substr(loc.start, loc.len);
}

void set_log_level(LogLevel level);

void internal_error(const Loc &loc, const char *msg);
void critical(const Loc &loc, const char *msg);
void error(const Loc &loc, const char *msg);
void warning(const Loc &loc, const char *msg);
void info(const Loc &loc, const char *msg);
void debug(const Loc &loc, const char *msg);

int error_count();

#endif  // LOG_H
