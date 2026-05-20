#pragma once
#include <string>
#include <sstream>
#include "common.h"

enum class UserType {
    CUSTOMER = 0,
    COURIER,
    ADMINISTRATOR
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
    bool m_isLogin;            // 是否登录

public:
    User() = default;

    User(const std::string& uname, const std::string& pwd, const std::string& name,
         const std::string& phone, const std::string& addr, double balance = 0.0)
        : m_username(uname), m_password(pwd), m_name(name), m_phone(phone),
          m_address(addr), m_balance(balance), m_isLogin(false) {}

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
    bool isLogin() const { return m_isLogin; }
    void logout() { m_isLogin = false; }
    void login() { m_isLogin = true; }

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