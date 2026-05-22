#pragma once
#include <vector>
#include <map>
#include "User.h"
#include "Parcel.h"
#include "Logger.h"

extern Logger g_logger;

// ================== 数据持久化管理类 ==================
class FileManager {
public:
    // 将所有用户序列化写入文件（带版本号，写入.tmp后原子重命名）
    static bool saveUsers(const std::string& filename, const std::map<std::string, class User*>& users, int version);
    // 从文件加载所有用户（通过 outVersion 返回文件版本号）
    static std::map<std::string, class User*> loadUsers(const std::string& filename, int& outVersion);
    // 将所有快递序列化写入文件（带版本号，写入.tmp后原子重命名）
    static bool saveParcels(const std::string& filename, const std::map<std::string, class Parcel*>& parcels, int version);
    // 从文件加载所有快递（通过 outVersion 返回文件版本号）
    static std::map<std::string,class Parcel*> loadParcels(const std::string& filename, int& outVersion);
    // 保存管理员总余额到配置文件（带版本号，写入.tmp后原子重命名）
    static bool saveConfig(const std::string& filename, double adminBalance, int version);
    // 从配置文件加载管理员总余额（通过 outVersion 返回文件版本号）
    static double loadConfig(const std::string& filename, int& outVersion);
};