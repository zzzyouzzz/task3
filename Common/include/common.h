#pragma once
#include <string>
#include <fstream>
#include <algorithm>
#include <vector>

// ================== 协议命令常量 ==================
namespace Command {
    const std::string LOGIN          = "LOGIN";
    const std::string LOGOUT         = "LOGOUT";
    const std::string REGISTER       = "REGISTER";   
    const std::string SEND_PARCEL    = "SEND_PARCEL";
    const std::string ASSIGN_PARCEL  = "ASSIGN_PARCEL";
    const std::string COLLECT_PARCEL = "COLLECT_PARCEL";
    const std::string SIGN_PARCEL    = "SIGN_PARCEL";
    const std::string QUERY_PARCEL   = "QUERY_PARCEL";
    const std::string QUERY_USER     = "QUERY_USER";
    const std::string RECHARGE_BALANCE = "RECHARGE_BALANCE";
    const std::string QUERY_BALANCE  = "QUERY_BALANCE";
    const std::string CHANGE_PASSWORD = "CHANGE_PASSWORD";
    const std::string DELETE_ACCOUNT = "DELETE_ACCOUNT";   
    const std::string DELETE_PARCEL  = "DELETE_PARCEL";
    const std::string GET_STATISTICS = "GET_STATISTICS";
    const std::string RESPONSE       = "RESPONSE";
}

// ================== 报文分隔符 ==================
const char DELIMITER = '|';

// ================== 错误码枚举（全层通用） ==================
enum class ErrorCode {
    SUCCESS              = 0,   // 操作成功
    INVALID_ARGS         = 1,   // 参数数量或格式错误
    PERMISSION_DENIED    = 2,   // 权限不足
    NOT_LOGGED_IN        = 3,   // 未登录
    USER_NOT_FOUND       = 4,   // 用户不存在
    USER_EXISTS          = 5,   // 用户名已存在
    INSUFFICIENT_BALANCE = 6,   // 余额不足
    PARCEL_NOT_FOUND     = 7,   // 快递单号不存在
    INVALID_STATUS       = 8,   // 快递状态不允许操作
    LOGIN_FAILED         = 9,   // 密码错误或身份不匹配
    INVALID_AMOUNT       = 10,  // 充值/扣款金额不合法
    COURIER_BUSY         = 11,  // 快递员忙碌
    NO_RESULT            = 12,  // 查询结果为空或无数据可操作
    DELETE_BLOCKED       = 13,  // 删除被阻塞（有未完成业务或权限）
    UNKNOWN              = 99   // 未知错误
};

// 去除两侧空白字符
static std::string trimString(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) start++;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) end--;
    return value.substr(start, end - start);
}

// 安全解析字符串为 int，失败返回 false
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

// 安全解析字符串为 long long，失败返回 false
static bool parseLongLong(const std::string& str, long long& value) {
    try {
        size_t idx = 0;
        value = std::stoll(str, &idx);
        return idx == str.size();
    } catch (...) {
        return false;
    }
}

// 安全解析字符串为 double，失败返回 false
static bool parseDouble(const std::string& str, double& value) {
    try {
        size_t idx = 0;
        value = std::stod(str, &idx);
        return idx == str.size();
    } catch (...) {
        return false;
    }
}

// ================== 通信协议编解码 ==================
class Protocol {
public:
    // 构造请求报文：命令|参数1|参数2|...\n
    static std::string buildRequest(const std::string& cmd, const std::vector<std::string>& args);
    // 解析请求报文
    static void parseRequest(const std::string& raw, std::string& cmd, std::vector<std::string>& args);
};