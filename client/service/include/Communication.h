#pragma once
#include <string>
#include <vector>
#include "common.h"
#include "Logger.h"
#include "User.h"
#include "Parcel.h"

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

extern Logger g_logger;

// ================== 通信类 ==================
class Communication {
private:
    socket_t m_socket;           // 连接 socket
    bool m_connected;             // 是否已连接

    // 发送请求并获取响应
    bool sendRequest(const std::string& cmd, const std::vector<std::string>& args, std::string& response);

    // 解析RESPONSE消息
    bool parseResponse(const std::string& resp, ErrorCode& status, std::vector<std::string>& data);

    std::string getSocketErrorString() const;

public:
    Communication() : m_socket(INVALID_SOCKET), m_connected(false) {}
    ~Communication() {
        disconnect();
    }

    // 连接到服务器
    bool connectToServer(const std::string& ip, int port);
    // 断开连接
    void disconnect();

    // 当前是否已连接
    bool isConnected() const { return m_connected; }

    // 用户登录
    ErrorCode loginUser(const std::string& username, const std::string& password, UserType type, std::string& userId);

    // 发送快递
    ErrorCode sendParcel(const std::string& receiver, ParcelType type, double weight, const std::string& desc, std::string& parcelId);

    // 管理员分配快递员
    ErrorCode assignParcel(const std::string& parcelId, const std::string& courier);

    // 快递员揽收
    ErrorCode collectParcels(const std::vector<std::string>& ids, std::vector<std::string>& collectedList);

    // 用户签收
    ErrorCode signParcels(const std::vector<std::string>& ids, std::vector<std::string>& signedList);

    // 查询快递
    ErrorCode queryParcels(const std::string& Id, const std::string& sender, const std::string& receiver, 
            const std::string& courier, const ParcelStatus& s, const time_t start, const time_t end, std::vector<Parcel>& parcels);

    // 管理员查询用户
    ErrorCode queryUsers(const std::string& username, const UserType type, std::vector<User>& users);

    // 注册用户
    ErrorCode registerUser(const std::string& username, const std::string& password,
                      const std::string& name, const std::string& phone, const std::string& addr, UserType type);

    // 充值余额
    ErrorCode rechargeBalance(double amount);

    // 查询余额
    ErrorCode queryBalance(double& balance);

    // 修改密码
    ErrorCode changePassword(const std::string& oldPwd, const std::string& newPwd);

    // 注销账户（管理员功能）
    ErrorCode deleteAccount(const std::string& targetUsername);

    // 删除快递（管理员功能）
    ErrorCode deleteParcel(const std::string& parcelId);

    // 用户注销
    ErrorCode logout();

    // 获取统计信息（管理员功能）
    ErrorCode getStatistics(int& totalUsers, int& totalParcels, int& pendingCollection, int& collected, int& Signed, double& adminTotalBalance);

};
