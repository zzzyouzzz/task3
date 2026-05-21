#pragma once
#include <atomic>
#include "ClientHandler.h"

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

const int MAX_BUF = 4096;   // 接收缓冲区大小
const int MAX_IDLE_TIME = 60 *3; // 最大挂机时间（秒）


// ================== 服务器主类 ==================
class Server {
private:
    socket_t m_listenSocket;                         // 监听 socket
    bool m_running;                                   // 主循环运行标志
    std::map<socket_t, ClientHandler> m_userMap;        // socket -> 客户端连接进程
    std::atomic<int> m_currentRequestId;                // 当前请求 ID

public:
    Server()
        : m_listenSocket(INVALID_SOCKET), m_running(false), m_currentRequestId(0) {}
    
    ~Server() { stop(); }

    // 启动服务器，开始监听指定 IP:端口
    void start(const std::string& listenIp, int port);
    // 停止服务器，关闭所有 socket 并清理
    void stop();

private:
    // 处理单个客户端的请求（含半包重组）
    bool handleClientRequest(ClientHandler& clientHandler, socket_t sock);
};