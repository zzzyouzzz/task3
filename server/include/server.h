#pragma once
#include <atomic>
#include "Logger.h"
#include "LogisticsSystem.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
    typedef int socklen_t;   // 修正：MinGW 可能缺少 socklen_t
    
    // Windows下inet_ntop的替代实现
    const char* windows_inet_ntop(int af, const void* src, char* dst, socklen_t size);

    #define inet_ntop windows_inet_ntop
    #ifndef INET_ADDRSTRLEN
        #define INET_ADDRSTRLEN 46
    #endif
#else
    #include <unistd.h>
    #include <cerrno>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    typedef int socket_t;
    #define CLOSE_SOCKET close
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

const int MAX_BUF = 4096;

// ================== 客户端连接上下文（每个连接持有一个缓冲区）==================
typedef struct client_info {
    socket_t socket;        // 客户端 socket
    User* user;             // 当前登录用户指针（nullptr 表示未登录）
    std::string buf;        // 累积接收缓冲区，用于处理 TCP 半包

    client_info(socket_t socket, User* user)
        : socket(socket), user(user) {}
    client_info() = default;
} Client_info;


// ================== 服务器主类 ==================
class Server {
private:
    socket_t m_listenSocket;                         // 监听 socket
    LogisticsSystem m_system;                         // 业务逻辑系统实例
    bool m_running;                                   // 主循环运行标志
    bool m_autoAssignCourier;                         // 是否自动分配快递员
    std::map<socket_t, Client_info> m_userMap;        // socket -> 客户端上下文
    std::atomic<long long> m_requestIdCounter;        // 请求 ID 原子计数器
    long long m_currentRequestId;                     // 当前处理中的请求 ID（日志用）

public:
    Server(const std::string& userFile, const std::string& parcelFile, const std::string& configFile, bool autoAssignCourier)
        : m_listenSocket(INVALID_SOCKET), m_system(userFile, parcelFile, configFile, autoAssignCourier), m_running(false), m_autoAssignCourier(autoAssignCourier), m_requestIdCounter(0), m_currentRequestId(0) {}
    
    ~Server() { stop(); }

    // 启动服务器，开始监听指定 IP:端口
    void start(const std::string& listenIp, int port);
    // 停止服务器，关闭所有 socket 并清理
    void stop();

private:
    // 处理单个客户端的请求（含半包重组）
    bool handleClientRequest(Client_info& clientInfo);
    // 路由命令到对应 handler
    std::string processCommand(User*& m_currentUser, const std::string& cmd, const std::vector<std::string>& args);

    std::string handleRegister(const std::vector<std::string>& args);
    // 构造响应报文 + 日志
    std::string buildResponse(const std::string& status, const std::vector<std::string>& data, ErrorCode code = ErrorCode::UNKNOWN);
    // 处理各命令 handler
    std::string handleLogout(User*& m_currentUser, const std::vector<std::string>& args);
    std::string handleLogin(User* &m_currentUser, const std::vector<std::string>& args);
    std::string handleSendParcel(const User* m_currentUser, const std::vector<std::string>& args);
    std::string handleAssignParcel(const User* m_currentUser, const std::vector<std::string>& args);
    std::string handleCollectParcel(const User* m_currentUser, const std::vector<std::string>& args);
    std::string handleSignParcel(const User* m_currentUser, const std::vector<std::string>& args);

    std::string handleQueryParcel(const User* m_currentUser, const std::vector<std::string>& args);

    std::string handleRechargeBalance(const User* m_currentUser, const std::vector<std::string>& args);
    std::string handleQueryBalance(const User* m_currentUser, const std::vector<std::string>& args);
    std::string handleChangePassword(const User* m_currentUser, const std::vector<std::string>& args);
    std::string handleDeleteAccount(const User* m_currentUser, const std::vector<std::string>& args);
    std::string handleQueryUser(const User* m_currentUser, const std::vector<std::string>& args);
    std::string handleDeleteParcel(const User* m_currentUser, const std::vector<std::string>& args);
    std::string handleGetStatistics(const User* m_currentUser, const std::vector<std::string>& args);
};