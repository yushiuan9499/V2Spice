#ifndef LOG_H
#define LOG_H

#include "defs.h"

enum LogLevel {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_CRITIAL,
    LOG_LEVEL_INTERNAL_ERROR
};

void init_log();

void set_log_level(LogLevel level);

void internal_error(Idx idx, const char *msg);
void critical(Idx idx, const char *msg);
void error(Idx idx, const char *msg);
void warning(Idx idx, const char *msg);
void info(Idx idx, const char *msg);
void debug(Idx idx, const char *msg);

int error_count();

#endif  // LOG_H
