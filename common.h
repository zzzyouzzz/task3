// common.h
#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <ctime>
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <fstream>
#include <iostream>

enum class ParcelStatus {
    PENDING_COLLECTION = 0,
    PENDING_SIGN,
    SIGNED,
    OTHER
};

enum class UserType {
    CUSTOMER = 0,
    COURIER,
    ADMINISTRATOR
};


enum class ParcelType {
    NORMAL = 0,
    FRAGILE,
    BOOK
};

namespace Command {
    const std::string LOGIN          = "LOGIN";
    const std::string LOGOUT          = "LOGOUT";
    const std::string REGISTER       = "REGISTER";   
    const std::string SEND_PARCEL       = "SEND_PARCEL";
    const std::string ASSIGN_PARCEL     = "ASSIGN_PARCEL";
    const std::string COLLECT_PARCEL    = "COLLECT_PARCEL";
    const std::string SIGN_PARCEL       = "SIGN_PARCEL";
    const std::string QUERY_PARCEL      = "QUERY_PARCEL";
    const std::string QUERY_USER        = "QUERY_USER";
    const std::string RECHARGE_BALANCE  = "RECHARGE_BALANCE";
    const std::string QUERY_BALANCE     = "QUERY_BALANCE";
    const std::string CHANGE_PASSWORD   = "CHANGE_PASSWORD";
    const std::string DELETE_ACCOUNT    = "DELETE_ACCOUNT";   
    const std::string DELETE_PARCEL     = "DELETE_PARCEL";
    const std::string RESPONSE          = "RESPONSE";
}

const char DELIMITER = '|';

class Protocol {
public:
    static std::string buildRequest(const std::string& cmd, const std::vector<std::string>& args) {
        std::string msg = cmd;
        for (const auto& a : args) {
            msg += DELIMITER + a;
        }
        msg += '\n';
        return msg;
    }

    static void parseRequest(const std::string& raw, std::string& cmd, std::vector<std::string>& args) {
        cmd.clear();
        args.clear();
        if (raw.empty()) return;
        size_t pos = 0;
        std::string msg = raw;
        if (!msg.empty() && msg.back() == '\n') msg.pop_back();
        while (pos < msg.size()) {
            size_t next = msg.find(DELIMITER, pos);
            if (next == std::string::npos) {//没有找到分隔符
                if (cmd.empty()) cmd = msg.substr(pos);
                else args.push_back(msg.substr(pos));
                break; 
            } else {
                if (cmd.empty()) cmd = msg.substr(pos, next - pos);
                else args.push_back(msg.substr(pos, next - pos));
                pos = next + 1;
            }
        }
    }
};

// ================== 日志系统 ==================
enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL
};

static LogLevel parseLogLevel(const std::string& levelName) {
    std::string name = levelName;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::toupper(c); });
    if (name == "DEBUG") return LOG_DEBUG;
    if (name == "INFO") return LOG_INFO;
    if (name == "WARNING" || name == "WARN") return LOG_WARNING;
    if (name == "ERROR") return LOG_ERROR;
    if (name == "FATAL") return LOG_FATAL;
    return LOG_INFO;
}

class Logger {
private:
    std::ofstream m_logFile;
    LogLevel m_minLevel;
    bool m_consoleOutput;
    std::mutex m_mutex;
    
    std::string getCurrentTime() {
        std::time_t now = std::time(nullptr);
        std::tm localTimeStorage;
    #ifdef _WIN32
        localtime_s(&localTimeStorage, &now);
    #else
        localtime_r(&now, &localTimeStorage);
    #endif
        std::ostringstream oss;
        oss << std::put_time(&localTimeStorage, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
    
    std::string levelToString(LogLevel level) {
        switch (level) {
            case LOG_DEBUG: return "DEBUG";
            case LOG_INFO: return "INFO";
            case LOG_WARNING: return "WARNING";
            case LOG_ERROR: return "ERROR";
            case LOG_FATAL: return "FATAL";
            default: return "UNKNOWN";
        }
    }
    
public:
    Logger() : m_minLevel(LOG_INFO), m_consoleOutput(true) {}
    
    ~Logger() {
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }
    
    bool initialize(const std::string& filename = "server.log", LogLevel minLevel = LOG_INFO, bool consoleOutput = true) {
        m_logFile.open(filename, std::ios::app);
        if (!m_logFile.is_open()) {
            return false;
        }
        
        m_minLevel = minLevel;
        m_consoleOutput = consoleOutput;
        
        log(LOG_INFO, "Logger initialized successfully");
        return true;
    }
    
    void log(LogLevel level, const std::string& message) {
        if (level < m_minLevel) return;
        
        std::ostringstream oss;
        oss << "[" << getCurrentTime() << "] [" << levelToString(level) << "] " << message;
        std::string logEntry = oss.str();
        
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logFile.is_open()) {
            m_logFile << logEntry << std::endl;
            m_logFile.flush();
        }
        if (m_consoleOutput) {
            std::cout << logEntry << std::endl;
        }
    }
    
    void debug(const std::string& message) { log(LOG_DEBUG, message); }
    void info(const std::string& message) { log(LOG_INFO, message); }
    void warning(const std::string& message) { log(LOG_WARNING, message); }
    void error(const std::string& message) { log(LOG_ERROR, message); }
    void fatal(const std::string& message) { log(LOG_FATAL, message); }
};


// ================== 用户基类及派生类 ==================
class User {
protected:
    std::string m_username;   // 用户名（唯一）
    std::string m_password;   // 密码
    std::string m_name;       // 真实姓名
    std::string m_phone;      // 电话
    std::string m_address;    // 地址
    double m_balance;         // 余额

public:
    User() = default;

    User(const std::string& uname, const std::string& pwd, const std::string& name,
         const std::string& phone, const std::string& addr, double balance = 0.0)
        : m_username(uname), m_password(pwd), m_name(name), m_phone(phone),
          m_address(addr), m_balance(balance) {}

    virtual ~User() = default;

    bool login(const std::string& password) const { return m_password == password; }
    bool changePassword(const std::string& oldPwd, const std::string& newPwd) {
        if (m_password != oldPwd) return false;
        m_password = newPwd;
        return true;
    }
    double getBalance() const { return m_balance; }
    void recharge(double amount) { m_balance += amount; }
    bool deduct(double amount) {
        if (m_balance < amount) return false;
        m_balance -= amount;
        return true;
    }
    void addBalance(double amount) { m_balance += amount; }

    std::string getUsername() const { return m_username; }
    std::string getName() const { return m_name; }
    std::string getPhone() const { return m_phone; }
    std::string getAddress() const { return m_address; }

    virtual UserType getUserType() const { return UserType::CUSTOMER; }

    virtual std::string serialize() const {
        std::ostringstream oss;
        oss << static_cast<int>(getUserType()) << DELIMITER
            << m_username << DELIMITER << m_password << DELIMITER
            << m_name << DELIMITER << m_phone << DELIMITER
            << m_address << DELIMITER << m_balance;
        return oss.str();
    }

};

class Customer : public User {
public:
    Customer() = default;
    Customer(const std::string& uname, const std::string& pwd, const std::string& name,
             const std::string& phone, const std::string& addr, double balance = 0.0)
        : User(uname, pwd, name, phone, addr, balance) {}
    UserType getUserType() const override { return UserType::CUSTOMER; }
};

class Courier : public User {
public:
    Courier() = default;
    Courier(const std::string& uname, const std::string& pwd, const std::string& name,
            const std::string& phone, const std::string& addr, double balance = 0.0)
        : User(uname, pwd, name, phone, addr, balance) {}
    UserType getUserType() const override { return UserType::COURIER; }
};

class Administrator : public User {
public:
    Administrator() = default;
    Administrator(const std::string& uname, const std::string& pwd, const std::string& name,
                  const std::string& phone, const std::string& addr, double balance = 0.0)
        : User(uname, pwd, name, phone, addr, balance) {}
    UserType getUserType() const override { return UserType::ADMINISTRATOR; }
};

// ================== 快递类体系 ==================
class Parcel {
protected:
    std::string m_parcelId;       // 快递单号
    std::string m_senderName;     // 寄件人用户名
    std::string m_receiverName;   // 收件人用户名
    time_t m_sendTime;            // 寄送时间
    time_t m_receiveTime;         // 签收时间
    ParcelStatus m_status;        // 快递状态
    std::string m_description;    // 物品描述
    double m_weight;              // 重量/数量
    std::string m_courierName;    // 分配的快递员用户名
    ParcelType m_type;

public:
    Parcel() = default;
    Parcel(ParcelType type, const std::string& id, const std::string& sender, const std::string& receiver,
           time_t sendTime, time_t receiveTime, ParcelStatus status, const std::string& desc, double weight,    
           const std::string& courierName)
        : m_parcelId(id), m_senderName(sender), m_receiverName(receiver),
          m_sendTime(sendTime), m_receiveTime(receiveTime),
          m_status(status), m_description(desc),
          m_weight(weight), m_courierName(courierName), m_type(type) {}
    Parcel(const std::string& id, const std::string& sender, const std::string& receiver,
           double weight, const std::string& desc, ParcelType type)
        : m_parcelId(id), m_senderName(sender), m_receiverName(receiver),
          m_sendTime(time(nullptr)), m_receiveTime(0),
          m_status(ParcelStatus::PENDING_COLLECTION), m_description(desc),
          m_weight(weight), m_courierName(""), m_type(type) {}

    virtual ~Parcel() = default;

    virtual double getPrice() const { return 0.0; };   // 纯虚函数，计算快递费

    std::string getParcelId() const { return m_parcelId; }
    std::string getSenderName() const { return m_senderName; }
    std::string getReceiverName() const { return m_receiverName; }
    ParcelStatus getStatus() const { return m_status; }
    std::string getDescription() const { return m_description; }
    double getWeight() const { return m_weight; }
    std::string getCourierName() const { return m_courierName; }
    ParcelType getParcelType() const { return m_type; }
    time_t getSendTime() const { return m_sendTime; }
    time_t getReceiveTime() const { return m_receiveTime; }

    void setCourier(const std::string& name) { m_courierName = name; }
    void setStatus(ParcelStatus s) { m_status = s; }
    void setReceiveTime(time_t t) { m_receiveTime = t; }

    virtual std::string serialize() const {
        std::ostringstream oss;
        oss << static_cast<int>(m_type) << DELIMITER
            << m_parcelId << DELIMITER << m_senderName << DELIMITER
            << m_receiverName << DELIMITER << m_sendTime << DELIMITER
            << m_receiveTime << DELIMITER << static_cast<int>(m_status) << DELIMITER
            << m_description << DELIMITER << m_weight << DELIMITER
            << m_courierName;
        return oss.str();
    }

};

class NormalParcel : public Parcel {
public:
    NormalParcel() = default;
    NormalParcel(const std::string& id, const std::string& sender, const std::string& receiver,
                 double weight, const std::string& desc)
        : Parcel(id, sender, receiver, weight, desc, ParcelType::NORMAL) {}
    double getPrice() const override { return 5.0 * getWeight(); }
};

class FragileParcel : public Parcel {
public:
    FragileParcel() = default;
    FragileParcel(const std::string& id, const std::string& sender, const std::string& receiver,
                  double weight, const std::string& desc)
        : Parcel(id, sender, receiver, weight, desc, ParcelType::FRAGILE) {}
    double getPrice() const override { return 8.0 * getWeight(); }
};

class BookParcel : public Parcel {
public:
    BookParcel() = default;
    BookParcel(const std::string& id, const std::string& sender, const std::string& receiver,
               double weight, const std::string& desc)
        : Parcel(id, sender, receiver, weight, desc, ParcelType::BOOK) {}
    double getPrice() const override { return 2.0 * getWeight(); }  // weight 在此表示数量
};

static std::string trimString(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) start++;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) end--;
    return value.substr(start, end - start);
}

#endif