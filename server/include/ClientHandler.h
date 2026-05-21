#pragma once
#include "Logger.h"
#include "LogisticsSystem.h"
extern LogisticsSystem *m_system;

// ================== 客户端连接进程==================
class ClientHandler {
    private:
        int requestId;
        time_t lastActiveTime;
        std::string m_currentUser;             // 当前登录用户
        UserType m_userType;                 // 当前登录用户类型
        std::string buf;        // 累积接收缓冲区，用于处理 TCP 半包
    public:
        ClientHandler() : requestId(0) {
            lastActiveTime = time(nullptr);
            m_currentUser.clear();
            buf.clear();
        }

        // 更新最后活跃时间
        void updateLastActiveTime() { lastActiveTime = time(nullptr); }
        // 获取最后活跃时间
        time_t getLastActiveTime() { return lastActiveTime; }

        // 接收新数据并累加到缓冲区
        void pushBuffer(const std::string& data) { buf += data; }
        void setRequestId(int Id) { requestId = Id; }
        bool handleRequest(std::string& response);
        // 路由命令到对应 handler, 网关权限检查
        std::string processCommand(const std::string& cmd, const std::vector<std::string>& args);
        // 构造响应报文 + 日志
        std::string buildResponse(ErrorCode code = ErrorCode::UNKNOWN, const std::vector<std::string>& data = {});
        // 处理各命令 handler
        std::string handleLogout(const std::vector<std::string>& args);
        std::string handleRegister(const std::vector<std::string>& args);
        std::string handleLogin(const std::vector<std::string>& args);
        std::string handleSendParcel(const std::vector<std::string>& args);
        std::string handleAssignParcel(const std::vector<std::string>& args);
        std::string handleCollectParcel(const std::vector<std::string>& args);
        std::string handleSignParcel(const std::vector<std::string>& args);
        std::string handleQueryParcel(const std::vector<std::string>& args);
        std::string handleRechargeBalance(const std::vector<std::string>& args);
        std::string handleQueryBalance(const std::vector<std::string>& args);
        std::string handleChangePassword(const std::vector<std::string>& args);
        std::string handleDeleteAccount(const std::vector<std::string>& args);
        std::string handleQueryUser(const std::vector<std::string>& args);
        std::string handleDeleteParcel(const std::vector<std::string>& args);
        std::string handleGetStatistics(const std::vector<std::string>& args);
};
