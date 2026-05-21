#include "server.h"

// Windows 下 inet_ntop 替代实现（通过 getnameinfo）
const char* windows_inet_ntop(int af, const void* src, char* dst, socklen_t size) {
    if (af == AF_INET) {
        struct sockaddr_in in;
        memset(&in, 0, sizeof(in));
        in.sin_family = AF_INET;
        memcpy(&in.sin_addr, src, sizeof(struct in_addr));
        
        if (getnameinfo((struct sockaddr*)&in, sizeof(in), dst, size, NULL, 0, NI_NUMERICHOST) != 0) {
            return NULL;
        }
        return dst;
    }
    return NULL;
}

// 启动服务器主循环：初始化 socket → bind → listen → select 事件循环
void Server::start(const std::string& listenIp, int port) {
#ifdef _WIN32
    // Windows 下初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        g_logger.error("WSAStartup failed");
        return;
    }
#endif
    // 创建 TCP socket
    m_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenSocket == INVALID_SOCKET) {
        g_logger.error("Socket creation failed");
        return;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (listenIp.empty() || listenIp == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        addr.sin_addr.s_addr = inet_addr(listenIp.c_str());
        if (addr.sin_addr.s_addr == INADDR_NONE) {
            g_logger.warning("Invalid listen IP, fallback to INADDR_ANY: " + listenIp);
            addr.sin_addr.s_addr = INADDR_ANY;
        }
    }

    // 绑定端口
    if (bind(m_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        g_logger.error("Bind failed on port " + std::to_string(port));
        CLOSE_SOCKET(m_listenSocket);
        return;
    }
    // 开始监听
    if (listen(m_listenSocket, 5) == SOCKET_ERROR) {
        g_logger.error("Listen failed");
        CLOSE_SOCKET(m_listenSocket);
        return;
    }
    m_running = true;
    g_logger.info("Server started on port " + std::to_string(port));
    g_logger.info("Server entering main loop, waiting for client connections...");

    fd_set readfds;
    
    // ============ 主事件循环 ============
    while (m_running) {
        // 初始化 select 监听集合
        FD_ZERO(&readfds);
        FD_SET(m_listenSocket, &readfds);
        socket_t maxFD = m_listenSocket;
        for (auto it = m_userMap.begin(); it != m_userMap.end(); it++) {
            socket_t sock = it->first;
            if (sock > maxFD) maxFD = sock;
            FD_SET(sock, &readfds);
        }
        maxFD++;
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000 * 1000; // 1s
        // 等待 socket 事件，1 秒超时
        int ret = select(maxFD, &readfds, nullptr, nullptr, &tv);
        if (ret == SOCKET_ERROR) {
            g_logger.error("Select failed");
            continue;
        }
        if (ret == 0) {
            //g_logger.info("No activity on any socket, continue waiting...");
            // 检查所有客户端是否超时
            for (auto it = m_userMap.begin(); it != m_userMap.end(); ) {
                socket_t sock = it->first;
                ClientHandler& client = it->second;
                if (time(nullptr) - client.getLastActiveTime() > MAX_IDLE_TIME) {
                    g_logger.info("Client " + std::to_string(sock) + " idle for " + std::to_string(MAX_IDLE_TIME) + " seconds, closing connection.");
                    // 客户端超时，断开连接
                    CLOSE_SOCKET(sock);
                    client.handleLogout({});
                    it = m_userMap.erase(it);
                    continue;
                }
                ++it;
            }
            continue;
        }
        // 处理新连接
        if (FD_ISSET(m_listenSocket, &readfds)) {
            sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            socket_t clientSocket = accept(m_listenSocket, (sockaddr*)&clientAddr, &clientLen);
            if (clientSocket != INVALID_SOCKET) {
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
                g_logger.info("Client connected from: " + std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port)));

                // 设置客户端 socket 为非阻塞模式
                #ifdef _WIN32
                u_long ulTrue = 1;
                ioctlsocket(clientSocket, FIONBIO, &ulTrue);
                #endif
                #ifdef __linux__
                fcntl(clientSocket, F_SETFL, O_NONBLOCK);
                #endif

                // 创建客户端连接进程
                m_userMap[clientSocket] = ClientHandler();
            }  
        } 

        // 处理已有客户端的请求
        for (auto it = m_userMap.begin(); it != m_userMap.end() && m_running; ) {
            socket_t sock = it->first;
            if (FD_ISSET(sock, &readfds)) {
                ClientHandler& client = it->second;
                if (!handleClientRequest(client, sock)) {
                    // 客户端断开或出错，清理连接
                    CLOSE_SOCKET(sock);
                    it = m_userMap.erase(it);
                    continue;
                }
            }
            ++it;
        }    
    }

}

void Server::stop() {
    if (!m_running) return;
    m_running = false;
    g_logger.info("Server stopping...");
    if (m_listenSocket != INVALID_SOCKET) CLOSE_SOCKET(m_listenSocket);
    for (auto client : m_userMap) {
        CLOSE_SOCKET(client.first);
    }
    m_userMap.clear();
#ifdef _WIN32
    WSACleanup();
#endif
    g_logger.info("Server stopped successfully");
}

bool Server::handleClientRequest(ClientHandler& client, socket_t sock){
    while(true) {
        char buffer[MAX_BUF];
        memset(buffer, 0, sizeof(buffer));
        int ret = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (ret >= 0){
            if(ret == 0) return false;
        } else {
            #ifdef _WIN32
            ret = WSAGetLastError();
            if (ret == WSAEWOULDBLOCK) return true;
            else return false;
            #endif
            #ifdef __linux__
            int err = errno;
            if (err == EWOULDBLOCK || err == EAGAIN) return true;
            else return false;
            #endif
        }
        client.pushBuffer(std::string(buffer, ret));
        
        std::string response;
        client.setRequestId(m_currentRequestId);
        client.updateLastActiveTime();
        if (!client.handleRequst(response)) continue;

        m_currentRequestId++;

        if (send(sock, response.c_str(), static_cast<int>(response.size()), 0) == SOCKET_ERROR) {
            g_logger.error("req=" + std::to_string(m_currentRequestId - 1) + " Error sending response");
            return false;
        }
    }
    
}

