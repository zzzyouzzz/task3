#pragma once
#include "Logger.h"

bool loadStartupConfig(const std::string& filename, std::string& ip, int& port) {
    g_logger.initialize("client.log", LOG_INFO);
    std::ifstream file(filename);
    if (!file.is_open()) {
        g_logger.error("Unable to open client config file: " + filename);
        return false;
    }

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
            LogLevel logLevel = parseLogLevel(value);
            if (!g_logger.initialize("client.log", logLevel)) {
                std::cerr << "无法初始化日志文件: client.log" << std::endl;
            }
        }
    }
    g_logger.info("Client configuration loaded from " + filename);
    return !ip.empty() && port > 0;
}