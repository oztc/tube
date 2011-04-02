// -*- mode: c++ -*-

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <cstring>
#include <cstdio>

#define MAX_LOG_LENGTH 1024

#ifdef LOG_ENABLED
#define LOG(level, ...)                                                 \
    do {                                                                \
        char str[MAX_LOG_LENGTH];                                       \
        snprintf(str, MAX_LOG_LENGTH, __VA_ARGS__);                     \
        tube::utils::logger.log(level, str, __FILE__, __LINE__);    \
    } while (0)                                                         \

#else
#define LOG(...)
#endif

#define ERROR   0
#define WARNING 1
#define INFO    2
#define DEBUG   3

namespace tube {
namespace utils {

struct LogWriter
{
    virtual void write_log(const char* str) = 0;
};

class Logger
{
    int current_level_;
    LogWriter* writer_;
public:
    Logger();
    ~Logger();

    void log(int level, const char* str, const char* file, int line);
    void set_level(int level) { current_level_ = level; }
};

struct StdLogWriter : public LogWriter
{
    virtual void write_log(const char* str);
};

struct FileLogWriter : public LogWriter
{
    FileLogWriter(const char* filename) throw();
    virtual ~FileLogWriter();

    virtual void write_log(const char* str);
private:
    FILE* fp_;
};

extern Logger logger;

}
}

#endif /* _LOGGER_H_ */
