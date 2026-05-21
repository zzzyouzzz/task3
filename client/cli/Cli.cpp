#include "Cli.h"

// 读取整数输入：循环提示直至输入合法范围内的整数值
bool LogisticsClient::readInt(const std::string& prompt, int& value, int minValue, int maxValue) const {
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

// 读取浮点数输入：循环提示直至输入合法范围内的浮点数值
bool LogisticsClient::readDouble(const std::string& prompt, double& value, double minValue, double maxValue) const {
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

// 读取字符串输入：循环提示直至输入非空字符串
bool LogisticsClient::readString(const std::string& prompt, std::string& value) const {
    while (true) {
        std::cout << prompt;
        if (!std::getline(std::cin, value)) return false;
        value = trimString(value);
        if (value.empty()) {
            std::cout << "输入不能为空。\n";
            continue;
        }
        return true;
    }
}
// 读取时间输入：格式 YYYY-MM-DD HH:MM:SS，可选输入
bool LogisticsClient::readTime(const std::string& prompt, time_t& value) const {
    std::cout << prompt;
    std::cout << "(格式:YYYY-MM-DD HH:MM:SS)";
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    if (line.empty()) return true;
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    int year, month, day, hour, minute, second;

    try {
        if (sscanf(line.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) return false;
        if (year < 1900 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return false;
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = -1;
        value = mktime(&tm);
        return value != -1;
    } catch (...) {
        return false;
    }
}

LogisticsClient::LogisticsClient(const std::string& ip, const int port) : m_ip(ip), m_port(port) {
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

// 重新连接服务器
bool LogisticsClient::reconnect() {
    if (!m_system.connectToServer(m_ip, m_port)) {
        is_running = false;
        g_logger.error("Unable to reconnect to server at " + m_ip + ":" + std::to_string(m_port));
        std::cerr << "重新连接服务器失败。" << std::endl;
        return false;
    }
    is_running = true;
    g_logger.info("Client reconnected to server at " + m_ip + ":" + std::to_string(m_port));
    std::cout << "重新连接服务器成功！" << std::endl;
    std::string name;
    ErrorCode ec = m_system.loginUser(username, password, type, name);
    if (ec != ErrorCode::SUCCESS) {
        std::cout << "登录失败" << std::endl;
        return false;
    }
    return true;
}



void LogisticsClient::run() {
    
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

void LogisticsClient::loginUI() {
    int userType;
    
    if (!readString("用户名: ", username)) return;
    if (!readString("密码: ", password)) return;
    if (!readInt("用户类型 (0-客户, 1-快递员, 2-管理员): ", userType, 0, 2) || userType < 0 || userType > 2) return;
    
    type = static_cast<UserType>(userType);
    std::string name;
    ErrorCode ec = m_system.loginUser(username, password, type, name);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "登录成功！欢迎 " << name << "！" << std::endl;
            mainMenu(type);
            break;
        case ErrorCode::USER_NOT_FOUND:
            std::cout << "登录失败：用户 " << username << " 不存在。" << std::endl;
            break;
        case ErrorCode::LOGIN_FAILED:
            std::cout << "登录失败：密码错误或身份不匹配。" << std::endl;
            break;
        case ErrorCode::USER_ALREADY_LOGIN:
            std::cout << "登录失败：用户已登录。" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "登录失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "登录失败，请重试。" << std::endl;
            break;
    }
}

void LogisticsClient::registerUI() {
    std::string username, password, name, phone, address;
    int userType;
    
    if (!readString("用户名: ", username) || username.empty()) return;
    if (!readString("密码: ", password) || password.empty()) return;
    if (!readString("姓名: ", name) || name.empty()) return;
    if (!readString("电话: ", phone) || phone.empty()) return;
    if (!readString("地址: ", address) || address.empty()) return;
    if (!readInt("用户类型 (0-客户, 1-快递员): ", userType, 0, 1) || userType < 0 || userType > 1) return;
    
    UserType type = static_cast<UserType>(userType);
    
    ErrorCode ec = m_system.registerUser(username, password, name, phone, address, type);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "注册成功！" << std::endl;
            break;
        case ErrorCode::USER_EXISTS:
            std::cout << "注册失败：用户名 " << username << " 已被注册。" << std::endl;
            break;
        case ErrorCode::INVALID_ARGS:
            std::cout << "注册失败：不允许注册管理员账号。" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "注册失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "注册失败，请重试。" << std::endl;
            break;
    }
}

void LogisticsClient::mainMenu(UserType type) {
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
        std::cout << "12. 获取统计信息" << std::endl;
        std::cout << "0. 注销登录" << std::endl;
        
        int op;
        if (!readInt("请选择: ", op, 0, 12)) {
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
                queryParcelUI(type);    
                break;
            case 4: 
                if (type != UserType::CUSTOMER) {
                    std::cout << "客户才能充值余额。" << std::endl;
                } else {
                    rechargeBalanceUI();
                }
                break;
            case 5: queryBalanceUI(); break;
            case 6: changePasswordUI(); break;
            case 7: 
                if (type == UserType::ADMINISTRATOR) {
                    assignParcelUI();
                } else {
                    std::cout << "您没有权限分配快递员。" << std::endl;
                }
                break;
            case 8: 
                if (type == UserType::COURIER) {
                    collectParcelUI();
                } else {
                    std::cout << "您没有权限揽收快递。" << std::endl;
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
            case 12: 
                if (type == UserType::ADMINISTRATOR) {
                    getStatisticsUI();
                } else {
                    std::cout << "您没有权限获取统计信息。" << std::endl;
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
void LogisticsClient::sendParcelUI() {
    std::string receiver, desc;
    int ptype;
    double weight;
    
    if (!readString("收件人用户名: ", receiver)) return;
    if (!readInt("快递类型 (0-普通, 1-易碎, 2-书籍): ", ptype, 0, 2)) return;
    if (!readDouble("重量(kg): ", weight, 0.1, 1000.0)) return;
    if (!readString("物品描述: ", desc)) return;
    
    ParcelType type = static_cast<ParcelType>(ptype);
    std::string pid;

    ErrorCode ec = m_system.sendParcel(receiver, type, weight, desc, pid);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "快递发送成功！单号: " << pid << std::endl;
            break;
        case ErrorCode::USER_NOT_FOUND:
            std::cout << "发送失败：收件人 " << receiver << " 不存在。" << std::endl;
            break;
        case ErrorCode::INSUFFICIENT_BALANCE:
            std::cout << "发送失败：余额不足，请先充值。" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "发送失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "发送失败。" << std::endl;
            break;
    }
    
}

void LogisticsClient::signParcelUI() {
    std::cout << "请输入要签收的快递单号(多个用逗号分隔): ";
    std::string line; 
    std::getline(std::cin, line);
    
    std::vector<std::string> ids;
    std::istringstream iss(line);
    std::string token;
    while (std::getline(iss, token, ',')) {
        token = trimString(token);
        if (!token.empty()) {
            ids.push_back(token);
        }
    }
    
    std::vector<std::string> signedList;
    
    ErrorCode ec = m_system.signParcels(ids, signedList);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "签收成功: ";
            for (size_t i = 0; i < signedList.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << signedList[i];
            }
            std::cout << std::endl;
            break;
        case ErrorCode::NO_RESULT:
            std::cout << "签收失败：没有可签收的快递（可能已签收或不属于你）。" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "签收失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "签收失败。" << std::endl;
            break;
    }
    
}

void LogisticsClient::queryParcelUI(const UserType& queryer) {
    std::vector<Parcel> parcels;
    std::string parcelId;
    std::string sender;
    std::string receiver;
    time_t start, end;
    ParcelStatus status;
    int statusInt;
    int anser;
    if (!readInt("是否查询所有快递？(0-否, 1-是): ", anser, 0, 1)) return;
    if (anser == 1) {
        parcelId = "";
        sender = "";
        receiver = "";
        status = ParcelStatus::OTHER;
        start = 0;
        end = 0;
    } else {
        int filterType;
        if (!readInt("是否按单号筛选（0-否，1-是）： ", filterType, 0, 1)) return;
        if (filterType == 1) {
            if (!readString("请输入快递单号: ", parcelId)) return;
        } else {
            parcelId = "";
        }
        if (!readInt("是否按寄件人筛选（0-否，1-是）： ", filterType, 0, 1)) return;
        if (filterType == 1) {
            if (!readString("请输入寄件人: ", sender)) return;
        } else {
            sender = "";
        } 
        if (!readInt("是否按收件人筛选（0-否，1-是）： ", filterType, 0, 1)) return;
        if (filterType == 1) {
            if (!readString("请输入收件人: ", receiver)) return;
        } else {
            receiver = "";
        } 
        if (!sender.empty() && !receiver.empty() && sender == receiver) {
            std::cout << "寄件人和收件人不能相同。" << std::endl;
            return;
        }
        if (!readInt("是否按状态筛选（0-否，1-是）： ", filterType, 0, 1)) return;
        if (filterType == 1) {
            if (!readInt("请输入状态 (0-待揽收, 1-待签收, 2-已签收, 3-其他): ", statusInt, 0, 3)) return;
            status = static_cast<ParcelStatus>(statusInt);
        } else {
            status = ParcelStatus::OTHER;
        } 
        if (!readInt("是否按开始时间筛选（0-否，1-是）： ", filterType, 0, 1)) return;
        if (filterType == 1) {
            if (!readTime("请输入开始时间: ", start)) return;
        } else {
            start = 0;
        } 
        if (!readInt("是否按结束时间筛选（0-否，1-是）： ", filterType, 0, 1)) return;
        if (filterType == 1) {
            if (!readTime("请输入结束时间: ", end)) return;
            if (end > time(nullptr)) {
                std::cout << "结束时间不能晚于当前时间。" << std::endl;
                return;
            }
        } else {
            end = 0;
        } 
        
    }
    
    ErrorCode ec = m_system.queryParcels(parcelId, sender, receiver, "", status, start, end, parcels);
    switch(ec) {
        case ErrorCode::SUCCESS:
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "查询失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "查询失败。" << std::endl;
            break;
    }
    
    if (parcels.empty()) {
        std::cout << "暂无匹配的快递。" << std::endl;
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

void LogisticsClient::rechargeBalanceUI() {
    double amount;
    if (!readDouble("充值金额: ", amount, 0.1, 10000.0)) return;
    
    ErrorCode ec = m_system.rechargeBalance(amount);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "充值成功！" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "充值失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "充值失败。" << std::endl;
            break;
    }
}

void LogisticsClient::queryBalanceUI() {
    double balance;
    ErrorCode ec = m_system.queryBalance(balance);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "当前余额: " << balance << " 元" << std::endl;
            break;
        case ErrorCode::USER_NOT_LOGIN:
            std::cout << "用户未登录。" << std::endl;
            break;
        case ErrorCode::PERMISSION_DENIED:
            std::cout << "不许查别人的余额。" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "查询失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        case ErrorCode::INVALID_ARGS:
            std::cout << "无效参数错误。" << std::endl;
            break;
        default:
            std::cout << "查询失败。" << std::endl;
            break;
    }
}

void LogisticsClient::changePasswordUI() {
    std::string oldPwd, newPwd;
    
    if (!readString("请输入旧密码: ", oldPwd)) return;
    if (!readString("请输入新密码: ", newPwd)) return;
    
    ErrorCode ec = m_system.changePassword(oldPwd, newPwd);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "密码修改成功！" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "密码修改失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "密码修改失败。" << std::endl;
            break;
    }
}

void LogisticsClient::assignParcelUI() {
    std::string parcelId, courier;
    
    if (!readString("快递单号: ", parcelId)) return;
    if (!readString("快递员用户名: ", courier)) return;
    
    ErrorCode ec = m_system.assignParcel(parcelId, courier);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "分配成功！" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "分配失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "分配失败。" << std::endl;
            break;
    }
}

void LogisticsClient::collectParcelUI() {
    std::cout << "请输入要揽收的快递单号(多个用逗号分隔): ";
    std::string line; 
    std::getline(std::cin, line);
    
    std::vector<std::string> ids;
    std::istringstream iss(line);
    std::string token;
    while (std::getline(iss, token, ',')) {
        token = trimString(token);
        if (!token.empty()) {
            ids.push_back(token);
        }
    }
    
    std::vector<std::string> collected; 
    
    ErrorCode ec = m_system.collectParcels(ids, collected);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "揽收成功: ";
            for (size_t i = 0; i < collected.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << collected[i];
            }
            std::cout << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "揽收失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "揽收失败。" << std::endl;
            break;
    }
}

void LogisticsClient::queryUsersUI() {
    std::vector<User*> users;
    int filterType;
    std::string username;
    
    if (!readInt("是否按用户名筛选(可选) 0-否 1-是: ", filterType, 0, 1)) return;
    if (filterType == 1) {
        if (!readString("请输入用户名: ", username)) return;
    } else {
        username = "";
    }

    UserType userType;
    int typeInt;
    
    if (!readInt("请输入用户类型(0:客户, 1:快递员, 2:全部): ", typeInt, 0, 2)) return;
    userType = static_cast<UserType>(typeInt);
    
    ErrorCode ec = m_system.queryUsers(username, userType, users);
    switch(ec) {
        case ErrorCode::SUCCESS:
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "查询失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "查询失败。" << std::endl;
            break;
    }

    if (users.empty()) {
        std::cout << "暂无匹配的用户。" << std::endl;
        return;
    }
    
    std::cout << "用户列表:" << std::endl;
    for (const auto& user : users) {
        std::cout << "用户名: " << user->getUsername() 
                    << ", 用户类型: " << user->getUserTypeStr() << std::endl;
    }

}

void LogisticsClient::deleteAccountUI() {
    std::string targetUsername;
    
    if (!readString("请输入要注销的用户名: ", targetUsername)) return;
    
    // 确认操作
    std::string confirm;
    std::cout << "确认要注销用户 '" << targetUsername << "' 吗？(输入'y'确认): ";
    if (!std::getline(std::cin, confirm) || confirm != "y") {
        std::cout << "操作已取消。" << std::endl;
        return;
    }
    
    ErrorCode ec = m_system.deleteAccount(targetUsername);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "用户账户注销成功。" << std::endl;
            break;
        case ErrorCode::USER_NOT_FOUND:
            std::cout << "用户不存在。" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "注销失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        case ErrorCode::INVALID_ARGS:
            std::cout << "参数解析失败。" << std::endl;
            break;
        case ErrorCode::PERMISSION_DENIED:
            std::cout << "权限不足。" << std::endl;
            break;
        case ErrorCode::DELETE_BLOCKED:
            std::cout << "删除被阻塞。有未完成业务。" << std::endl;
            break;
        default:
            std::cout << "注销失败。" << std::endl;
            break;
    }
}

void LogisticsClient::deleteParcelUI() {
    std::string parcelId;
    if (!readString("请输入要删除的快递单号: ", parcelId)) return;
    
    ErrorCode ec = m_system.deleteParcel(parcelId);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "快递删除成功。" << std::endl;
            break;
        case ErrorCode::PARCEL_NOT_FOUND:
            std::cout << "快递不存在。" << std::endl;
            break;
        case ErrorCode::INVALID_STATUS:
            std::cout << "不许删除未签收的快递。" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "删除失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        case ErrorCode::INVALID_ARGS:
            std::cout << "参数解析失败。" << std::endl;
            break;
        default:
            std::cout << "删除失败。" << std::endl;
            break;
    }
}

void LogisticsClient::logoutUI() {
    ErrorCode ec = m_system.logout();
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "已注销。" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "注销失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "注销失败。" << std::endl;
            break;
    }
}
    
void LogisticsClient::getStatisticsUI() {
    int totalUsers = 0, totalParcels = 0, pendingCollection = 0, collected = 0, Signed = 0;
    double adminTotalBalance = 0.0;
    
    ErrorCode ec = m_system.getStatistics(totalUsers, totalParcels, pendingCollection, collected, Signed, adminTotalBalance);
    switch(ec) {
        case ErrorCode::SUCCESS:
            std::cout << "总用户数: " << totalUsers << std::endl;
            std::cout << "总快递数: " << totalParcels << std::endl;
            std::cout << "待揽收数: " << pendingCollection << std::endl;
            std::cout << "已揽收数: " << collected << std::endl;
            std::cout << "已签收数: " << Signed << std::endl;
            std::cout << "管理员总余额: " << adminTotalBalance << std::endl;
            break;
        case ErrorCode::INVALID_ARGS:
            std::cout << "结果解析失败。" << std::endl;
            break;
        case ErrorCode::PERMISSION_DENIED:
            std::cout << "权限不足。" << std::endl;
            break;
        case ErrorCode::INTERNAL_ERROR:
            std::cout << "查询失败：服务器内部错误，请联系管理员。" << std::endl;
            if (!reconnect()) is_running = false;
            break;
        default:
            std::cout << "获取统计信息失败。" << std::endl;
            break;
    }
}