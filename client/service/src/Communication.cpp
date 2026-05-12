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
bool Communication::parseResponse(const std::string& resp, std::string& status, std::vector<std::string>& data) {
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
    if (!std::getline(iss, status, DELIMITER)) {
        g_logger.warning("parseResponse failed: missing status field");
        return false;
    }
    data.clear();
    // ERROR 响应第二位为错误码，解析并存储
    if (status == "ERROR") {
        if (std::getline(iss, token, DELIMITER)) {
            int code = 0;
            if (parseInt(token, code)) {
                m_lastErrorCode = static_cast<ErrorCode>(code);
            } else {
                m_lastErrorCode = ErrorCode::UNKNOWN;
            }
        }
    }
    g_logger.debug("Parsed response status: " + status + ", raw: " + resp);
    while (std::getline(iss, token, DELIMITER)) data.push_back(token);
    return true;
}


bool Communication::connectToServer(const std::string& ip, int port) {
    g_logger.info("Connecting to server " + ip + ":" + std::to_string(port));
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
bool Communication::loginUser(const std::string& username, const std::string& password, UserType type, std::string& userId) {
    std::string resp;
    if (!sendRequest(Command::LOGIN, {username, password, std::to_string(static_cast<int>(type))}, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return false;
    if (status == "OK" && !data.empty()) {
        userId = data[0];
        return true;
    }
    return false;
}

// 发送快递
bool Communication::sendParcel(const std::string& receiver, ParcelType type, double weight, const std::string& desc, std::string& parcelId) {
    std::string resp;
    if (!sendRequest(Command::SEND_PARCEL, {receiver, std::to_string(static_cast<int>(type)),
                        std::to_string(weight), desc}, resp))
        return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return false;
    if (status == "OK" && !data.empty()) {
        parcelId = data[0];
        return true;
    }
    return false;
}

// 管理员分配快递员
bool Communication::assignParcel(const std::string& parcelId, const std::string& courier) {
    std::string resp;
    if (!sendRequest(Command::ASSIGN_PARCEL, {parcelId, courier}, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return false;
    return status == "OK";
}

// 快递员揽收
bool Communication::collectParcels(const std::vector<std::string>& ids, std::vector<std::string>& collectedList) {
    std::string resp;
    if (!sendRequest(Command::COLLECT_PARCEL, ids, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || status != "OK" || data.empty()) return false;
    collectedList.clear();
    for (const auto& raw : data) {
        collectedList.push_back(raw);
    }
    return true;
}

// 用户签收
bool Communication::signParcels(const std::vector<std::string>& ids, std::vector<std::string>& signedList) {
    std::string resp;
    if (!sendRequest(Command::SIGN_PARCEL, ids, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || status != "OK" || data.empty()) return false;
    signedList.clear();
    for (const auto& raw : data) {
        signedList.push_back(raw);
    }
    return true;
}

// 查询快递
bool Communication::queryParcels(const std::string& Id, const std::string& sender, const std::string& receiver, 
        const std::string& courier, const ParcelStatus& s, const time_t start, const time_t end, std::vector<Parcel>& parcels) {
    std::string resp;
    if (!sendRequest(Command::QUERY_PARCEL, {Id, sender, receiver, courier, std::to_string(static_cast<int>(s)), std::to_string(start), std::to_string(end)}, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || status != "OK" || data.empty()) return false;
    parcels.clear();
    for (int i = 0; i + 9 < data.size(); i += 10) {
        Parcel parcel(static_cast<ParcelType>(std::stoi(data[i])), data[i + 1], data[i + 2], data[i + 3], static_cast<time_t>(std::stoi(data[i + 4])), static_cast<time_t>(std::stoi(data[i + 5])),
                        static_cast<ParcelStatus>(std::stoi(data[i + 6])), data[i + 7], std::stod(data[i + 8]),
                        data[i + 9]);
        parcels.push_back(parcel);
    }
    return true;
}

// 管理员查询用户
bool Communication::queryUsers(const std::string& username, const UserType type, std::vector<User>& users) {
    std::string resp;
    if (!sendRequest(Command::QUERY_USER, {username, std::to_string(static_cast<int>(type))}, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || status != "OK" || data.empty()) return false;
    for (int i = 0; i + 6 < data.size(); i += 7) {
        users.push_back(User(data[i + 1], data[i + 2], data[i + 3], data[i + 4], data[i + 5], std::stod(data[i + 6])));
    }
    return true;
}

bool Communication::registerUser(const std::string& username, const std::string& password,
                    const std::string& name, const std::string& phone, const std::string& addr, UserType type) {
    std::string resp;
    if (!sendRequest(Command::REGISTER, {username, password, name, phone, addr, std::to_string(static_cast<int>(type))}, resp))
        return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return false;
    return status == "OK";
}

bool Communication::rechargeBalance(double amount) {
    std::string resp;
    if (!sendRequest(Command::RECHARGE_BALANCE, {std::to_string(amount)}, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return false;
    return status == "OK";
}

bool Communication::queryBalance(double& balance) {
    std::string resp;
    if (!sendRequest(Command::QUERY_BALANCE, {}, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || status != "OK" || data.empty()) return false;
    try {
        balance = std::stod(data[0]);
    } catch (...) {
        return false;
    }
    return true;
}

bool Communication::changePassword(const std::string& oldPwd, const std::string& newPwd) {
    std::string resp;
    if (!sendRequest(Command::CHANGE_PASSWORD, {oldPwd, newPwd}, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return false;
    return status == "OK";
}

// 注销账户（管理员功能）
bool Communication::deleteAccount(const std::string& targetUsername) {
    std::string resp;
    if (!sendRequest(Command::DELETE_ACCOUNT, {targetUsername}, resp))
        return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return false;
    return status == "OK";
}

// 删除快递（管理员功能）
bool Communication::deleteParcel(const std::string& parcelId) {
    std::string resp;
    if (!sendRequest(Command::DELETE_PARCEL, {parcelId}, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return false;
    return status == "OK";
}

bool Communication::logout() {
    std::string resp;
    if (!sendRequest(Command::LOGOUT, {}, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data)) return false;
    return status == "OK";
}

bool Communication::getStatistics(std::vector<std::string>& stats) {
    std::string resp;
    if (!sendRequest(Command::GET_STATISTICS, {}, resp)) return false;
    std::string status;
    std::vector<std::string> data;
    if (!parseResponse(resp, status, data) || status != "OK" || data.empty()) return false;
    stats = data;
    return true;
}