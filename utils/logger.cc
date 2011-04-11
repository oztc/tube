#include "pch.h"

#include <stdexcept>
#include <cstdlib>
#include <sys/time.h>
#include <sys/syscall.h>

#include "utils/logger.h"
#include "utils/misc.h"

namespace tube {
namespace utils {

void
StdLogWriter::write_log(const char* str)
{
    fprintf(stderr, "%s\n", str);
}

FileLogWriter::FileLogWriter(const char* filename) 
{
    fp_ = fopen(filename, "a+");
    if (fp_ == NULL)
        throw std::invalid_argument(std::string("cannot open log file!"));
}

FileLogWriter::~FileLogWriter()
{
    fclose(fp_);
}

void
FileLogWriter::write_log(const char* str)
{
    fprintf(fp_, "%s\n", str);
    fflush(fp_);
}

Logger::Logger()
{
    current_level_ = WARNING;
    const char* log_file = getenv("LOG_FILE");
    if (log_file != NULL) {
        writer_ = new FileLogWriter(log_file);
    } else {
        writer_ = new StdLogWriter();
    }
}

Logger::~Logger()
{
    delete writer_;
}

void
Logger::log(int level, const char* str, const char* file, int line)
{
    if (level <= current_level_) {
        char logstr[MAX_LOG_LENGTH];
        struct timeval tv;
        pid_t tid = get_thread_id();
        gettimeofday(&tv, NULL);
        snprintf(logstr, MAX_LOG_LENGTH, "%lu.%.6lu thread %u %s:%d : %s",
                 tv.tv_sec, tv.tv_usec, tid, file, line, str);
        writer_->write_log(logstr);
    }
}

Logger logger;

}
}
