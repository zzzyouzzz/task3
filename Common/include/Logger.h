#pragma once
#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <mutex>
#include <iomanip>
#include <filesystem>

// ================== 日志系统 ==================
enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL
};

static LogLevel parseLogLevel(const std::string& levelName) {
    std::string name = levelName;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::toupper(c); });
    if (name == "DEBUG") return LOG_DEBUG;
    if (name == "INFO") return LOG_INFO;
    if (name == "WARNING" || name == "WARN") return LOG_WARNING;
    if (name == "ERROR") return LOG_ERROR;
    if (name == "FATAL") return LOG_FATAL;
    return LOG_INFO;
}

class Logger {
private:
    std::ofstream m_logFile;
    LogLevel m_minLevel;
    bool m_consoleOutput;
    std::mutex m_mutex;
    std::string m_filename;
    static constexpr size_t MAX_LOG_SIZE = 5 * 1024 * 1024; // 5MB
    
    void rotateLog();
    
    std::string getCurrentTime();
    
    std::string levelToString(LogLevel level);
    
public:
    Logger() : m_minLevel(LOG_INFO), m_consoleOutput(true) {}
    
    ~Logger() {
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }
    
    bool initialize(const std::string& filename = "server.log", LogLevel minLevel = LOG_INFO, bool consoleOutput = true);
    
    void log(LogLevel level, const std::string& message);
    
    void debug(const std::string& message) { log(LOG_DEBUG, message); }
    void info(const std::string& message) { log(LOG_INFO, message); }
    void warning(const std::string& message) { log(LOG_WARNING, message); }
    void error(const std::string& message) { log(LOG_ERROR, message); }
    void fatal(const std::string& message) { log(LOG_FATAL, message); }
};
