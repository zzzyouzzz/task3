#include "FileManager.h"

// ================== FileManager 实现 ==================
// 将所有用户序列化保存到文件，每行一个用户
bool FileManager::saveUsers(const std::string& filename, const std::map<std::string, User*>& users) {
    std::ofstream ofs(filename);
    if (!ofs) return false;
    for (const auto& pair : users) {
        ofs << pair.second->serialize() << std::endl;
    }
    return true;
}

// 从文件加载所有用户，按 | 分隔解析每行字段
std::map<std::string, User*> FileManager::loadUsers(const std::string& filename) {
    std::map<std::string, User*> users;
    std::ifstream ifs(filename);
    if (!ifs) return users;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
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

// 将所有快递序列化保存到文件，每行一个快递
bool FileManager::saveParcels(const std::string& filename, const std::map<std::string, Parcel*>& parcels) {
    std::ofstream ofs(filename);
    if (!ofs) return false;
    for (const auto& p : parcels) {
        ofs << p.second->serialize() << std::endl;
    }
    return true;
}

// 从文件加载所有快递，按 | 分隔解析每行字段
std::map<std::string, Parcel*> FileManager::loadParcels(const std::string& filename) {
    std::map<std::string, Parcel*> parcels;
    std::ifstream ifs(filename);
    if (!ifs) return parcels;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::vector<std::string> fields;
        std::string token;
        while (std::getline(iss, token, DELIMITER)) fields.push_back(token);
        getline(iss, token);
        fields.push_back(token);
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

bool FileManager::saveConfig(const std::string& filename, double adminBalance) {
    std::ofstream ofs(filename);
    if (!ofs) return false;
    ofs << adminBalance << std::endl;
    return true;
}

double FileManager::loadConfig(const std::string& filename) {
    std::ifstream ifs(filename);
    double balance = 0.0;
    if (ifs) ifs >> balance;
    return balance;
}
