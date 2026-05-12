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
            // 超时无事件，继续等待
            g_logger.info("No client activity, continue waiting...");
            Sleep(1000);
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

                m_userMap[clientSocket] = Client_info(clientSocket, nullptr);
            }  
        } 

        // 处理已有客户端的请求
        for (auto it = m_userMap.begin(); it != m_userMap.end() && m_running; ) {
            socket_t sock = it->first;
            if (FD_ISSET(sock, &readfds)) {
                Client_info& clientInfo = it->second;
                if (!handleClientRequest(clientInfo)) {
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

bool Server::handleClientRequest(Client_info& clientInfo){
    while(true) {
        char buffer[MAX_BUF];
        memset(buffer, 0, sizeof(buffer));
        int ret = recv(clientInfo.socket, buffer, sizeof(buffer)-1, 0);
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
        clientInfo.buf += std::string(buffer, ret);
        size_t pos = clientInfo.buf.find('\n');
        if (pos == std::string::npos) continue;
        std::string request = clientInfo.buf.substr(0, pos+1);
        clientInfo.buf.erase(0, pos+1);
        g_logger.debug("Raw request received: " + request);
        std::string cmd;
        std::vector<std::string> args;
        Protocol::parseRequest(request, cmd, args);
        long long reqId = ++m_requestIdCounter;
        m_currentRequestId = reqId;
        std::string argSummary = "args_count=" + std::to_string(args.size());
        if (cmd != Command::LOGIN && cmd != Command::REGISTER && cmd != Command::CHANGE_PASSWORD) {
            argSummary = "args=[";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) argSummary += ", ";
                argSummary += args[i].substr(0, 64);
                if (args[i].size() > 64) argSummary += "...";
            }
            argSummary += "]";
        }
        g_logger.info("req=" + std::to_string(reqId) + " Processing command: " + cmd + " from socket, " + argSummary);
        std::string response;
        try {
            response = processCommand(clientInfo.user, cmd, args);
        } catch (const std::exception& ex) {
            g_logger.error("req=" + std::to_string(reqId) + " Unhandled exception processing command: " + std::string(ex.what()));
            response = buildResponse("ERROR", {"Internal server error"});
        } catch (...) {
            g_logger.error("req=" + std::to_string(reqId) + " Unhandled unknown exception processing command");
            response = buildResponse("ERROR", {"Internal server error"});
        }

        if (send(clientInfo.socket, response.c_str(), static_cast<int>(response.size()), 0) == SOCKET_ERROR) {
            g_logger.error("req=" + std::to_string(reqId) + " Error sending response");
            return false;
        }
    }
    
}

std::string Server::processCommand(User*& m_currentUser, const std::string& cmd, const std::vector<std::string>& args) {
    if (cmd == Command::LOGIN) {
        return handleLogin(m_currentUser, args);
    } else if (cmd == Command::REGISTER) {       
        return handleRegister(args);
    } else {
        if (m_currentUser == nullptr) {
            g_logger.warning("User not logged in");
            return buildResponse("ERROR", {"Not logged in"});
        } else {
            if (cmd == Command::SEND_PARCEL) {
                if (m_currentUser->getUserType() != UserType::CUSTOMER) {
                    g_logger.warning("SendParcel command: Permission denied for user: " + 
                        (m_currentUser ? m_currentUser->getUsername() : "anonymous"));
                    return buildResponse("ERROR", {"Only customers can send parcels"});
                }
                return handleSendParcel(m_currentUser, args);
            } else if (cmd == Command::ASSIGN_PARCEL) {
                if (m_currentUser->getUserType() != UserType::ADMINISTRATOR) {
                    g_logger.warning("AssignParcel command: Permission denied for user: " + 
                        (m_currentUser ? m_currentUser->getUsername() : "anonymous"));
                    return buildResponse("ERROR", {"Only administrators can assign parcels"});
                }
                return handleAssignParcel(m_currentUser, args);
            } else if (cmd == Command::COLLECT_PARCEL) {
                if (m_currentUser->getUserType() != UserType::COURIER) {
                    g_logger.warning("CollectParcel command: Permission denied for user: " + 
                        (m_currentUser ? m_currentUser->getUsername() : "anonymous"));
                    return buildResponse("ERROR", {"Only couriers can collect parcels"});
                }
                return handleCollectParcel(m_currentUser, args);
            } else if (cmd == Command::SIGN_PARCEL) {
                if (m_currentUser->getUserType() != UserType::CUSTOMER) {
                    g_logger.warning("SignParcel command: Permission denied for user: " + 
                        (m_currentUser ? m_currentUser->getUsername() : "anonymous"));
                    return buildResponse("ERROR", {"Only customers can sign parcels"});
                }
                return handleSignParcel(m_currentUser, args);
            } else if (cmd == Command::QUERY_PARCEL) {    
                return handleQueryParcel(m_currentUser, args);
            } else if (cmd == Command::QUERY_USER) {
                return handleQueryUser(m_currentUser, args);
            } else if (cmd == Command::RECHARGE_BALANCE) {
                return handleRechargeBalance(m_currentUser, args);
            } else if (cmd == Command::QUERY_BALANCE) {
                return handleQueryBalance(m_currentUser, args);
            } else if (cmd == Command::CHANGE_PASSWORD) {
                return handleChangePassword(m_currentUser, args);
            } else if (cmd == Command::DELETE_ACCOUNT) {
                if (m_currentUser->getUserType() != UserType::ADMINISTRATOR) {
                    g_logger.warning("DeleteAccount command: Permission denied for user: " + 
                        (m_currentUser ? m_currentUser->getUsername() : "anonymous"));
                    return buildResponse("ERROR", {"Only administrators can delete accounts"});
                }
                return handleDeleteAccount(m_currentUser, args);
            } else if (cmd == Command::DELETE_PARCEL) {
                if (m_currentUser->getUserType() != UserType::ADMINISTRATOR) {
                    g_logger.warning("DeleteParcel command: Permission denied for user: " + 
                        (m_currentUser ? m_currentUser->getUsername() : "anonymous"));
                    return buildResponse("ERROR", {"Only administrators can delete parcels"});
                }
                return handleDeleteParcel(m_currentUser, args);
            } else if (cmd == Command::LOGOUT) {
                return handleLogout(m_currentUser, args);
            } else if (cmd == Command::GET_STATISTICS) {
                if (m_currentUser->getUserType() != UserType::ADMINISTRATOR) {
                    g_logger.warning("GetStatistics command: Permission denied for user: " + 
                        (m_currentUser ? m_currentUser->getUsername() : "anonymous"));
                    return buildResponse("ERROR", {"Only administrators can get statistics"});
                }
                return handleGetStatistics(m_currentUser, args);
            } else {
                return buildResponse("ERROR", {"Unknown command"});
            }
        }
    }
}


std::string Server::handleRegister(const std::vector<std::string>& args) {
    if (args.size() < 6) {
        g_logger.warning("Register command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }
    std::string username = args[0];
    std::string password = args[1];
    std::string name = args[2];
    std::string phone = args[3];
    std::string address = args[4];
    int typeInt = 0;
    if (!parseInt(args[5], typeInt) || typeInt < 0 || typeInt > static_cast<int>(UserType::COURIER)) {
        g_logger.warning("Register command: Invalid user type: " + args[5]);
        return buildResponse("ERROR", {"Invalid user type"});
    }
    UserType type = static_cast<UserType>(typeInt);
    
    g_logger.info("Register attempt for user: " + username + " type: " + std::to_string(typeInt));
    
    // 只允许注册客户或快递员
    if (type != UserType::CUSTOMER && type != UserType::COURIER) {
        g_logger.warning("Register command: Invalid user type: " + std::to_string(typeInt));
        return buildResponse("ERROR", {"Invalid user type"});
    }
    
    ErrorCode ec = m_system.registerUser(username, password, name, phone, address, type);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("User registered successfully: " + username + " as " + (type == UserType::CUSTOMER ? "Customer" : "Courier"));
        return buildResponse("OK", {"Registration successful"});
    } else {
        g_logger.warning("User registration failed - username already exists: " + username);
        return buildResponse("ERROR", {"Username already exists"}, ec);
    }
}

std::string Server::buildResponse(const std::string& status, const std::vector<std::string>& data, ErrorCode code) {
    std::string resp = Command::RESPONSE + std::string(1, DELIMITER) + status;
    if (status == "ERROR") {
        resp += std::string(1, DELIMITER) + std::to_string(static_cast<int>(code));
    }
    for (const auto& d : data) {
        resp += std::string(1, DELIMITER) + d;
    }
    resp += '\n';

    std::string prefix = "";
    if (m_currentRequestId != 0) {
        prefix = "req=" + std::to_string(m_currentRequestId) + " ";
    }
    g_logger.debug(prefix + "Sending response: " + resp);
    return resp;
}

std::string Server::handleLogout(User*& m_currentUser, const std::vector<std::string>& args) {
    if (args.size() != 0) {
        g_logger.warning("Logout command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }
    m_currentUser = nullptr;
    g_logger.info("User logged out successfully");
    return buildResponse("OK", {"Logout successful"});
}

std::string Server::handleLogin(User* &m_currentUser, const std::vector<std::string>& args) {
    if (args.size() < 3) {
        g_logger.warning("Login command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }
    std::string username = args[0];
    std::string password = args[1];
    int typeInt = 0;
    if (!parseInt(args[2], typeInt) || typeInt < 0 || typeInt > static_cast<int>(UserType::ADMINISTRATOR)) {
        g_logger.warning("Login command: Invalid user type: " + args[2]);
        return buildResponse("ERROR", {"Invalid user type"});
    }
    UserType type = static_cast<UserType>(typeInt);
    
    g_logger.info("Login attempt for user: " + username + " type: " + std::to_string(typeInt));
    
    auto [ec, user] = m_system.loginUser(username, password, type);
    if (ec == ErrorCode::SUCCESS) {
        m_currentUser = user;
        g_logger.info("User logged in successfully: " + m_currentUser->getUsername());
        return buildResponse("OK", {m_currentUser->getUsername(), std::to_string(typeInt)});
    } else {
        g_logger.warning("Login failed for user: " + username);
        return buildResponse("ERROR", {"Login failed"}, ec);
    }
}

std::string Server::handleSendParcel(const User* m_currentUser, const std::vector<std::string>& args) {
    if (args.size() < 4) {
        g_logger.warning("SendParcel command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }
    std::string receiver = args[0];
    int ptype = 0;
    if (!parseInt(args[1], ptype)) {
        g_logger.warning("SendParcel command: Invalid parcel type: " + args[1]);
        return buildResponse("ERROR", {"Invalid parcel type"});
    }
    double weight = 0.0;
    if (!parseDouble(args[2], weight)) {
        g_logger.warning("SendParcel command: Invalid weight: " + args[2]);
        return buildResponse("ERROR", {"Invalid weight"});
    }
    std::string desc = args[3];
    ParcelType type = static_cast<ParcelType>(ptype);
    
    g_logger.info("SendParcel attempt - Sender: " + m_currentUser->getUsername() + 
                    ", Receiver: " + receiver + ", Type: " + std::to_string(ptype) + 
                    ", Weight: " + std::to_string(weight));
    
    auto [ec, pid] = m_system.sendParcel(m_currentUser->getUsername(), receiver, type, weight, desc);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Parcel sent successfully - ID: " + pid + ", Sender: " + m_currentUser->getUsername());
        return buildResponse("OK", {pid});
    } else {
        g_logger.warning("SendParcel failed - Sender: " + m_currentUser->getUsername() + 
                        ", Receiver: " + receiver + " (receiver not found or insufficient balance)");
        return buildResponse("ERROR", {"Failed to send parcel"}, ec);
    }
}

std::string Server::handleAssignParcel(const User* m_currentUser, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        g_logger.warning("AssignParcel command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }
    std::string parcelId = args[0];
    std::string courier = args[1];
    
    g_logger.info("AssignParcel attempt - Admin: " + m_currentUser->getUsername() + 
                    ", Parcel: " + parcelId + ", Courier: " + courier);
    
    ErrorCode ec = m_system.assignCourier(parcelId, courier);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Parcel assigned successfully - Parcel: " + parcelId + ", Courier: " + courier);
        return buildResponse("OK", {"Assigned"});
    } else {
        g_logger.warning("Parcel assignment failed - Parcel: " + parcelId + ", Courier: " + courier);
        return buildResponse("ERROR", {"Assignment failed"}, ec);
    }
}

std::string Server::handleCollectParcel(const User* m_currentUser, const std::vector<std::string>& args) {
    if (args.empty()) {
        g_logger.warning("CollectParcel command: No parcel IDs provided");
        return buildResponse("ERROR", {"No parcel IDs"});
    }
    std::vector<std::string> ids = args;
    
    g_logger.info("CollectParcel attempt - Courier: " + m_currentUser->getUsername() + 
                    ", Parcel IDs count: " + std::to_string(ids.size()));
    
    auto [ec, collected] = m_system.collectParcels(m_currentUser->getUsername(), ids);
    if (ec == ErrorCode::SUCCESS) {
        std::string list;
        for (size_t i = 0; i < collected.size(); ++i) {
            if (i > 0) list += DELIMITER;
            list += collected[i];
        }
        g_logger.info("Parcels collected successfully - Courier: " + m_currentUser->getUsername() + 
                        ", Collected count: " + std::to_string(collected.size()));
        return buildResponse("OK", {list});
    }
    g_logger.warning("No parcels collected - Courier: " + m_currentUser->getUsername());
    return buildResponse("ERROR", {"No parcels collected"}, ec);
}

std::string Server::handleSignParcel(const User* m_currentUser, const std::vector<std::string>& args) {
    if (args.empty()) {
        g_logger.warning("SignParcel command: No parcel IDs provided");
        return buildResponse("ERROR", {"No parcel IDs"});
    }
    std::vector<std::string> ids = args;
    
    g_logger.info("SignParcel attempt - User: " + m_currentUser->getUsername() + 
                    ", Parcel IDs count: " + std::to_string(ids.size()));
    
    auto [ec, signedList] = m_system.signParcels(m_currentUser->getUsername(), ids);
    if (ec == ErrorCode::SUCCESS) {
        std::string list;
        for (size_t i = 0; i < signedList.size(); ++i) {
            if (i > 0) list += DELIMITER;
            list += signedList[i];
        }
        g_logger.info("Parcels signed successfully - User: " + m_currentUser->getUsername() + 
                        ", Signed count: " + std::to_string(signedList.size()));
        return buildResponse("OK", {list});
    }
    g_logger.warning("No parcels signed - User: " + m_currentUser->getUsername());
    return buildResponse("ERROR", {"No parcels signed"}, ec);
}

std::string Server::handleQueryParcel(const User* m_currentUser, const std::vector<std::string>& args) {
    g_logger.info("QueryParcel request - User: " + m_currentUser->getUsername() + 
                    ", UserType: " + std::to_string(static_cast<int>(m_currentUser->getUserType())));
    
    if (args.size() < 7) {
        g_logger.warning("QueryParcel command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }

    std::string parcelId = args[0];
    std::string sender = args[1];
    std::string receiver = args[2];
    std::string courier = args[3];
    int statusInt = 0;
    if (!parseInt(args[4], statusInt) || statusInt < 0 || statusInt > static_cast<int>(ParcelStatus::OTHER)) {
        g_logger.warning("QueryParcel command: invalid status value: " + args[4]);
        return buildResponse("ERROR", {"Invalid status value"});
    }
    long long startValue = 0;
    long long endValue = 0;
    if (!parseLongLong(args[5], startValue) || !parseLongLong(args[6], endValue)) {
        g_logger.warning("QueryParcel command: invalid time values: " + args[5] + ", " + args[6]);
        return buildResponse("ERROR", {"Invalid time values"});
    }
    ParcelStatus status = static_cast<ParcelStatus>(statusInt);
    time_t startTime = static_cast<time_t>(startValue);
    time_t endTime = static_cast<time_t>(endValue);

    g_logger.info("QueryParcel attempt - User: " + m_currentUser->getUsername() + 
                    ", Parcel ID: " + parcelId + ", Sender: " + sender + ", Receiver: " + receiver + ", Courier: " + courier + ", Status: " + std::to_string(statusInt) + ", Start Time: " + std::to_string(startTime) + ", End Time: " + std::to_string(endTime));
    
    std::vector<Parcel*> parcels;

    if (m_currentUser->getUserType() == UserType::ADMINISTRATOR) {
        auto tmp1 = m_system.queryParcels(parcelId, sender, receiver, "", status, startTime, endTime);
        parcels = tmp1.second;
    } else if (m_currentUser->getUserType() == UserType::COURIER) {
        auto tmp2 = m_system.queryParcels(parcelId, sender, receiver, m_currentUser->getUsername(), status, startTime, endTime);
        parcels = tmp2.second;
    } else {
        if (sender.empty() && receiver.empty()) {
            auto q1 = m_system.queryParcels(parcelId, m_currentUser->getUsername(), "", "", status, startTime, endTime);
            auto q2 = m_system.queryParcels(parcelId, "", m_currentUser->getUsername(), "", status, startTime, endTime);
            parcels = q1.second;
            parcels.insert(parcels.end(), q2.second.begin(), q2.second.end());
        } else {
            if (!sender.empty() && sender != m_currentUser->getUsername() && !receiver.empty() && receiver != m_currentUser->getUsername()) {
                g_logger.warning("QueryParcel command: Sender and receiver must be the current user");
                return buildResponse("ERROR", {"Sender and receiver must be the current user"});
            }

            if (sender.empty() && m_currentUser->getUsername() != receiver) sender = m_currentUser->getUsername();
            if (receiver.empty() && m_currentUser->getUsername() != sender) receiver = m_currentUser->getUsername();
            
            auto tmp3 = m_system.queryParcels(parcelId, sender, receiver, "", status, startTime, endTime);
            parcels = tmp3.second;
        }
    }
    
    g_logger.debug("QueryParcel result - User: " + m_currentUser->getUsername() + 
                    ", Parcel count: " + std::to_string(parcels.size()));
    
    std::string data;
    for (size_t i = 0; i < parcels.size(); ++i) {
        if (i > 0) data += DELIMITER;
        data += parcels[i]->serialize();
    }
    return buildResponse("OK", {data});
}

std::string Server::handleRechargeBalance(const User* m_currentUser, const std::vector<std::string>& args) {
    if (args.size() < 1) {
        g_logger.warning("RechargeBalance command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }

    double amount = 0.0;
    if (!parseDouble(args[0], amount)) {
        g_logger.warning("RechargeBalance command: Invalid amount: " + args[0]);
        return buildResponse("ERROR", {"Invalid amount"});
    }
    g_logger.info("RechargeBalance attempt - User: " + m_currentUser->getUsername() + ", Amount: " + std::to_string(amount));
    if (amount <= 0.0) {
        g_logger.warning("RechargeBalance command: Invalid amount: " + args[0]);
        return buildResponse("ERROR", {"Invalid amount"});
    }
    ErrorCode ec = m_system.rechargeUser(m_currentUser->getUsername(), amount);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("User recharge successful - User: " + m_currentUser->getUsername() + ", Amount: " + std::to_string(amount));
        return buildResponse("OK", {"Recharge successful"});
    }
    g_logger.warning("RechargeBalance failed - User: " + m_currentUser->getUsername());
    return buildResponse("ERROR", {"Recharge failed"}, ec);
}

std::string Server::handleQueryBalance(const User* m_currentUser, const std::vector<std::string>& args) {
    auto [ec, balance] = m_system.getUserBalance(m_currentUser->getUsername());
    if (ec != ErrorCode::SUCCESS) {
        g_logger.warning("QueryBalance failed - User not found: " + m_currentUser->getUsername());
        return buildResponse("ERROR", {"User not found"}, ec);
    }
    g_logger.info("QueryBalance request - User: " + m_currentUser->getUsername() + ", Balance: " + std::to_string(balance));
    return buildResponse("OK", {std::to_string(balance)});
}

std::string Server::handleChangePassword(const User* m_currentUser, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        g_logger.warning("ChangePassword command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }
    std::string oldPwd = args[0];
    std::string newPwd = args[1];
    g_logger.info("ChangePassword attempt - User: " + m_currentUser->getUsername() + ", password change requested");

    ErrorCode ec = m_system.changeUserPassword(m_currentUser->getUsername(), oldPwd, newPwd);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Password changed successfully for user: " + m_currentUser->getUsername());
        return buildResponse("OK", {"Password changed"});
    }
    g_logger.warning("ChangePassword failed for user: " + m_currentUser->getUsername());
    return buildResponse("ERROR", {"Password change failed"}, ec);
}

std::string Server::handleDeleteAccount(const User* m_currentUser, const std::vector<std::string>& args) {
    if (args.size() < 1) {
        g_logger.warning("DeleteAccount command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }
    
    std::string targetUsername = args[0];
    
    g_logger.info("DeleteAccount attempt - Admin: " + m_currentUser->getUsername() + 
                    ", Target: " + targetUsername);
    
    ErrorCode ec = m_system.deleteUser(targetUsername);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Account deleted successfully - Admin: " + m_currentUser->getUsername() + 
                        ", Target: " + targetUsername);
        return buildResponse("OK", {"Account deleted successfully"});
    } else {
        g_logger.warning("DeleteAccount failed - Admin: " + m_currentUser->getUsername() + 
                        ", Target: " + targetUsername + " (user not found, has unfinished parcels, or self-deletion attempt)");
        return buildResponse("ERROR", {"Delete failed"}, ec);
    }
}

std::string Server::handleQueryUser(const User* m_currentUser, const std::vector<std::string>& args) {    
    if (args.size() < 2) {
        g_logger.warning("QueryUser command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }
    
    std::string username = args[0];
    int typeInt = 0;
    if (!parseInt(args[1], typeInt) || typeInt < 0 || typeInt > static_cast<int>(UserType::ADMINISTRATOR)) {
        g_logger.warning("QueryUser command: Invalid user type: " + args[1]);
        return buildResponse("ERROR", {"Invalid user type"});
    }
    UserType userType = static_cast<UserType>(typeInt);

    if (m_currentUser->getUserType() == UserType::CUSTOMER || m_currentUser->getUserType() == UserType::COURIER) {
        if (username != m_currentUser->getUsername()) {
            g_logger.warning("QueryUser command: Permission denied for user: " + 
                (m_currentUser ? m_currentUser->getUsername() : "anonymous"));
            return buildResponse("ERROR", {"Only administrators can query other users"});
        } else if (userType != m_currentUser->getUserType()) {
            g_logger.warning("QueryUser command: Invalid user type: " + std::to_string(typeInt));
            return buildResponse("ERROR", {"Invalid user type"});
        } else {
            std::string userTypeStr = m_currentUser->getUserType() == UserType::CUSTOMER ? "Customer" : "Courier";
            g_logger.info("QueryUser request - " + userTypeStr + " " + username + m_currentUser->getUsername() + ", Type: " + std::to_string(typeInt));
        }
    } else if (m_currentUser->getUserType() == UserType::ADMINISTRATOR) {
        g_logger.info("QueryUser request - Admin: " + m_currentUser->getUsername() + ", User: " + username + ", Type: " + std::to_string(typeInt));
    }
    
    auto [ecUsers, users] = m_system.getUsers(username, userType);
    std::string data;
    for (size_t i = 0; i < users.size(); ++i) {
        if (!data.empty()) data += DELIMITER;
        data += users[i]->serialize();
    }
    
    g_logger.debug("QueryUser result - Admin: " + m_currentUser->getUsername() + 
                    ", User count: " + std::to_string(users.size()));
    
    return buildResponse("OK", {data});
}

std::string Server::handleDeleteParcel(const User* m_currentUser, const std::vector<std::string>& args) {
    if (args.size() < 1) {
        g_logger.warning("DeleteParcel command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse("ERROR", {"Invalid arguments"});
    }
    
    std::string parcelId = args[0];
    ErrorCode ec = m_system.deleteParcel(parcelId);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Parcel deleted successfully - Parcel ID: " + parcelId);
        return buildResponse("OK", {"Parcel deleted successfully"});
    } else {
        g_logger.warning("DeleteParcel failed - Parcel ID: " + parcelId);
        return buildResponse("ERROR", {"Delete failed: parcel not found"}, ec);
    }
}

std::string Server::handleGetStatistics(const User* m_currentUser, const std::vector<std::string>& args) {
    g_logger.info("GetStatistics request - Admin: " + m_currentUser->getUsername());  
    auto [ecStats, stats] = m_system.getStatistics();
    std::vector<std::string> data;
    if (stats.empty()) {
        return buildResponse("ERROR", {"No statistics available"});
    }
    try {
        data.push_back(std::to_string(static_cast<int>(stats["totalUsers"])));
        data.push_back(std::to_string(static_cast<int>(stats["totalParcels"])));
        data.push_back(std::to_string(stats["totalBalance"]));
    } catch (const std::out_of_range& e) {
        return buildResponse("ERROR", {"Statistics key not found"});
    }
    return buildResponse("OK", data);
}
