#include "Logger.h"

// 日志轮转：超过 5MB 则重命名 .1 → .2 → .3，重新创建日志文件
void Logger::rotateLog() {
    if (!std::filesystem::exists(m_filename)) return;
    std::error_code ec;
    size_t size = std::filesystem::file_size(m_filename, ec);
    if (ec || size < MAX_LOG_SIZE) return;

    m_logFile.close();
    // Rotate: .3 -> remove, .2 -> .3, .1 -> .2, current -> .1
    std::filesystem::path base(m_filename);
    std::filesystem::remove(base.string() + ".3", ec);
    std::filesystem::rename(base.string() + ".2", base.string() + ".3", ec);
    std::filesystem::rename(base.string() + ".1", base.string() + ".2", ec);
    std::filesystem::rename(m_filename, base.string() + ".1", ec);

    m_logFile.open(m_filename, std::ios::app);
    if (!m_logFile.is_open()) {
        std::cerr << "Logger: failed to reopen log file after rotation: " << m_filename << std::endl;
    }
}

// 获取当前时间字符串，格式 YYYY-MM-DD HH:MM:SS
std::string Logger::getCurrentTime() {
    std::time_t now = std::time(nullptr);
    std::tm localTimeStorage;
#ifdef _WIN32
    localtime_s(&localTimeStorage, &now);
#else
    localtime_r(&now, &localTimeStorage);
#endif
    std::ostringstream oss;
    oss << std::put_time(&localTimeStorage, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        case LOG_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

// 初始化日志：设置文件名、日志级别、控制台输出，先检查轮转再打开文件
bool Logger::initialize(const std::string& filename, LogLevel minLevel, bool consoleOutput) {
    m_filename = filename;
    m_minLevel = minLevel;
    m_consoleOutput = consoleOutput;

    rotateLog();

    m_logFile.open(m_filename, std::ios::app);
    if (!m_logFile.is_open()) {
        return false;
    }
    
    m_minLevel = minLevel;
    m_consoleOutput = consoleOutput;
    
    log(LOG_INFO, "Logger initialized successfully");
    return true;
}

// 写入日志：低于最低级别的消息被过滤，线程安全
void Logger::log(LogLevel level, const std::string& message) {
    if (level < m_minLevel) return;
    
    std::ostringstream oss;
    oss << "[" << getCurrentTime() << "] [" << levelToString(level) << "] " << message;
    std::string logEntry = oss.str();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) {
        m_logFile << logEntry << std::endl;
        m_logFile.flush();
    }
    if (m_consoleOutput) {
        std::cout << logEntry << std::endl;
    }

    // 每次写入后检查是否需要轮转
    rotateLog();
}