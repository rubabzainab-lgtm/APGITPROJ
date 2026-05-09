#pragma once
#include <cstdio>
#include <cstdarg>
#include <ctime>

enum class LogLevel { INFO, WARN, ERROR_LVL };

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void open(const char* filename) {
        if (logFile_) fclose(logFile_);
        logFile_ = fopen(filename, "w");
    }

    void log(LogLevel level, const char* fmt, ...) {
        const char* tag = (level == LogLevel::INFO) ? "INFO"
                        : (level == LogLevel::WARN) ? "WARN" : "ERROR";

        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", t);

        char msgbuf[2048];
        va_list args;
        va_start(args, fmt);
        vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
        va_end(args);

        printf("[%s][%s] %s\n", timebuf, tag, msgbuf);
        if (logFile_) {
            fprintf(logFile_, "[%s][%s] %s\n", timebuf, tag, msgbuf);
            fflush(logFile_);
        }
    }

    ~Logger() {
        if (logFile_) fclose(logFile_);
    }

private:
    Logger() : logFile_(nullptr) {}
    FILE* logFile_;
};

#define LOG_INFO(fmt, ...)  Logger::instance().log(LogLevel::INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::instance().log(LogLevel::WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::instance().log(LogLevel::ERROR_LVL, fmt, ##__VA_ARGS__)
