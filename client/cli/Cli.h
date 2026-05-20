#pragma once
#include <iostream>
#include <sstream>
#include <cfloat>   
#include <ctime>
#include <string>
#include "Communication.h"

// ================== 命令行客户端界面 ==================
class LogisticsClient {
private:
    Communication m_system;       // 通信对象
    bool is_running = true;       // 运行标志

    // 读取整数（带范围校验）
    bool readInt(const std::string& prompt, int& value, int minValue = INT_MIN, int maxValue = INT_MAX);
    // 读取浮点数
    bool readDouble(const std::string& prompt, double& value, double minValue = -DBL_MAX, double maxValue = DBL_MAX);  
    // 读取字符串
    bool readString(const std::string& prompt, std::string& value);
    // 读取时间（YYYY-MM-DD HH:MM:SS 格式）
    bool readTime(const std::string& prompt, time_t& value);

public:
    LogisticsClient(const std::string& ip, const int port);  // 连接服务器
    void run();                      // 主循环

private:
    void loginUI();                  // 登录界面
    void registerUI();               // 注册界面
    void mainMenu(UserType type);    // 功能菜单
    void sendParcelUI();             // 寄件
    void signParcelUI();             // 签收
    void queryParcelUI(const UserType& queryer = UserType::ADMINISTRATOR); // 查询快递
    void rechargeBalanceUI();        // 充值
    void queryBalanceUI();           // 查余额
    void changePasswordUI();         // 改密码
    void assignParcelUI();           // 分配快递员
    void collectParcelUI();          // 揽收
    void queryUsersUI();             // 查询用户
    void deleteAccountUI();          // 注销账户
    void deleteParcelUI();           // 删除快递
    void logoutUI();                 // 注销登录
    void getStatisticsUI();          // 获取统计信息
};