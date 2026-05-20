#pragma once
#include "FileManager.h"
#include "Logger.h"

extern Logger g_logger;


// ================== 业务系统核心类（系统唯一实例） ==================
class LogisticsSystem {
private:
    std::map<std::string, User*> m_users;       // 用户名 -> User*
    std::map<std::string, Parcel*> m_parcels;   // 快递单号 -> Parcel*
    double m_adminTotalBalance;                 // 管理员总余额（公司资金池）
    int m_nextParcelId;                         // 递增的单号计数器
    bool m_autoAssignCourier;                   // 是否自动分配快递员
    std::map<std::string, int> m_courierCapacity; // 快递员 -> 容量限制

    std::string m_userFile;           // 用户数据文件路径
    std::string m_parcelFile;         // 快递数据文件路径
    std::string m_configFile;         // 配置数据文件路径

    // 生成唯一快递单号
    std::string generateParcelId();

    // 自动分配快递员
    void autoAssignCourier(std::string parcelId);

public:
    // 构造函数：加载数据文件并初始化
    LogisticsSystem(const std::string& userFile, const std::string& parcelFile, const std::string& configFile, bool autoAssignCourier);
    // 析构：保存数据并释放所有用户/快递对象
    ~LogisticsSystem();
    // 持久化当前所有数据到文件
    void saveData();


    // 用户登录 → SUCCESS 或 错误码
    ErrorCode loginUser(const std::string& username, const std::string& password, UserType type);

    // 注销登录 → SUCCESS 或 USER_NOT_LOGIN / USER_NOT_FOUND
    ErrorCode logoutUser(const std::string& username);

    // 注册用户 → SUCCESS 或 USER_EXISTS / INVALID_ARGS
    ErrorCode registerUser(const std::string& username, const std::string& password,
                      const std::string& name, const std::string& phone, const std::string& addr, UserType type);

    // 发送快递 → {SUCCESS, parcelId} 或 {错误码, ""}
    std::pair<ErrorCode, std::string> sendParcel(const std::string& senderName, const std::string& receiverName,
                           ParcelType type, double weight, const std::string& desc);

    // 分配快递员 → SUCCESS 或 USER_NOT_FOUND / PARCEL_NOT_FOUND
    ErrorCode assignCourier(const std::string& parcelId, const std::string& courierName);

    // 揽收快递 → {SUCCESS, collected} 或 {NO_RESULT, {}}
    std::pair<ErrorCode, std::vector<std::string>> collectParcels(const std::string& courierName, const std::vector<std::string>& parcelIds);

    // 签收快递 → {SUCCESS, signed} 或 {NO_RESULT, {}}
    std::pair<ErrorCode, std::vector<std::string>> signParcels(const std::string& userName, const std::vector<std::string>& parcelIds);

    // 查询快递 → {SUCCESS, parcels}（结果为空也是 SUCCESS）
    std::pair<ErrorCode, std::vector<Parcel*>> queryParcels(const std::string& parcelId, const std::string& senderName = "", 
        const std::string& receiverName = "",const std::string& courierName = "", 
        const ParcelStatus& status = ParcelStatus::OTHER, const time_t& startTime = 0, const time_t& endTime = 0);

    // 充值用户余额 → SUCCESS 或 INVALID_AMOUNT / USER_NOT_FOUND
    ErrorCode rechargeUser(const std::string& username, double amount);

    // 查询用户余额 → {SUCCESS, balance} 或 {USER_NOT_FOUND, -1}
    std::pair<ErrorCode, double> getUserBalance(const std::string& username) const;
    
    // 修改用户密码 → SUCCESS 或 USER_NOT_FOUND / LOGIN_FAILED
    ErrorCode changeUserPassword(const std::string& username, const std::string& oldPwd, const std::string& newPwd);

    // 查询用户 → {SUCCESS, users}（结果为空也是 SUCCESS）
    std::pair<ErrorCode, std::vector<User*>> getUsers(const std::string& username = "", const UserType& userType = UserType::ADMINISTRATOR);

    // 删除用户 → SUCCESS 或 USER_NOT_FOUND / DELETE_BLOCKED
    ErrorCode deleteUser(const std::string& targetUsername);

    // 删除快递 → SUCCESS 或 PARCEL_NOT_FOUND / INVALID_STATUS
    ErrorCode deleteParcel(const std::string& parcelId);

    // 获取统计信息 → {SUCCESS, stats}
    ErrorCode getStatistics(int& totalUsers, int& totalParcels, int& pendingCollection, int& collected, int& Signed, double& adminTotalBalance) const;

};