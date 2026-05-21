#include "Communication.h"

Logger g_logger;
std::string Communication::getSocketErrorString() const {
#ifdef _WIN32
    int err = WSAGetLastError();
    return "WSA error " + std::to_string(err);
#else
    return std::string(std::strerror(errno));
#endif
}

// 发送请求并接收完整响应（支持大包分次接收直到 \n）
bool Communication::sendRequest(const std::string& cmd, const std::vector<std::string>& args, std::string& response) {
    if (!m_connected) return false;
    std::string msg = Protocol::buildRequest(cmd, args);
    g_logger.debug("Sending request: " + msg);
    if (send(m_socket, msg.c_str(), static_cast<int>(msg.size()), 0) == SOCKET_ERROR) {
        g_logger.error("sendRequest failed while sending: " + getSocketErrorString());
        m_connected = false;
        return false;
    }

    response.clear();
    while (true) {
        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        int len = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (len < 0) {
            g_logger.error("sendRequest failed while receiving response: " + getSocketErrorString());
            m_connected = false;
            return false;
        }
        if (len == 0) {
            g_logger.error("sendRequest failed: connection closed by server");
            m_connected = false;
            return false;
        }
        response += std::string(buffer, len);
        if (response.find('\n') != std::string::npos) break;
    }

    g_logger.debug("Received raw response: " + response);
    return true;
}

// 解析 RESPONSE 消息：提取状态、错误码（如有）、数据字段
bool Communication::parseResponse(const std::string& resp, ErrorCode& status, std::vector<std::string>& data) {
    std::istringstream iss(resp);
    std::string token;
    if (!std::getline(iss, token, DELIMITER)) {
        g_logger.warning("parseResponse failed: missing response header");
        return false;
    }
    if (token != Command::RESPONSE) {
        g_logger.warning("parseResponse failed: unexpected response command: " + token);
        return false;
    }
    if (!std::getline(iss, token, DELIMITER)) {
        g_logger.warning("parseResponse failed: missing status field");
        return false;
    }
    data.clear();
    // ERROR 响应第二位为错误码，解析并存储
    int code = 0;
    if (parseInt(token, code)) {
        status = static_cast<ErrorCode>(code);
    } else {
        status = ErrorCode::UNKNOWN;
    }
    g_logger.debug("Parsed response status: " + token + ", raw: " + resp);
    while (std::getline(iss, token, DELIMITER)) data.push_back(token);
    return true;
}


bool Communication::connectToServer(const std::string& ip, int port) {
    g_logger.info("Connecting to server " + ip + ":" + std::to_string(port));
    // 保存服务器地址，供断线重连使用
    m_serverIp = ip;
    m_serverPort = port;
#ifdef _WIN32
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
                g_logger.error("WSAStartup failed");
                return false;
            }
#endif
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == INVALID_SOCKET) {
        g_logger.error("Socket creation failed: " + getSocketErrorString());
        return false;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        g_logger.error("Connect failed to " + ip + ":" + std::to_string(port) + " - " + getSocketErrorString());
        CLOSE_SOCKET(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }
    m_connected = true;
    g_logger.info("Connected to server successfully");
    return true;
}

void Communication::disconnect() {
    if (m_socket != INVALID_SOCKET) {
        CLOSE_SOCKET(m_socket);
        m_socket = INVALID_SOCKET;
        g_logger.info("Disconnected from server");
    }
    m_connected = false;
    #ifdef _WIN32
            WSACleanup();
    #endif
}

// 用户登录
ErrorCode Communication::loginUser(const std::string& username, const std::string& password, UserType type, std::string& userId) {
    std::string resp;
    if (!sendRequest(Command::LOGIN, {username, password, std::to_string(static_cast<int>(type))}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || data.empty()) return ErrorCode::INVALID_ARGS;
    userId = data[0];
    return status;
}

// 发送快递
ErrorCode Communication::sendParcel(const std::string& receiver, ParcelType type, double weight, const std::string& desc, std::string& parcelId) {
    std::string resp;
    if (!sendRequest(Command::SEND_PARCEL, {receiver, std::to_string(static_cast<int>(type)),
                        std::to_string(weight), desc}, resp))
        return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || data.empty()) return ErrorCode::INVALID_ARGS;
    parcelId = data[0];
    return status;
}

// 管理员分配快递员
ErrorCode Communication::assignParcel(const std::string& parcelId, const std::string& courier) {
    std::string resp;
    if (!sendRequest(Command::ASSIGN_PARCEL, {parcelId, courier}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return ErrorCode::INVALID_ARGS;
    return status;
}

// 快递员揽收
ErrorCode Communication::collectParcels(const std::vector<std::string>& ids, std::vector<std::string>& collectedList) {
    std::string resp;
    if (!sendRequest(Command::COLLECT_PARCEL, ids, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || data.empty()) return ErrorCode::INVALID_ARGS;
    collectedList.clear();
    for (const auto& raw : data) {
        collectedList.push_back(raw);
    }
    return status;
}

// 用户签收
ErrorCode Communication::signParcels(const std::vector<std::string>& ids, std::vector<std::string>& signedList) {
    std::string resp;
    if (!sendRequest(Command::SIGN_PARCEL, ids, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || data.empty()) return ErrorCode::INVALID_ARGS;
    signedList.clear();
    for (const auto& raw : data) {
        signedList.push_back(raw);
    }
    return status;
}

// 查询快递
ErrorCode Communication::queryParcels(const std::string& Id, const std::string& sender, const std::string& receiver, 
        const std::string& courier, const ParcelStatus& s, const time_t start, const time_t end, std::vector<Parcel>& parcels) {
    std::string resp;
    if (!sendRequest(Command::QUERY_PARCEL, {Id, sender, receiver, courier, std::to_string(static_cast<int>(s)), std::to_string(start), std::to_string(end)}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || data.empty()) return ErrorCode::INVALID_ARGS;
    parcels.clear();
    for (int i = 0; i + 9 < data.size(); i += 10) {
        Parcel parcel(static_cast<ParcelType>(std::stoi(data[i])), data[i + 1], data[i + 2], data[i + 3], static_cast<time_t>(std::stoi(data[i + 4])), static_cast<time_t>(std::stoi(data[i + 5])),
                        static_cast<ParcelStatus>(std::stoi(data[i + 6])), data[i + 7], std::stod(data[i + 8]),
                        data[i + 9]);
        parcels.push_back(parcel);
    }
    return status;
}

// 管理员查询用户
ErrorCode Communication::queryUsers(const std::string& username, const UserType type, std::vector<User*>& users) {
    std::string resp;
    if (!sendRequest(Command::QUERY_USER, {username, std::to_string(static_cast<int>(type))}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || data.empty()) return ErrorCode::INVALID_ARGS;
    users.clear();
    for (int i = 0; i + 6 < data.size(); i += 7) {
        switch (static_cast<UserType>(stoi(data[i]))) {
            case UserType::CUSTOMER:
                users.push_back(new Customer(data[i + 1], data[i + 2], data[i + 3], data[i + 4], data[i + 5], std::stod(data[i + 6])));
                break;
            case UserType::COURIER:
                users.push_back(new Courier(data[i + 1], data[i + 2], data[i + 3], data[i + 4], data[i + 5], std::stod(data[i + 6])));
                break;
            case UserType::ADMINISTRATOR:
                users.push_back(new Administrator(data[i + 1], data[i + 2], data[i + 3], data[i + 4], data[i + 5], std::stod(data[i + 6])));
                break;
        }
    }
    return status;
}

ErrorCode Communication::registerUser(const std::string& username, const std::string& password,
                    const std::string& name, const std::string& phone, const std::string& addr, UserType type) {
    std::string resp;
    if (!sendRequest(Command::REGISTER, {username, password, name, phone, addr, std::to_string(static_cast<int>(type))}, resp))
        return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return ErrorCode::INVALID_ARGS;
    return status;
}

ErrorCode Communication::rechargeBalance(double amount) {
    std::string resp;
    if (!sendRequest(Command::RECHARGE_BALANCE, {std::to_string(amount)}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return ErrorCode::INVALID_ARGS;
    return status;
}

ErrorCode Communication::queryBalance(double& balance) {
    std::string resp;
    if (!sendRequest(Command::QUERY_BALANCE, {}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || data.empty()) return ErrorCode::INVALID_ARGS;
    if (!parseDouble(data[0], balance)) return ErrorCode::INVALID_ARGS;
    return status;
}

ErrorCode Communication::changePassword(const std::string& oldPwd, const std::string& newPwd) {
    std::string resp;
    if (!sendRequest(Command::CHANGE_PASSWORD, {oldPwd, newPwd}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return ErrorCode::INVALID_ARGS;
    return status;
}

// 注销账户（管理员功能）
ErrorCode Communication::deleteAccount(const std::string& targetUsername) {
    std::string resp;
    if (!sendRequest(Command::DELETE_ACCOUNT, {targetUsername}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return ErrorCode::INVALID_ARGS;
    return status;
}

// 删除快递（管理员功能）
ErrorCode Communication::deleteParcel(const std::string& parcelId) {
    std::string resp;
    if (!sendRequest(Command::DELETE_PARCEL, {parcelId}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return ErrorCode::INVALID_ARGS;
    return status;
}

ErrorCode Communication::logout() {
    std::string resp;
    if (!sendRequest(Command::LOGOUT, {}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return ErrorCode::INVALID_ARGS;
    return status;
}

ErrorCode Communication::getStatistics(int& totalUsers, int& totalParcels, int& pendingCollection, int& collected, int& Signed, double& adminTotalBalance) {
    std::string resp;
    if (!sendRequest(Command::GET_STATISTICS, {}, resp)) return ErrorCode::INTERNAL_ERROR;
    ErrorCode status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return ErrorCode::INVALID_ARGS;
    if (status != ErrorCode::SUCCESS) return status;
    if (data.size() != 6) return ErrorCode::INVALID_ARGS;
    if (!parseInt(data[0], totalUsers)) {
        std::cout << "获取用户总数失败。" << std::endl;
        return ErrorCode::INVALID_ARGS;
    }
    if (!parseInt(data[1], totalParcels)) {
        std::cout << "获取快递总数失败。" << std::endl;
        return ErrorCode::INVALID_ARGS;
    }
    if (!parseInt(data[2], pendingCollection)) {
        std::cout << "获取待收快递总数失败。" << std::endl;
        return ErrorCode::INVALID_ARGS;
    }   
    if (!parseInt(data[3], collected)) {
        std::cout << "获取已收快递总数失败。" << std::endl;
        return ErrorCode::INVALID_ARGS;
    }
    if (!parseInt(data[4], Signed)) {
        std::cout << "获取已签收快递总数失败。" << std::endl;
        return ErrorCode::INVALID_ARGS;
    }
    if (!parseDouble(data[5], adminTotalBalance)) {
        std::cout << "获取管理员总余额失败。" << std::endl;
        return ErrorCode::INVALID_ARGS;
    }
    return status;
}

void Communication::setAutoReconnectInfo(const std::string& username, const std::string& password, UserType type) {
    m_reloginUsername = username;
    m_reloginPassword = password;
    m_reloginType = type;
    m_hasReconnectInfo = true;
    g_logger.info("Auto-reconnect credentials saved for user: " + username);
}

bool Communication::reconnectAndRelogin() {
    if (!m_hasReconnectInfo) {
        g_logger.error("reconnectAndRelogin failed: no saved credentials");
        return false;
    }
    g_logger.info("Attempting reconnect to " + m_serverIp + ":" + std::to_string(m_serverPort));
    disconnect();
    if (!connectToServer(m_serverIp, m_serverPort)) {
        g_logger.error("Reconnect failed: could not connect to server");
        return false;
    }
    std::string userId;
    ErrorCode ec = loginUser(m_reloginUsername, m_reloginPassword, m_reloginType, userId);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Reconnect and relogin successful for user: " + m_reloginUsername);
        return true;
    }
    g_logger.error("Reconnect failed: relogin error " + std::to_string(static_cast<int>(ec)));
    return false;
}
