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
    // 将所有用户序列化写入文件
    static bool saveUsers(const std::string& filename, const std::map<std::string, class User*>& users);
    // 从文件加载所有用户
    static std::map<std::string, class User*> loadUsers(const std::string& filename);
    // 将所有快递序列化写入文件
    static bool saveParcels(const std::string& filename, const std::map<std::string, class Parcel*>& parcels);
    // 从文件加载所有快递
    static std::map<std::string,class Parcel*> loadParcels(const std::string& filename);
    // 保存管理员总余额到配置文件
    static bool saveConfig(const std::string& filename, double adminBalance);
    // 从配置文件加载管理员总余额
    static double loadConfig(const std::string& filename);
};