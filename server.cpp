// server.cpp – 物流管理系统服务端（修正版）
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>
#include <iomanip>


#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
    typedef int socklen_t;   // 修正：MinGW 可能缺少 socklen_t
    
    // Windows下inet_ntop的替代实现
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
    #define inet_ntop windows_inet_ntop
    #ifndef INET_ADDRSTRLEN
        #define INET_ADDRSTRLEN 46
    #endif
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
#include <cctype>

static std::string trimString(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) start++;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) end--;
    return value.substr(start, end - start);
}

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

static bool parseInt(const std::string& str, int& value) {
    try {
        size_t idx = 0;
        long parsed = std::stol(str, &idx);
        if (idx != str.size()) return false;
        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

static bool parseLongLong(const std::string& str, long long& value) {
    try {
        size_t idx = 0;
        value = std::stoll(str, &idx);
        return idx == str.size();
    } catch (...) {
        return false;
    }
}

static bool parseDouble(const std::string& str, double& value) {
    try {
        size_t idx = 0;
        value = std::stod(str, &idx);
        return idx == str.size();
    } catch (...) {
        return false;
    }
}

// 全局日志实例
Logger g_logger;


// ================== 数据持久化管理类 ==================
class FileManager {
public:
    static bool saveUsers(const std::string& filename, const std::map<std::string, class User*>& users);
    static std::map<std::string, class User*> loadUsers(const std::string& filename);
    static bool saveParcels(const std::string& filename, const std::map<std::string, class Parcel*>& parcels);
    static std::map<std::string,class Parcel*> loadParcels(const std::string& filename);
    static bool saveConfig(const std::string& filename, double adminBalance);
    static double loadConfig(const std::string& filename);
};



// ================== FileManager 实现 ==================
bool FileManager::saveUsers(const std::string& filename, const std::map<std::string, User*>& users) {
    std::ofstream ofs(filename);
    if (!ofs) return false;
    for (const auto& pair : users) {
        ofs << pair.second->serialize() << std::endl;
    }
    return true;
}

std::map<std::string, User*> FileManager::loadUsers(const std::string& filename) {
    std::map<std::string, User*> users;
    std::ifstream ifs(filename);
    if (!ifs) return users;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> fields;
        while (std::getline(iss, token, DELIMITER)) {
            fields.push_back(token);
        }
        if (fields.size() < 7) continue;
        int type = std::stoi(fields[0]);
        std::string uname = fields[1], pwd = fields[2], name = fields[3];
        std::string phone = fields[4], addr = fields[5];
        double balance = std::stod(fields[6]);
        User* u = nullptr;
        switch (static_cast<UserType>(type)) {
            case UserType::CUSTOMER: u = new Customer(uname, pwd, name, phone, addr, balance); break;
            case UserType::COURIER: u = new Courier(uname, pwd, name, phone, addr, balance); break;
            case UserType::ADMINISTRATOR: u = new Administrator(uname, pwd, name, phone, addr, balance); break;
        }
        if (u) users[uname] = u;
    }
    return users;
}

bool FileManager::saveParcels(const std::string& filename, const std::map<std::string, Parcel*>& parcels) {
    std::ofstream ofs(filename);
    if (!ofs) return false;
    for (const auto& p : parcels) {
        ofs << p.second->serialize() << std::endl;
    }
    return true;
}

std::map<std::string, Parcel*> FileManager::loadParcels(const std::string& filename) {
    std::map<std::string, Parcel*> parcels;
    std::ifstream ifs(filename);
    if (!ifs) return parcels;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::vector<std::string> fields;
        std::string token;
        while (std::getline(iss, token, DELIMITER)) fields.push_back(token);
        getline(iss, token);
        fields.push_back(token);
        if (fields.size() < 10) continue;
        int type = std::stoi(fields[0]);
        std::string id = fields[1], sender = fields[2], receiver = fields[3];
        time_t sendTime = std::stoll(fields[4]);
        time_t recvTime = std::stoll(fields[5]);
        ParcelStatus status = static_cast<ParcelStatus>(std::stoi(fields[6]));
        std::string desc = fields[7];
        double weight = std::stod(fields[8]);
        std::string courier = fields[9];
        Parcel* p = nullptr;
        switch (static_cast<ParcelType>(type)) {
            case ParcelType::NORMAL: p = new NormalParcel(id, sender, receiver, weight, desc); break;
            case ParcelType::FRAGILE: p = new FragileParcel(id, sender, receiver, weight, desc); break;
            case ParcelType::BOOK: p = new BookParcel(id, sender, receiver, weight, desc); break;
        }
        if (p) {
            p->setStatus(status);
            p->setReceiveTime(recvTime);
            p->setCourier(courier);
            parcels[id] = p;
        }
    }
    return parcels;
}

bool FileManager::saveConfig(const std::string& filename, double adminBalance) {
    std::ofstream ofs(filename);
    if (!ofs) return false;
    ofs << adminBalance << std::endl;
    return true;
}

double FileManager::loadConfig(const std::string& filename) {
    std::ifstream ifs(filename);
    double balance = 0.0;
    if (ifs) ifs >> balance;
    return balance;
}

// ================== 业务系统核心类（系统唯一实例） ==================
class LogisticsSystem {
private:
    std::map<std::string, User*> m_users;       // 用户名 -> User*
    std::map<std::string, Parcel*> m_parcels;   // 快递单号 -> Parcel*
    double m_adminTotalBalance;                 // 管理员总余额（公司资金池）
    mutable std::mutex m_mutex;                 // 修正：mutable 允许在 const 函数中锁定
    int m_nextParcelId;                         // 递增的单号计数器
    bool m_autoAssignCourier;                   // 是否自动分配快递员
    std::map<std::string, int> m_courierCapacity; // 快递员 -> 容量限制

    std::string m_userFile;
    std::string m_parcelFile;
    std::string m_configFile;

    // 生成唯一快递单号
    std::string generateParcelId() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return "PCL" + std::to_string(time(nullptr)) + "-" + std::to_string(m_nextParcelId++);
    }

    // 自动分配快递员
    void autoAssignCourier(std::string parcelId) {
        if (!m_autoAssignCourier) return;
        int minCapacity = INT_MAX;
        std::string minCourier = "";
        for (const auto& pair : m_courierCapacity) {
            if (pair.second < minCapacity) {
                minCapacity = pair.second;
                minCourier = pair.first;
            }
        }
        if (!minCourier.empty()) {
            assignCourier(parcelId, minCourier);
            m_courierCapacity[minCourier]++;

        }
    }

public:
    LogisticsSystem(const std::string& userFile, const std::string& parcelFile, const std::string& configFile, bool autoAssignCourier)
        : m_adminTotalBalance(0.0), m_nextParcelId(1), m_autoAssignCourier(autoAssignCourier),
          m_userFile(userFile), m_parcelFile(parcelFile), m_configFile(configFile) {
        m_users = FileManager::loadUsers(m_userFile);
        m_parcels = FileManager::loadParcels(m_parcelFile);
        m_adminTotalBalance = FileManager::loadConfig(m_configFile);
        // 确保至少有一个管理员账号
        if (m_users.find("admin") == m_users.end()) {
            Administrator* admin = new Administrator("admin", "admin123", "System Admin", "000-0000", "Head Office");
            m_users["admin"] = admin;
            saveData();
        }
        // 计算当前最大单号计数器
        for (const auto& p : m_parcels) {
            std::string id = p.first;
            size_t pos = id.find_last_of('-');
            if (pos != std::string::npos) {
                int num = std::stoi(id.substr(pos+1));
                if (num >= m_nextParcelId) m_nextParcelId = num + 1;
            }
        }

        std::vector<User*> couriers = getUsers("", UserType::COURIER);
        for (const auto& c : couriers) {
            std::string courier = c->getUsername();
            std::vector<Parcel*> parcels = queryParcels("", "", courier);
            m_courierCapacity[courier] = parcels.size();
        } 
    }

    ~LogisticsSystem() {
        saveData();
        for (auto& pair : m_users) delete pair.second;
        for (auto& p : m_parcels) delete p.second;
    }

    void saveData() {
        FileManager::saveUsers(m_userFile, m_users);
        FileManager::saveParcels(m_parcelFile, m_parcels);
        FileManager::saveConfig(m_configFile, m_adminTotalBalance);
    }

    User* loginUser(const std::string& username, const std::string& password, UserType type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_users.find(username);
        if (it == m_users.end()) return nullptr;
        if (it->second->getUserType() != type) return nullptr;
        if (!it->second->login(password)) return nullptr;
        return it->second;
    }

    bool registerUser(const std::string& username, const std::string& password,
                      const std::string& name, const std::string& phone, const std::string& addr, UserType type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_users.find(username) != m_users.end()) return false;
        User* u = nullptr;
        if (type == UserType::CUSTOMER) {
            u = new Customer(username, password, name, phone, addr);
        } else if (type == UserType::COURIER) {
            u = new Courier(username, password, name, phone, addr);
        } else {
            return false; // 不允许注册管理员
        }
        m_users[username] = u;
        saveData();
        return true;
    }


    // 发送快递
    std::string sendParcel(const std::string& senderName, const std::string& receiverName,
                           ParcelType type, double weight, const std::string& desc) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_users.find(receiverName) == m_users.end()) return "";
        User* sender = m_users[senderName];
        Parcel* parcel = nullptr;
        std::string pid = generateParcelId();
        switch (type) {
            case ParcelType::NORMAL:
                parcel = new NormalParcel(pid, senderName, receiverName, weight, desc);
                break;
            case ParcelType::FRAGILE:
                parcel = new FragileParcel(pid, senderName, receiverName, weight, desc);
                break;
            case ParcelType::BOOK:
                parcel = new BookParcel(pid, senderName, receiverName, weight, desc);
                break;
        }
        double price = parcel->getPrice();
        if (!sender->deduct(price)) {
            delete parcel;
            return "";
        }
        m_adminTotalBalance += price;
        m_parcels[pid] = parcel;
        
        // 自动分配快递员（如果启用）
        autoAssignCourier(pid);
        g_logger.info("Auto-assigned courier for parcel: " + pid);
        saveData();
        
        return pid;
    }

    // 分配快递员
    bool assignCourier(const std::string& parcelId, const std::string& courierName) {
        
        // 检查快递员是否存在
        auto courierIt = m_users.find(courierName);
        if (courierIt == m_users.end() || courierIt->second->getUserType() != UserType::COURIER) {
            return false;
        }
        
        // 查找快递
        auto parcelIt = m_parcels.find(parcelId);
        if (parcelIt == m_parcels.end()) {
            return false;
        }
        parcelIt->second->setCourier(courierName);
        parcelIt->second->setStatus(ParcelStatus::PENDING_COLLECTION);
        saveData();
        
        return true;
    }

    // 揽收快递
    std::vector<std::string> collectParcels(const std::string& courierName, const std::vector<std::string>& parcelIds) {
        std::vector<std::string> collected;

        // 检查快递员是否存在
        auto courierIt = m_users.find(courierName);
        if (courierIt == m_users.end() || courierIt->second->getUserType() != UserType::COURIER) {
            return collected;
        }
        
        for (const auto& parcelId : parcelIds) {
            auto parcelIt = m_parcels.find(parcelId);
            if (parcelIt == m_parcels.end() || parcelIt->second->getCourierName() != courierName ||
                parcelIt->second->getStatus() != ParcelStatus::PENDING_COLLECTION) {
                continue;
            }
            if (m_adminTotalBalance >= parcelIt->second->getPrice() * 0.5) {
                m_adminTotalBalance -= parcelIt->second->getPrice() * 0.5;
                courierIt->second->recharge(parcelIt->second->getPrice() * 0.5);
                parcelIt->second->setStatus(ParcelStatus::PENDING_SIGN);
                collected.push_back(parcelId);
            }
        }
        
        if (!collected.empty()) {
            saveData();
        }
        
        return collected;
    }

    // 签收快递
    std::vector<std::string> signParcels(const std::string& userName, const std::vector<std::string>& parcelIds) {
        std::vector<std::string> signedList;
        
        for (const auto& parcelId : parcelIds) {
            auto parcelIt = m_parcels.find(parcelId);
            if (parcelIt == m_parcels.end() || parcelIt->second->getReceiverName() != userName ||
                parcelIt->second->getStatus() != ParcelStatus::PENDING_SIGN) {
                continue;
            }
            parcelIt->second->setStatus(ParcelStatus::SIGNED);
            parcelIt->second->setReceiveTime(time(nullptr));
            signedList.push_back(parcelId);
        }
        
        if (!signedList.empty()) {
            saveData();
        }
        
        return signedList;
    }


    // 查询快递
    std::vector<Parcel*> queryParcels(const std::string& parcelId, const std::string& senderName = "", const std::string& receiverName = "",
        const std::string& courierName = "", const ParcelStatus& status = ParcelStatus::OTHER, const time_t& startTime = 0, const time_t& endTime = 0
    ) {
        std::vector<Parcel*> result;
        for (auto& parcel : m_parcels) {
            if (!parcelId.empty() && parcel.first != parcelId) continue;
            if (!senderName.empty() && parcel.second->getSenderName() != senderName) continue;
            if (!receiverName.empty() && parcel.second->getReceiverName() != receiverName) continue;
            if (!courierName.empty() && parcel.second->getCourierName() != courierName) continue;
            if (status != ParcelStatus::OTHER && parcel.second->getStatus() != status) continue;
            if (startTime != 0 && parcel.second->getSendTime() < startTime) continue;
            if (endTime != 0 && parcel.second->getReceiveTime() > endTime) continue;
            result.push_back(parcel.second);
        }      
        return result;
    }

    bool rechargeUser(const std::string& username, double amount) {
        if (amount <= 0.0) return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_users.find(username);
        if (it == m_users.end()) return false;
        it->second->recharge(amount);
        saveData();
        return true;
    }

    double getUserBalance(const std::string& username) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_users.find(username);
        if (it == m_users.end()) return -1.0;
        if(it->second->getUserType() == UserType::ADMINISTRATOR) return m_adminTotalBalance;
        return it->second->getBalance();
    }

    bool changeUserPassword(const std::string& username, const std::string& oldPwd, const std::string& newPwd) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_users.find(username);
        if (it == m_users.end()) return false;
        bool changed = it->second->changePassword(oldPwd, newPwd);
        if (changed) saveData();
        return changed;
    }

    // 查询用户
    std::vector<User*> getUsers(const std::string& username = "", const UserType& userType = UserType::ADMINISTRATOR) {
        std::vector<User*> users;
        
        for (const auto& pair : m_users) {
            if (!username.empty() && pair.first != username) continue;
            if (userType != UserType::ADMINISTRATOR && pair.second->getUserType() != userType) continue;
            users.push_back(pair.second);
        }
        
        return users;
    }

    // 删除用户账户
    bool deleteUser(const std::string& targetUsername) {
        
        // 检查目标用户是否存在
        auto targetIt = m_users.find(targetUsername);
        if (targetIt == m_users.end()) {
            return false;
        }
        
        if (targetIt->second->getUserType() == UserType::ADMINISTRATOR) {
            return false; // 管理员不能删除
        }
        
        // 检查目标用户是否有未完成的快递
        for (const auto& parcel : m_parcels) {
            if ((parcel.second->getSenderName() == targetUsername || parcel.second->getReceiverName() == targetUsername) &&
                parcel.second->getStatus() != ParcelStatus::SIGNED) {
                return false; // 有未完成的快递，不能删除
            }
        }
        
        // 删除用户
        delete targetIt->second;
        m_users.erase(targetIt);
        
        // 保存数据
        saveData();
        return true;
    }

    // 删除快递
    bool deleteParcel(const std::string& parcelId) {
        auto parcelIt = m_parcels.find(parcelId);
        if (parcelIt == m_parcels.end()) {
            return false;
        }

        if (parcelIt->second->getStatus() != ParcelStatus::SIGNED) {
            return false; // 未签收的快递不能删除
        }
        
        m_parcels.erase(parcelIt);
        saveData();
        return true;
    }

};


// ================== 服务器主类 ==================
class Server {
private:
    socket_t m_listenSocket;
    LogisticsSystem m_system;
    bool m_running;
    bool m_autoAssignCourier;
    std::map<socket_t, User*> m_userMap;
    std::atomic<long long> m_requestIdCounter;
    long long m_currentRequestId;

public:
    Server(const std::string& userFile, const std::string& parcelFile, const std::string& configFile, bool autoAssignCourier)
        : m_listenSocket(INVALID_SOCKET), m_system(userFile, parcelFile, configFile, autoAssignCourier), m_running(false), m_autoAssignCourier(autoAssignCourier), m_requestIdCounter(0), m_currentRequestId(0) {}
    
    ~Server() {}

    void start(const std::string& listenIp, int port) {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            g_logger.error("WSAStartup failed");
            return;
        }
#endif
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

        if (bind(m_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            g_logger.error("Bind failed on port " + std::to_string(port));
            CLOSE_SOCKET(m_listenSocket);
            return;
        }
        if (listen(m_listenSocket, 5) == SOCKET_ERROR) {
            g_logger.error("Listen failed");
            CLOSE_SOCKET(m_listenSocket);
            return;
        }
        m_running = true;
        g_logger.info("Server started on port " + std::to_string(port));
        g_logger.info("Server entering main loop, waiting for client connections...");

        fd_set readfds;
        
        

        while (m_running) {
            FD_ZERO(&readfds);
            FD_SET(m_listenSocket, &readfds);
            socket_t maxFD = m_listenSocket;
            for (auto client : m_userMap) {
                if (client.first > maxFD) maxFD = client.first;
                FD_SET(client.first, &readfds);
            }
            maxFD++;
            int ret = select(maxFD, &readfds, nullptr, nullptr, nullptr);
            if (ret == SOCKET_ERROR) {
                g_logger.error("Select failed");
                continue;
            }
            if (ret == 0) {
                continue;
            }
            if (FD_ISSET(m_listenSocket, &readfds)) {
                sockaddr_in clientAddr;
                socklen_t clientLen = sizeof(clientAddr);
                socket_t clientSocket = accept(m_listenSocket, (sockaddr*)&clientAddr, &clientLen);
                if (clientSocket != INVALID_SOCKET) {
                    char clientIP[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
                    g_logger.info("Client connected from: " + std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port)));

                    u_long ulTrue = 1;
                    ioctlsocket(clientSocket, FIONBIO, &ulTrue);

                    m_userMap[clientSocket] = nullptr;
                }  
            } 

            for (auto it = m_userMap.begin(); it != m_userMap.end() && m_running; ) {
                socket_t sock = it->first;
                if (FD_ISSET(sock, &readfds)) {
                    char buffer[4096];
                    memset(buffer, 0, sizeof(buffer));
                    int recvLen = recv(sock, buffer, sizeof(buffer)-1, 0);
                    if (recvLen <= 0) {
                        g_logger.info("Client disconnected");
                        CLOSE_SOCKET(sock);
                        it = m_userMap.erase(it);
                        continue;
                    }
                    std::string request(buffer, recvLen);
                    g_logger.debug("Raw request received: " + request);
                    size_t pos = request.find('\n');
                    if (pos == std::string::npos) {
                        g_logger.warning("Malformed request without newline from client socket");
                        ++it;
                        continue;
                    }
                    std::string cmd;
                    std::vector<std::string> args;
                    Protocol::parseRequest(request.substr(0, pos+1), cmd, args);
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
                        response = processCommand(m_userMap[sock], cmd, args);
                    } catch (const std::exception& ex) {
                        g_logger.error("req=" + std::to_string(reqId) + " Unhandled exception processing command: " + std::string(ex.what()));
                        response = buildResponse("ERROR", {"Internal server error"});
                    } catch (...) {
                        g_logger.error("req=" + std::to_string(reqId) + " Unhandled unknown exception processing command");
                        response = buildResponse("ERROR", {"Internal server error"});
                    }

                    send(sock, response.c_str(), static_cast<int>(response.size()), 0);
                }
                ++it;
            }    
        }

    }

    void stop() {
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

private:

    std::string processCommand(User*& m_currentUser, const std::string& cmd, const std::vector<std::string>& args) {
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
                    if (m_currentUser->getUserType() != UserType::ADMINISTRATOR) {
                        g_logger.warning("QueryUser command: Permission denied for user: " + 
                           (m_currentUser ? m_currentUser->getUsername() : "anonymous"));
                        return buildResponse("ERROR", {"Only administrators can query users"});
                    }
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
                } else {
                    return buildResponse("ERROR", {"Unknown command"});
                }
            }
        }
    }


    std::string handleRegister(const std::vector<std::string>& args) {
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
        
        if (m_system.registerUser(username, password, name, phone, address, type)) {
            g_logger.info("User registered successfully: " + username + " as " + (type == UserType::CUSTOMER ? "Customer" : "Courier"));
            return buildResponse("OK", {"Registration successful"});
        } else {
            g_logger.warning("User registration failed - username already exists: " + username);
            return buildResponse("ERROR", {"Username already exists"});
        }
    }

    std::string buildResponse(const std::string& status, const std::vector<std::string>& data) {
        std::string resp = Command::RESPONSE + std::string(1, DELIMITER) + status;
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

    std::string handleLogout(User*& m_currentUser, const std::vector<std::string>& args) {
        if (args.size() != 0) {
            g_logger.warning("Logout command: Invalid arguments count: " + std::to_string(args.size()));
            return buildResponse("ERROR", {"Invalid arguments"});
        }
        m_currentUser = nullptr;
        g_logger.info("User logged out successfully");
        return buildResponse("OK", {"Logout successful"});
    }

    std::string handleLogin(User* &m_currentUser, const std::vector<std::string>& args) {
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
        
        User* user = m_system.loginUser(username, password, type);
        if (user) {
            m_currentUser = user;
            g_logger.info("User logged in successfully: " + m_currentUser->getUsername());
            return buildResponse("OK", {m_currentUser->getUsername(), std::to_string(typeInt)});
        } else {
            g_logger.warning("Login failed for user: " + username);
            return buildResponse("ERROR", {"Login failed"});
        }
    }

    std::string handleSendParcel(const User* m_currentUser, const std::vector<std::string>& args) {
        if (args.size() < 4) {
            g_logger.warning("SendParcel command: Invalid arguments count: " + std::to_string(args.size()));
            return buildResponse("ERROR", {"Invalid arguments"});
        }
        std::string receiver = args[0];
        int ptype = std::stoi(args[1]);
        double weight = std::stod(args[2]);
        std::string desc = args[3];
        ParcelType type = static_cast<ParcelType>(ptype);
        
        g_logger.info("SendParcel attempt - Sender: " + m_currentUser->getUsername() + 
                     ", Receiver: " + receiver + ", Type: " + std::to_string(ptype) + 
                     ", Weight: " + std::to_string(weight));
        
        std::string pid = m_system.sendParcel(m_currentUser->getUsername(), receiver, type, weight, desc);
        if (!pid.empty()) {
            g_logger.info("Parcel sent successfully - ID: " + pid + ", Sender: " + m_currentUser->getUsername());
            return buildResponse("OK", {pid});
        } else {
            g_logger.warning("SendParcel failed - Sender: " + m_currentUser->getUsername() + 
                           ", Receiver: " + receiver + " (receiver not found or insufficient balance)");
            return buildResponse("ERROR", {"Failed to send parcel (receiver not found or insufficient balance)"});
        }
    }

    std::string handleAssignParcel(const User* m_currentUser, const std::vector<std::string>& args) {
        if (args.size() < 2) {
            g_logger.warning("AssignParcel command: Invalid arguments count: " + std::to_string(args.size()));
            return buildResponse("ERROR", {"Invalid arguments"});
        }
        std::string parcelId = args[0];
        std::string courier = args[1];
        
        g_logger.info("AssignParcel attempt - Admin: " + m_currentUser->getUsername() + 
                     ", Parcel: " + parcelId + ", Courier: " + courier);
        
        if (m_system.assignCourier(parcelId, courier)) {
            g_logger.info("Parcel assigned successfully - Parcel: " + parcelId + ", Courier: " + courier);
            return buildResponse("OK", {"Assigned"});
        } else {
            g_logger.warning("Parcel assignment failed - Parcel: " + parcelId + ", Courier: " + courier);
            return buildResponse("ERROR", {"Assignment failed"});
        }
    }

    std::string handleCollectParcel(const User* m_currentUser, const std::vector<std::string>& args) {
        if (args.empty()) {
            g_logger.warning("CollectParcel command: No parcel IDs provided");
            return buildResponse("ERROR", {"No parcel IDs"});
        }
        std::vector<std::string> ids = args;
        
        g_logger.info("CollectParcel attempt - Courier: " + m_currentUser->getUsername() + 
                     ", Parcel IDs count: " + std::to_string(ids.size()));
        
        std::vector<std::string> collected = m_system.collectParcels(m_currentUser->getUsername(), ids);
        if (!collected.empty()) {
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
        return buildResponse("ERROR", {"No parcels collected"});
    }

    std::string handleSignParcel(const User* m_currentUser, const std::vector<std::string>& args) {
        if (args.empty()) {
            g_logger.warning("SignParcel command: No parcel IDs provided");
            return buildResponse("ERROR", {"No parcel IDs"});
        }
        std::vector<std::string> ids = args;
        
        g_logger.info("SignParcel attempt - User: " + m_currentUser->getUsername() + 
                     ", Parcel IDs count: " + std::to_string(ids.size()));
        
        std::vector<std::string> signedList = m_system.signParcels(m_currentUser->getUsername(), ids);
        if (!signedList.empty()) {
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
        return buildResponse("ERROR", {"No parcels signed"});
    }

    std::string handleQueryParcel(const User* m_currentUser, const std::vector<std::string>& args) {
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
            parcels = m_system.queryParcels(parcelId, sender, receiver, "", status, startTime, endTime);
        } else if (m_currentUser->getUserType() == UserType::COURIER) {
            parcels = m_system.queryParcels(parcelId, sender, receiver, m_currentUser->getUsername(), status, startTime, endTime);
        } else {
            if (sender.empty() && receiver.empty()) {
                parcels = m_system.queryParcels(parcelId, m_currentUser->getUsername(), "", "", status, startTime, endTime);
                std::vector<Parcel*> parcels2 = m_system.queryParcels(parcelId, "", m_currentUser->getUsername(), "", status, startTime, endTime);
                parcels.insert(parcels.end(), parcels2.begin(), parcels2.end());
            } else {
                if (sender.empty()) sender = m_currentUser->getUsername();
                if (receiver.empty()) receiver = m_currentUser->getUsername();
                parcels = m_system.queryParcels(parcelId, sender, receiver, "", status, startTime, endTime);
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

    std::string handleRechargeBalance(const User* m_currentUser, const std::vector<std::string>& args) {
        if (args.size() < 1) {
            g_logger.warning("RechargeBalance command: Invalid arguments count: " + std::to_string(args.size()));
            return buildResponse("ERROR", {"Invalid arguments"});
        }

        double amount = std::stod(args[0]);
        g_logger.info("RechargeBalance attempt - User: " + m_currentUser->getUsername() + ", Amount: " + std::to_string(amount));
        if (amount <= 0.0) {
            g_logger.warning("RechargeBalance command: Invalid amount: " + args[0]);
            return buildResponse("ERROR", {"Invalid amount"});
        }
        if (m_system.rechargeUser(m_currentUser->getUsername(), amount)) {
            g_logger.info("User recharge successful - User: " + m_currentUser->getUsername() + ", Amount: " + std::to_string(amount));
            return buildResponse("OK", {"Recharge successful"});
        }
        g_logger.warning("RechargeBalance failed - User: " + m_currentUser->getUsername());
        return buildResponse("ERROR", {"Recharge failed"});
    }

    std::string handleQueryBalance(const User* m_currentUser, const std::vector<std::string>& args) {
        double balance = m_system.getUserBalance(m_currentUser->getUsername());
        if (balance < 0.0) {
            g_logger.warning("QueryBalance failed - User not found: " + m_currentUser->getUsername());
            return buildResponse("ERROR", {"User not found"});
        }
        g_logger.info("QueryBalance request - User: " + m_currentUser->getUsername() + ", Balance: " + std::to_string(balance));
        return buildResponse("OK", {std::to_string(balance)});
    }

    std::string handleChangePassword(const User* m_currentUser, const std::vector<std::string>& args) {
        if (args.size() < 2) {
            g_logger.warning("ChangePassword command: Invalid arguments count: " + std::to_string(args.size()));
            return buildResponse("ERROR", {"Invalid arguments"});
        }
        std::string oldPwd = args[0];
        std::string newPwd = args[1];
        g_logger.info("ChangePassword attempt - User: " + m_currentUser->getUsername() + ", password change requested");

        if (m_system.changeUserPassword(m_currentUser->getUsername(), oldPwd, newPwd)) {
            g_logger.info("Password changed successfully for user: " + m_currentUser->getUsername());
            return buildResponse("OK", {"Password changed"});
        }
        g_logger.warning("ChangePassword failed for user: " + m_currentUser->getUsername());
        return buildResponse("ERROR", {"Password change failed"});
    }

    std::string handleDeleteAccount(const User* m_currentUser, const std::vector<std::string>& args) {
        if (args.size() < 1) {
            g_logger.warning("DeleteAccount command: Invalid arguments count: " + std::to_string(args.size()));
            return buildResponse("ERROR", {"Invalid arguments"});
        }
        
        std::string targetUsername = args[0];
        
        g_logger.info("DeleteAccount attempt - Admin: " + m_currentUser->getUsername() + 
                     ", Target: " + targetUsername);
        
        if (m_system.deleteUser(targetUsername)) {
            g_logger.info("Account deleted successfully - Admin: " + m_currentUser->getUsername() + 
                         ", Target: " + targetUsername);
            return buildResponse("OK", {"Account deleted successfully"});
        } else {
            g_logger.warning("DeleteAccount failed - Admin: " + m_currentUser->getUsername() + 
                           ", Target: " + targetUsername + " (user not found, has unfinished parcels, or self-deletion attempt)");
            return buildResponse("ERROR", {"Delete failed: user not found, has unfinished parcels, or cannot delete self"});
        }
    }

    std::string handleQueryUser(const User* m_currentUser, const std::vector<std::string>& args) {    
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

        g_logger.info("QueryUser request - Admin: " + m_currentUser->getUsername() + ", User: " + username + ", Type: " + std::to_string(typeInt));
        
        std::vector<User*> users = m_system.getUsers(username, userType);
        std::string data;
        for (size_t i = 0; i < users.size(); ++i) {
            if (!data.empty()) data += DELIMITER;
            data += users[i]->serialize();
        }
        
        g_logger.debug("QueryUser result - Admin: " + m_currentUser->getUsername() + 
                      ", User count: " + std::to_string(users.size()));
        
        return buildResponse("OK", {data});
    }

    std::string handleDeleteParcel(const User* m_currentUser, const std::vector<std::string>& args) {
        if (args.size() < 1) {
            g_logger.warning("DeleteParcel command: Invalid arguments count: " + std::to_string(args.size()));
            return buildResponse("ERROR", {"Invalid arguments"});
        }
        
        std::string parcelId = args[0];
        if (m_system.deleteParcel(parcelId)) {
            g_logger.info("Parcel deleted successfully - Parcel ID: " + parcelId);
            return buildResponse("OK", {"Parcel deleted successfully"});
        } else {
            g_logger.warning("DeleteParcel failed - Parcel ID: " + parcelId);
            return buildResponse("ERROR", {"Delete failed: parcel not found"});
        }
    }
};

Server *server;

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT) {
        g_logger.info("Ctrl+C or Ctrl+BREAK pressed, shutting down server...");
        server->stop();
        return TRUE;
    }
    return FALSE;
}

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