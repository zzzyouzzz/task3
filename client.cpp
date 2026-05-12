// client.cpp – 物流管理系统客户端
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cerrno>
#include <climits>
#include <cfloat>
#include <cctype>
#include <memory>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    typedef int socket_t;
    #define CLOSE_SOCKET close
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

#include "common.h"

Logger g_logger;

// ================== 通信类 ==================
class Communication {
private:
    socket_t m_socket;
    bool m_connected;

    std::string getSocketErrorString() const {
    #ifdef _WIN32
        int err = WSAGetLastError();
        return "WSA error " + std::to_string(err);
    #else
        return std::string(std::strerror(errno));
    #endif
    }

    // 发送请求并获取响应
    bool sendRequest(const std::string& cmd, const std::vector<std::string>& args, std::string& response) {
        if (!m_connected) return false;
        std::string msg = Protocol::buildRequest(cmd, args);
        g_logger.debug("Sending request: " + msg);
        if (send(m_socket, msg.c_str(), static_cast<int>(msg.size()), 0) == SOCKET_ERROR) {
            g_logger.error("sendRequest failed while sending: " + getSocketErrorString());
            m_connected = false;
            return false;
        }

        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        int len = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) {
            g_logger.error("sendRequest failed while receiving response: " + getSocketErrorString());
            m_connected = false;
            return false;
        }
        response = std::string(buffer, len);
        g_logger.debug("Received raw response: " + response);
        return true;
    }

    // 解析RESPONSE消息
    bool parseResponse(const std::string& resp, std::string& status, std::vector<std::string>& data) {
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
        g_logger.debug("Parsed response status: " + status + ", raw: " + resp);
        while (std::getline(iss, token, DELIMITER)) data.push_back(token);
        return true;
    }

public:
    Communication() : m_socket(INVALID_SOCKET), m_connected(false) {}
    ~Communication() {
        disconnect();
    }

    bool connectToServer(const std::string& ip, int port) {
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

    void disconnect() {
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
    bool loginUser(const std::string& username, const std::string& password, UserType type, std::string& userId) {
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
    bool sendParcel(const std::string& receiver, ParcelType type, double weight, const std::string& desc, std::string& parcelId) {
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
    bool assignParcel(const std::string& parcelId, const std::string& courier) {
        std::string resp;
        if (!sendRequest(Command::ASSIGN_PARCEL, {parcelId, courier}, resp)) return false;
        std::string status;
        std::vector<std::string> data;
        if (!parseResponse(resp, status, data)) return false;
        return status == "OK";
    }

    // 快递员揽收
    bool collectParcels(const std::vector<std::string>& ids, std::vector<std::string>& collectedList) {
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
    bool signParcels(const std::vector<std::string>& ids, std::vector<std::string>& signedList) {
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
    bool queryParcels(const std::string& Id, const std::string& sender, const std::string& receiver, 
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
    bool queryUsers(const std::string& username, const UserType type, std::vector<User>& users) {
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

    bool registerUser(const std::string& username, const std::string& password,
                      const std::string& name, const std::string& phone, const std::string& addr, UserType type) {
        std::string resp;
        if (!sendRequest(Command::REGISTER, {username, password, name, phone, addr, std::to_string(static_cast<int>(type))}, resp))
            return false;
        std::string status;
        std::vector<std::string> data;
        if (!parseResponse(resp, status, data)) return false;
        return status == "OK";
    }

    bool rechargeBalance(double amount) {
        std::string resp;
        if (!sendRequest(Command::RECHARGE_BALANCE, {std::to_string(amount)}, resp)) return false;
        std::string status;
        std::vector<std::string> data;
        if (!parseResponse(resp, status, data)) return false;
        return status == "OK";
    }

    bool queryBalance(double& balance) {
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

    bool changePassword(const std::string& oldPwd, const std::string& newPwd) {
        std::string resp;
        if (!sendRequest(Command::CHANGE_PASSWORD, {oldPwd, newPwd}, resp)) return false;
        std::string status;
        std::vector<std::string> data;
        if (!parseResponse(resp, status, data)) return false;
        return status == "OK";
    }

    // 注销账户（管理员功能）
    bool deleteAccount(const std::string& targetUsername) {
        std::string resp;
        if (!sendRequest(Command::DELETE_ACCOUNT, {targetUsername}, resp))
            return false;
        std::string status;
        std::vector<std::string> data;
        if (!parseResponse(resp, status, data)) return false;
        return status == "OK";
    }

    // 删除快递（管理员功能）
    bool deleteParcel(const std::string& parcelId) {
        std::string resp;
        if (!sendRequest(Command::DELETE_PARCEL, {parcelId}, resp)) return false;
        std::string status;
        std::vector<std::string> data;
        if (!parseResponse(resp, status, data)) return false;
        return status == "OK";
    }

    bool logout() {
        std::string resp;
        if (!sendRequest(Command::LOGOUT, {}, resp)) return false;
        std::string status;
        std::vector<std::string> data;
        if (!parseResponse(resp, status, data)) return false;
        return status == "OK";
    }

};


// ================== 客户端界面 ==================
class LogisticsClient {
private:
    Communication m_system;
    bool is_running = true;

    bool readInt(const std::string& prompt, int& value, int minValue = INT_MIN, int maxValue = INT_MAX) {
        while (true) {
            std::cout << prompt;
            std::string line;
            if (!std::getline(std::cin, line)) return false;
            std::istringstream iss(line);
            if (!(iss >> value) || !iss.eof()) {
                std::cout << "请输入有效数字。\n";
                continue;
            }
            if (value < minValue || value > maxValue) {
                std::cout << "请输入合法范围内的值。\n";
                continue;
            }
            return true;
        }
    }

    bool readDouble(const std::string& prompt, double& value, double minValue = -DBL_MAX, double maxValue = DBL_MAX) {
        while (true) {
            std::cout << prompt;
            std::string line;
            if (!std::getline(std::cin, line)) return false;
            std::istringstream iss(line);
            if (!(iss >> value) || !iss.eof()) {
                std::cout << "请输入有效数字。\n";
                continue;
            }
            if (value < minValue || value > maxValue) {
                std::cout << "请输入合法范围内的值。\n";
                continue;
            }
            return true;
        }
    }

    

    bool readString(const std::string& prompt, std::string& value) {
        std::cout << prompt;
            if (!std::getline(std::cin, value)) return false;
            value = trimString(value);
            return true;
    
    }

    bool readTime(const std::string& prompt, time_t& value) {
        std::cout << prompt;
        std::cout << "(格式:YYYY-MM-DD HH:MM:SS)";
        std::string line;
        if (!std::getline(std::cin, line)) return false;
        if (line.empty()) {
            value = 0;
            return true;
        }

        int year, month, day, hour, minute, second;
        char tail;
        if (sscanf(line.c_str(), "%4d-%2d-%2d %2d:%2d:%2d%c", &year, &month, &day, &hour, &minute, &second, &tail) != 6) {
            return false;
        }

        auto isLeapYear = [](int y) {
            return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
        };
        auto daysInMonth = [&](int y, int m) {
            static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            if (m == 2) return mdays[1] + (isLeapYear(y) ? 1 : 0);
            return mdays[m - 1];
        };

        if (year < 1900 || year > 2099 || month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month) ||
            hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
            return false;
        }

        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = -1;

        value = mktime(&tm);
        if (value == (time_t)-1) return false;

        std::tm* normalized = localtime(&value);
        if (!normalized) return false;
        if (normalized->tm_year != tm.tm_year || normalized->tm_mon != tm.tm_mon || normalized->tm_mday != tm.tm_mday ||
            normalized->tm_hour != tm.tm_hour || normalized->tm_min != tm.tm_min || normalized->tm_sec != tm.tm_sec) {
            return false;
        }

        return true;
    }

public:
    LogisticsClient(const std::string& ip, const int port) {
        if (!m_system.connectToServer(ip, port)) {
            is_running = false;
            g_logger.error("Unable to connect to server at " + ip + ":" + std::to_string(port));
            std::cerr << "连接服务器失败。" << std::endl;
            return;
        }
        is_running = true;
        g_logger.info("Client connected to server at " + ip + ":" + std::to_string(port));
        std::cout << "连接服务器成功！" << std::endl;
    }

    static std::string trimString(const std::string& value) {
        size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) start++;
        size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) end--;
        return value.substr(start, end - start);
    }
    void run() {
        
        while (is_running) {
            std::cout << "\n=== 主菜单 ===" << std::endl;
            std::cout << "1. 用户登录" << std::endl;
            std::cout << "2. 新用户注册" << std::endl;
            std::cout << "3. 退出系统" << std::endl;
            
            int choice;
            if (!readInt("请选择: ", choice, 1, 3)) {
                std::cout << "输入无效，请重新选择。" << std::endl;
                continue;
            }
            
            switch (choice) {
                case 1: loginUI(); break;
                case 2: registerUI(); break;
                case 3: 
                    std::cout << "感谢使用物流管理系统，再见！" << std::endl;
                    return;
            }
        }
    }

private:
    void loginUI() {
        std::string username, password;
        int userType;
        
        if (!readString("用户名: ", username)) return;
        if (!readString("密码: ", password)) return;
        if (!readInt("用户类型 (0-客户, 1-快递员, 2-管理员): ", userType, 0, 2) || userType < 0 || userType > 2) return;
        
        UserType type = static_cast<UserType>(userType);
        std::string name;
        if (m_system.loginUser(username, password, type, name)) {
            std::cout << "登录成功！欢迎 " << name << "！" << std::endl;
            mainMenu(type);
        } else {
            std::cout << "登录失败，用户名或密码错误。" << std::endl;
        }
    }
    
    void registerUI() {
        std::string username, password, name, phone, address;
        int userType;
        
        if (!readString("用户名: ", username) || username.empty()) return;
        if (!readString("密码: ", password) || password.empty()) return;
        if (!readString("姓名: ", name) || name.empty()) return;
        if (!readString("电话: ", phone) || phone.empty()) return;
        if (!readString("地址: ", address) || address.empty()) return;
        if (!readInt("用户类型 (0-客户, 1-快递员): ", userType, 0, 1) || userType < 0 || userType > 1) return;
        
        UserType type = static_cast<UserType>(userType);
        
        if (m_system.registerUser(username, password, name, phone, address, type)) {
            std::cout << "注册成功！" << std::endl;
        } else {
            std::cout << "注册失败，用户名已存在。" << std::endl;
        }
    }
    
    void mainMenu(UserType type) {
        while (true) {
            std::cout << "\n=== 功能菜单 ===" << std::endl;
            std::cout << "1. 发送快递" << std::endl;
            std::cout << "2. 签收快递" << std::endl;
            std::cout << "3. 查询快递" << std::endl;
            std::cout << "4. 充值余额" << std::endl;
            std::cout << "5. 查询余额" << std::endl;
            std::cout << "6. 修改密码" << std::endl;
            std::cout << "7. 分配快递员" << std::endl;
            std::cout << "8. 揽收快递" << std::endl;
            std::cout << "9. 查询用户" << std::endl;
            std::cout << "10. 注销账户" << std::endl;
            std::cout << "11. 删除快递" << std::endl;          
            std::cout << "0. 注销登录" << std::endl;
            
            int op;
            if (!readInt("请选择: ", op, 0, 11)) {
                std::cout << "输入无效，请重新选择。" << std::endl;
                continue;
            }
            
            switch (op) {
                case 1: 
                    if (type == UserType::CUSTOMER) {
                        sendParcelUI();
                    } else {
                        std::cout << "您没有权限发送快递。" << std::endl;
                    }
                    break;
                case 2: 
                    if (type == UserType::CUSTOMER) {
                        signParcelUI();
                    } else {
                        std::cout << "您没有权限签收快递。" << std::endl;
                    }
                    break;
                case 3: 
                    if (type == UserType::CUSTOMER) {
                        queryParcelUI(UserType::CUSTOMER);
                    } else if (type == UserType::COURIER) {
                        queryParcelUI(UserType::COURIER);
                    } else if (type == UserType::ADMINISTRATOR) {
                        queryParcelUI();
                    }
                    break;
                case 4: rechargeBalanceUI(); break;
                case 5: queryBalanceUI(); break;
                case 6: changePasswordUI(); break;
                case 7: 
                    if (type == UserType::COURIER) {
                        assignParcelUI();
                    } else {
                        std::cout << "您没有权限分配快递员。" << std::endl;
                    }
                    break;
                case 8: 
                    if (type == UserType::COURIER) {
                        collectParcelUI();
                    } else {
                        std::cout << "您没有权限收快递。" << std::endl;
                    }
                    break;
                case 9: 
                    if (type == UserType::ADMINISTRATOR) {
                        queryUsersUI();
                    } else {
                        std::cout << "您没有权限查询用户信息。" << std::endl;
                    }
                    break;
                case 10: 
                    if (type == UserType::ADMINISTRATOR) {
                        deleteAccountUI();
                    } else {
                        std::cout << "您没有权限注销账户。" << std::endl;
                    }
                    break;
                case 11: 
                    if (type == UserType::ADMINISTRATOR) {
                        deleteParcelUI();
                    } else {
                        std::cout << "您没有权限删除快递。" << std::endl;
                    }
                    break;
                case 0: 
                    logoutUI();
                    return;
                default:
                    std::cout << "无效选项" << std::endl;
            }
        }
    }
    
    // 各个功能的具体实现...
    void sendParcelUI() {
        std::string receiver, desc;
        int ptype;
        double weight;
        
        if (!readString("收件人用户名: ", receiver)) return;
        if (!readInt("快递类型 (0-普通, 1-易碎, 2-书籍): ", ptype, 0, 2)) return;
        if (!readDouble("重量(kg): ", weight, 0.1, 1000.0)) return;
        if (!readString("物品描述: ", desc)) return;
        
        ParcelType type = static_cast<ParcelType>(ptype);
        std::string pid;

        if (m_system.sendParcel(receiver, type, weight, desc, pid)) {
            std::cout << "快递发送成功！单号: " << pid << std::endl;
        } else {
            std::cout << "发送失败，请检查收件人是否存在或余额是否充足。" << std::endl;
        }
        
    }
    
    void signParcelUI() {
        std::cout << "请输入要签收的快递单号(多个用逗号分隔): ";
        std::string line; 
        std::getline(std::cin, line);
        
        std::vector<std::string> ids;
        std::istringstream iss(line);
        std::string token;
        while (std::getline(iss, token, ',')) {
            trimString(token);
            if (!token.empty()) {
                ids.push_back(token);
            }
        }
        
        std::vector<std::string> signedList;
        
        if (m_system.signParcels(ids, signedList)) {
            std::cout << "签收成功: ";
            for (size_t i = 0; i < signedList.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << signedList[i];
            }
            std::cout << std::endl;
        } else {
            std::cout << "签收失败，请检查单号是否正确。" << std::endl;
        }
    }
    
    void queryParcelUI(const UserType& queryer = UserType::ADMINISTRATOR) {
        std::vector<Parcel> parcels;
        std::string parcelId;
        std::string sender;
        std::string receiver;
        time_t start, end;
        ParcelStatus status;
        int statusInt;
        int anser;
        if (!readInt("是否查询所有快递？(0-否, 1-是): ", anser)) return;
        if (anser == 1) {
            parcelId = "";
            sender = "";
            receiver = "";
            status = ParcelStatus::OTHER;
            start = 0;
            end = 0;
        } else {
            if (!readString("请输入快递单号(可选): ", parcelId)) return;
            if (!readString("请输入寄件人(可选): ", sender)) return;
            if (!readString("请输入收件人(可选): ", receiver)) return;
            if (!sender.empty() && !receiver.empty() && sender == receiver) {
                std::cout << "寄件人和收件人不能相同。" << std::endl;
                return;
            }
            if (!readInt("请输入状态 (0-待揽收, 1-待签收, 2-已签收, 3-其他): ", statusInt, 0, 3)) return;
            status = static_cast<ParcelStatus>(statusInt);
            if (!readTime("请输入开始时间(可选): ", start)) return;
            if (!readTime("请输入结束时间(可选): ", end)) return;
            if (end > time(nullptr)) {
                std::cout << "结束时间不能晚于当前时间。" << std::endl;
                return;
            }
        }
        bool success = m_system.queryParcels(parcelId, sender, receiver, "", status, start, end, parcels);
        
        if (!success) {
            std::cout << "查询失败。" << std::endl;
            return;
        }
        
        std::cout << "快递列表:" << std::endl;
        for (const auto& parcel : parcels) {
            std::cout << "单号: " << parcel.getParcelId() 
                      << ", 寄件人: " << parcel.getSenderName()
                      << ", 收件人: " << parcel.getReceiverName()
                      << ", 状态: " << static_cast<int>(parcel.getStatus())
                      << ", 快递员: " << parcel.getCourierName() << std::endl;
        }
    }
    
    void rechargeBalanceUI() {
        double amount;
        if (!readDouble("充值金额: ", amount, 0.1, 10000.0)) return;
        
        if (m_system.rechargeBalance(amount)) {
            std::cout << "充值成功！" << std::endl;
        } else {
            std::cout << "充值失败。" << std::endl;
        }
    }
    
    void queryBalanceUI() {
        double balance;
        if (m_system.queryBalance(balance)) {
            std::cout << "当前余额: " << balance << " 元" << std::endl;
        } else {
            std::cout << "查询失败。" << std::endl;
        }
    }
    
    void changePasswordUI() {
        std::string oldPwd, newPwd;
        
        if (!readString("请输入旧密码: ", oldPwd)) return;
        if (!readString("请输入新密码: ", newPwd)) return;
        
        if (m_system.changePassword(oldPwd, newPwd)) {
            std::cout << "密码修改成功！" << std::endl;
        } else {
            std::cout << "密码修改失败，请检查旧密码是否正确。" << std::endl;
        }
    }
    
    void assignParcelUI() {
        std::string parcelId, courier;
        
        if (!readString("快递单号: ", parcelId)) return;
        if (!readString("快递员用户名: ", courier)) return;
        
        if (m_system.assignParcel(parcelId, courier)) {
            std::cout << "分配成功！" << std::endl;
        } else {
            std::cout << "分配失败，请检查单号和快递员是否正确。" << std::endl;
        }
    }
    
    void collectParcelUI() {
        std::cout << "请输入要揽收的快递单号(多个用逗号分隔): ";
        std::string line; 
        std::getline(std::cin, line);
        
        std::vector<std::string> ids;
        std::istringstream iss(line);
        std::string token;
        while (std::getline(iss, token, ',')) ids.push_back(token);
        
        std::vector<std::string> collected; 
        
        
        if (m_system.collectParcels(ids, collected) && !collected.empty()) {
            std::cout << "揽收成功: ";
            for (size_t i = 0; i < collected.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << collected[i];
            }
            std::cout << std::endl;
        } else {
            std::cout << "揽收失败" << std::endl;
        }
    }
    
    void queryUsersUI() {
        std::vector<User> users;
        UserType userType;
        int typeInt;
        
        if (!readInt("请输入用户类型(0:客户, 1:快递员, 2:全部): ", typeInt, 0, 2)) return;
        userType = static_cast<UserType>(typeInt);
        
        

        if (!m_system.queryUsers("", userType, users) || users.empty()) {
            std::cout << "暂无用户。" << std::endl;
            return;
        }
        
        std::cout << "用户列表:" << std::endl;
        for (const auto& user : users) {
            std::cout << "用户名: " << user.getUsername() 
                      << ", 姓名: " << user.getName()
                      << ", 电话: " << user.getPhone() << std::endl;
        }
    }
    
    void deleteAccountUI() {
        std::string targetUsername;
        
        if (!readString("请输入要注销的用户名: ", targetUsername)) return;
        
        // 确认操作
        std::string confirm;
        std::cout << "确认要注销用户 '" << targetUsername << "' 吗？(输入'y'确认): ";
        if (!std::getline(std::cin, confirm) || confirm != "y") {
            std::cout << "操作已取消。" << std::endl;
            return;
        }
        
        if (m_system.deleteAccount(targetUsername)) {
            std::cout << "用户账户注销成功。" << std::endl;
        } else {
            std::cout << "注销失败 " << std::endl;
        }
    }

    void deleteParcelUI() {
        std::string parcelId;
        if (!readString("请输入要删除的快递单号: ", parcelId)) return;
        if (m_system.deleteParcel(parcelId)) {
            std::cout << "快递删除成功。" << std::endl;
        } else {
            std::cout << "删除失败 " << std::endl;
        }
    }

    void logoutUI() {
        if (m_system.logout()) {
            std::cout << "已注销。" << std::endl;
        } else {
            std::cout << "注销失败。" << std::endl;
        } 
    }
    
};

bool loadStartupConfig(const std::string& filename, std::string& ip, int& port) {
    g_logger.initialize("client.log", LOG_INFO);
    std::ifstream file(filename);
    if (!file.is_open()) {
        g_logger.error("Unable to open client config file: " + filename);
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = LogisticsClient::trimString(line);
        if (line.empty() || line[0] == '#') continue;
        auto separator = line.find('=');
        if (separator == std::string::npos) continue;
        std::string key = LogisticsClient::trimString(line.substr(0, separator));
        std::string value = LogisticsClient::trimString(line.substr(separator + 1));
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

int main() {
    std::string ip, filename = "client_config.txt";
    int port = 0;
    if (!loadStartupConfig(filename, ip, port)) {
        std::cerr << "配置文件加载失败。" << std::endl;
        return 1;
    }
    LogisticsClient client(ip, port);
    client.run();
    return 0;
}