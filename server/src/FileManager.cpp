#include "FileManager.h"
#include <filesystem>

namespace {
    const std::string VERSION_PREFIX = "V=";

    // 生成 .tmp 临时文件路径
    std::string tempPath(const std::string& filename) {
        return filename + ".tmp";
    }

    // 读取并解析版本头，返回版本头之后的首行数据
    // 旧格式文件（无版本头）返回 0 并保留首行
    std::string readVersionHeader(std::ifstream& ifs, int& outVersion) {
        std::string firstLine;
        if (!std::getline(ifs, firstLine)) {
            outVersion = 0;
            return {};
        }
        if (firstLine.rfind(VERSION_PREFIX, 0) == 0) {
            parseInt(firstLine.substr(VERSION_PREFIX.size()), outVersion);
            std::string nextLine;
            std::getline(ifs, nextLine);
            return nextLine;
        }
        outVersion = 0;
        return firstLine;
    }

    // 原子提交：将 .tmp 文件重命名为目标文件
    bool commitTempFile(const std::string& target) {
        std::error_code ec;
        std::filesystem::rename(tempPath(target), target, ec);
        return !ec;
    }
}

// ================== FileManager 实现 ==================

// 将所有用户序列化保存到文件（带版本头 + 临时文件 + 原子重命名）
bool FileManager::saveUsers(const std::string& filename, const std::map<std::string, User*>& users, int version) {
    std::string tmp = tempPath(filename);
    std::ofstream ofs(tmp);
    if (!ofs) return false;
    ofs << VERSION_PREFIX << version << std::endl;
    for (const auto& pair : users) {
        ofs << pair.second->serialize() << std::endl;
    }
    ofs.close();
    if (ofs.fail()) return false;
    return commitTempFile(filename);
}

// 从文件加载所有用户（支持带版本头的新格式与旧格式）
std::map<std::string, User*> FileManager::loadUsers(const std::string& filename, int& outVersion) {
    std::map<std::string, User*> users;
    std::ifstream ifs(filename);
    if (!ifs) { outVersion = 0; return users; }

    // 处理版本头 + 读取所有数据行
    std::vector<std::string> dataLines;
    std::string firstData = readVersionHeader(ifs, outVersion);
    if (!firstData.empty()) dataLines.push_back(firstData);
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) dataLines.push_back(line);
    }

    for (const auto& dataLine : dataLines) {
        std::istringstream iss(dataLine);
        std::string token;
        std::vector<std::string> fields;
        while (std::getline(iss, token, DELIMITER)) {
            fields.push_back(token);
        }
        if (fields.size() < 7) continue;
        int type;
        if (!parseInt(fields[0], type)) {
            g_logger.error("Invalid user type: " + fields[0]);
            continue;
        }
        std::string uname = fields[1], pwd = fields[2], name = fields[3];
        std::string phone = fields[4], addr = fields[5];
        double balance;
        if (!parseDouble(fields[6], balance)) {
            g_logger.error("Invalid balance: " + fields[6]);
            continue;
        }
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

// 将所有快递序列化保存到文件（带版本头 + 临时文件 + 原子重命名）
bool FileManager::saveParcels(const std::string& filename, const std::map<std::string, Parcel*>& parcels, int version) {
    std::string tmp = tempPath(filename);
    std::ofstream ofs(tmp);
    if (!ofs) return false;
    ofs << VERSION_PREFIX << version << std::endl;
    for (const auto& p : parcels) {
        ofs << p.second->serialize() << std::endl;
    }
    ofs.close();
    if (ofs.fail()) return false;
    return commitTempFile(filename);
}

// 从文件加载所有快递（支持带版本头的新格式与旧格式）
std::map<std::string, Parcel*> FileManager::loadParcels(const std::string& filename, int& outVersion) {
    std::map<std::string, Parcel*> parcels;
    std::ifstream ifs(filename);
    if (!ifs) { outVersion = 0; return parcels; }

    // 处理版本头 + 读取所有数据行
    std::vector<std::string> dataLines;
    std::string firstData = readVersionHeader(ifs, outVersion);
    if (!firstData.empty()) dataLines.push_back(firstData);
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) dataLines.push_back(line);
    }

    for (const auto& dataLine : dataLines) {
        std::istringstream iss(dataLine);
        std::vector<std::string> fields;
        std::string token;
        while (std::getline(iss, token, DELIMITER)) fields.push_back(token);
        if (fields.size() < 10) continue;
        int type;
        if (!parseInt(fields[0], type)) {
            g_logger.error("Invalid parcel type: " + fields[0]);
            continue;
        }
        std::string id = fields[1], sender = fields[2], receiver = fields[3];
        long long sendTimeLL = 0, recvTimeLL = 0;
        if (!parseLongLong(fields[4], sendTimeLL) || !parseLongLong(fields[5], recvTimeLL)) {
            g_logger.error("Invalid time in parcel " + id + ": send=" + fields[4] + " recv=" + fields[5]);
            continue;
        }
        time_t sendTime = static_cast<time_t>(sendTimeLL);
        time_t recvTime = static_cast<time_t>(recvTimeLL);
        int statusInt;
        if (!parseInt(fields[6], statusInt)) {
            g_logger.error("Invalid parcel status: " + fields[6]);
            continue;
        }
        ParcelStatus status = static_cast<ParcelStatus>(statusInt);
        std::string desc = fields[7];
        double weight;
        if (!parseDouble(fields[8], weight)) {
            g_logger.error("Invalid weight: " + fields[8]);
            continue;
        }
        std::string courier = fields[9];
        Parcel* p = nullptr;
        switch (static_cast<ParcelType>(type)) {
            case ParcelType::NORMAL: p = new NormalParcel(static_cast<ParcelType>(type), id, sender, receiver, sendTime, recvTime, status, desc, weight, courier); break;
            case ParcelType::FRAGILE: p = new FragileParcel(static_cast<ParcelType>(type), id, sender, receiver, sendTime, recvTime, status, desc, weight, courier); break;
            case ParcelType::BOOK: p = new BookParcel(static_cast<ParcelType>(type), id, sender, receiver, sendTime, recvTime, status, desc, weight, courier); break;
        }
        if (p) {
            parcels[id] = p;
        }
    }
    return parcels;
}

bool FileManager::saveConfig(const std::string& filename, double adminBalance, int version) {
    std::string tmp = tempPath(filename);
    std::ofstream ofs(tmp);
    if (!ofs) return false;
    ofs << VERSION_PREFIX << version << std::endl;
    ofs << adminBalance << std::endl;
    ofs.close();
    if (ofs.fail()) return false;
    return commitTempFile(filename);
}

double FileManager::loadConfig(const std::string& filename, int& outVersion) {
    std::ifstream ifs(filename);
    if (!ifs) { outVersion = 0; return 0.0; }

    std::string dataLine = readVersionHeader(ifs, outVersion);
    double balance = 0.0;
    if (!dataLine.empty()) {
        std::istringstream iss(dataLine);
        iss >> balance;
    }
    return balance;
}
