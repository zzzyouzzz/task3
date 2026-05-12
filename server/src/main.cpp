#include "server.h"

// 从配置文件加载启动参数，忽略空行和 # 注释
static bool loadStartupConfig(const std::string& filename, std::string& ip, int& port, std::string& logLevelName,
                              std::string& userFile, std::string& parcelFile, std::string& configFile, bool& autoAssignCourier, bool& consoleOutput) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trimString(line);
        if (line.empty() || line[0] == '#') continue;
        auto separator = line.find('=');
        if (separator == std::string::npos) continue;
        std::string key = trimString(line.substr(0, separator));
        std::string value = trimString(line.substr(separator + 1));
        if (key == "ip") {
            ip = value;
        } else if (key == "port") {
            try {
                port = std::stoi(value);
            } catch (...) {
                port = 0;
            }
        } else if (key == "log_level") {
            logLevelName = value;
        } else if (key == "user_file") {
            userFile = value;
        } else if (key == "parcel_file") {
            parcelFile = value;
        } else if (key == "config_file") {
            configFile = value;
        } else if (key == "auto_assign_courier") {
            autoAssignCourier = (value == "true" || value == "1");
        } else if (key == "log_output") {
            std::string output = value;
            std::transform(output.begin(), output.end(), output.begin(), [](unsigned char c) { return std::tolower(c); });
            if (output == "console") {
                consoleOutput = true;
            } else if (output == "file") {
                consoleOutput = false;
            } else if (output == "both") {
                consoleOutput = true;
            }
        }
    }
    return !ip.empty() && port > 0;
}
Server *server;

// Ctrl+C / Ctrl+Break 信号处理：优雅关闭服务器
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT) {
        g_logger.info("Ctrl+C or Ctrl+BREAK pressed, shutting down server...");
        server->stop();
        return TRUE;
    }
    return FALSE;
}

Logger g_logger;

int main() {
    std::string listenIp = "0.0.0.0";
    int listenPort = 8888;
    std::string logLevelName = "INFO";
    std::string userFile = "users.dat";
    std::string parcelFile = "parcels.dat";
    std::string configFile = "config.dat";
    bool autoAssignCourier = false;
    bool consoleOutput = true;

    if (!g_logger.initialize("server.log", LOG_INFO, consoleOutput)) {
        std::cerr << "Failed to initialize logger!" << std::endl;
        return 1;
    }
    g_logger.info("Logger initialized with default INFO level");

    if (loadStartupConfig("server_config.txt", listenIp, listenPort, logLevelName,
                          userFile, parcelFile, configFile, autoAssignCourier, consoleOutput)) {
        g_logger.info("Loaded server startup config from server_config.txt");
    } else {
        g_logger.warning("Failed to load server_config.txt, using default settings");
    }

    LogLevel logLevel = parseLogLevel(logLevelName);
    if (!g_logger.initialize("server.log", logLevel, consoleOutput)) {
        g_logger.error("Failed to reinitialize logger with log level " + logLevelName);
        return 1;
    }
    g_logger.info("Logger reinitialized with config log level " + logLevelName);

    g_logger.info("Logistics System Server starting...");
    g_logger.info("Server configured to listen on " + listenIp + ":" + std::to_string(listenPort));
    g_logger.info("Log level set to " + logLevelName);
    g_logger.info("Data files: users=" + userFile + ", parcels=" + parcelFile + ", config=" + configFile);
    g_logger.info("Auto-assign courier: " + std::string(autoAssignCourier ? "enabled" : "disabled"));

    server = new Server(userFile, parcelFile, configFile, autoAssignCourier);
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    server->start(listenIp, listenPort);

    delete server;
    server = nullptr;

    g_logger.info("Server shutdown complete");

    SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);

    return 0;
}