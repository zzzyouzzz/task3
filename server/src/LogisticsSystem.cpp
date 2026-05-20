#include "LogisticsSystem.h"

// 生成唯一快递单号：PCL + 时间戳 + 递增序号
std::string LogisticsSystem::generateParcelId() {
    return "PCL" + std::to_string(time(nullptr)) + "-" + std::to_string(m_nextParcelId++);
}

// 自动分配快递员：选择当前包裹数最少的快递员
void LogisticsSystem::autoAssignCourier(std::string parcelId) {
    if (!m_autoAssignCourier) return;
    int minCapacity = INT_MAX;
    std::string minCourier = "";
    for (const auto& pair : m_courierCapacity) {
        if (pair.second < minCapacity) {
            minCapacity = pair.second;
            minCourier = pair.first;
        }
    }
    if (!minCourier.empty()) {
        assignCourier(parcelId, minCourier);
        m_courierCapacity[minCourier]++;
    }
}

// 构造函数：加载数据文件 + 初始化默认管理员 + 计算单号计数器 + 快递员负载 + 系统运行标志
LogisticsSystem::LogisticsSystem(const std::string& userFile, const std::string& parcelFile, const std::string& configFile, bool autoAssignCourier)
    : m_adminTotalBalance(0.0), m_nextParcelId(1), m_autoAssignCourier(autoAssignCourier),
        m_userFile(userFile), m_parcelFile(parcelFile), m_configFile(configFile) {
    // 从磁盘加载已有数据
    m_users = FileManager::loadUsers(m_userFile);
    m_parcels = FileManager::loadParcels(m_parcelFile);
    m_adminTotalBalance = FileManager::loadConfig(m_configFile);
    // 确保至少有一个管理员账号
    if (m_users.find("admin") == m_users.end()) {
        Administrator* admin = new Administrator("admin", "admin123", "System Admin", "000-0000", "Head Office");
        m_users["admin"] = admin;
        saveData();
    }
    // 计算当前最大单号计数器
    for (const auto& p : m_parcels) {
        std::string id = p.first;
        size_t pos = id.find_last_of('-');
        if (pos != std::string::npos) {
            int num = std::stoi(id.substr(pos+1));
            if (num >= m_nextParcelId) m_nextParcelId = num + 1;
        }
    }

    // 初始化快递员负载
    std::vector<User*> couriers;
    ErrorCode result = getUsers(couriers, "", UserType::COURIER);
    if (result != ErrorCode::SUCCESS) return;
    for (const auto& c : couriers) {
        std::string courier = c->getUsername();
        std::vector<Parcel*> parcelsVec;
        ErrorCode result = queryParcels(parcelsVec, "", "", "", courier);
        if (result != ErrorCode::SUCCESS) continue;
        // 计算快递员负载
        if (parcelsVec.empty()) continue;
        m_courierCapacity[courier] = static_cast<int>(parcelsVec.size());
    } 
}

// 析构：保存数据 → 释放所有 User 和 Parcel 动态内存
LogisticsSystem::~LogisticsSystem() {
    // 注销所有用户
    for (auto& pair : m_users) pair.second->logout();
    saveData();
    for (auto& pair : m_users) delete pair.second;
    for (auto& p : m_parcels) delete p.second;
}

// 持久化所有数据到磁盘文件
void LogisticsSystem::saveData() {
    FileManager::saveUsers(m_userFile, m_users);
    FileManager::saveParcels(m_parcelFile, m_parcels);
    FileManager::saveConfig(m_configFile, m_adminTotalBalance);
}

// 用户登录：校验用户名存在 → 身份类型匹配 → 密码正确 → 返回用户指针
ErrorCode LogisticsSystem::loginUser(const std::string& username, const std::string& password, UserType type) {
    auto it = m_users.find(username);
    if (it == m_users.end()) return ErrorCode::USER_NOT_FOUND;
    if (it->second->getUserType() != type) return ErrorCode::LOGIN_FAILED;
    if (!it->second->login(password)) return ErrorCode::LOGIN_FAILED;
    if (it->second->isLogin()) return ErrorCode::USER_ALREADY_LOGIN;
    // 登录成功后，设置用户状态为已登录
    it->second->login();
    saveData();
    return ErrorCode::SUCCESS;
}

// 注册用户：检查用户名唯一性 → 创建对应用户对象 → 持久化
ErrorCode LogisticsSystem::registerUser(const std::string& username, const std::string& password,
                    const std::string& name, const std::string& phone, const std::string& addr, UserType type) {
    if (m_users.find(username) != m_users.end()) return ErrorCode::USER_EXISTS;
    User* u = nullptr;
    if (type == UserType::CUSTOMER) {
        u = new Customer(username, password, name, phone, addr);
    } else if (type == UserType::COURIER) {
        u = new Courier(username, password, name, phone, addr);
    } else {
        return ErrorCode::INVALID_ARGS;
    }
    m_users[username] = u;
    saveData();
    return ErrorCode::SUCCESS;
}

// 注销登录：校验用户存在 → 校验是否登录 → 重置登录状态 → 持久化
ErrorCode LogisticsSystem::logoutUser(const std::string& username) {
    auto it = m_users.find(username);
    if (it == m_users.end()) return ErrorCode::USER_NOT_FOUND;
    if (!it->second->isLogin()) return ErrorCode::USER_NOT_LOGIN;
    it->second->logout();
    saveData();
    return ErrorCode::SUCCESS;
}



// 发送快递：校验收件人存在 → 生成单号 → 扣款 → 自动分配快递员 → 持久化
std::pair<ErrorCode, std::string> LogisticsSystem::sendParcel(const std::string& senderName, const std::string& receiverName,
                        ParcelType type, double weight, const std::string& desc) {
    if (m_users.find(receiverName) == m_users.end()) return {ErrorCode::USER_NOT_FOUND, ""};
    User* sender = m_users[senderName];
    Parcel* parcel = nullptr;
    std::string pid = generateParcelId();
    switch (type) {
        case ParcelType::NORMAL:
            parcel = new NormalParcel(pid, senderName, receiverName, weight, desc);
            break;
        case ParcelType::FRAGILE:
            parcel = new FragileParcel(pid, senderName, receiverName, weight, desc);
            break;
        case ParcelType::BOOK:
            parcel = new BookParcel(pid, senderName, receiverName, weight, desc);
            break;
    }
    double price = parcel->getPrice();
    if (!sender->deduct(price)) {
        delete parcel;
        return {ErrorCode::INSUFFICIENT_BALANCE, ""};
    }
    m_adminTotalBalance += price;
    m_parcels[pid] = parcel;
    
    autoAssignCourier(pid);
    g_logger.info("Auto-assigned courier for parcel: " + pid);
    saveData();
    
    return {ErrorCode::SUCCESS, pid};
}

// 分配快递员：校验快递员存在 + 校验快递单号存在 → 保存
ErrorCode LogisticsSystem::assignCourier(const std::string& parcelId, const std::string& courierName) {
    auto courierIt = m_users.find(courierName);
    if (courierIt == m_users.end() || courierIt->second->getUserType() != UserType::COURIER) {
        return ErrorCode::USER_NOT_FOUND;
    }
    auto parcelIt = m_parcels.find(parcelId);
    if (parcelIt == m_parcels.end()) {
        return ErrorCode::PARCEL_NOT_FOUND;
    }
    if (parcelIt->second->getStatus() != ParcelStatus::PENDING_COLLECTION) {
        return ErrorCode::PARCEL_STATUS_INVALID;
    }
    parcelIt->second->setCourier(courierName);
    parcelIt->second->setStatus(ParcelStatus::PENDING_COLLECTION);
    saveData();
    return ErrorCode::SUCCESS;
}

// 揽收快递：校验快递员 → 遍历包裹 → 结算佣金 → 状态置为待签收
std::pair<ErrorCode, std::vector<std::string>> LogisticsSystem::collectParcels(const std::string& courierName, const std::vector<std::string>& parcelIds) {
    std::vector<std::string> collected;

    auto courierIt = m_users.find(courierName);
    if (courierIt == m_users.end() || courierIt->second->getUserType() != UserType::COURIER) {
        return {ErrorCode::USER_NOT_FOUND, collected};
    }
    
    for (const auto& parcelId : parcelIds) {
        auto parcelIt = m_parcels.find(parcelId);
        if (parcelIt == m_parcels.end() || parcelIt->second->getCourierName() != courierName ||
            parcelIt->second->getStatus() != ParcelStatus::PENDING_COLLECTION) {
            continue;
        }
        if (m_adminTotalBalance >= parcelIt->second->getPrice() * 0.5) {
            m_adminTotalBalance -= parcelIt->second->getPrice() * 0.5;
            courierIt->second->recharge(parcelIt->second->getPrice() * 0.5);
            parcelIt->second->setStatus(ParcelStatus::PENDING_SIGN);
            collected.push_back(parcelId);
        }
    }
    
    if (!collected.empty()) {
        saveData();
        return {ErrorCode::SUCCESS, collected};
    }
    return {ErrorCode::NO_RESULT, collected};
}

// 签收快递：校验收件人 → 状态置为已签收 → 记录签收时间
std::pair<ErrorCode, std::vector<std::string>> LogisticsSystem::signParcels(const std::string& userName, const std::vector<std::string>& parcelIds) {
    std::vector<std::string> signedList;
    
    for (const auto& parcelId : parcelIds) {
        auto parcelIt = m_parcels.find(parcelId);
        if (parcelIt == m_parcels.end() || parcelIt->second->getReceiverName() != userName ||
            parcelIt->second->getStatus() != ParcelStatus::PENDING_SIGN) {
            continue;
        }
        parcelIt->second->setStatus(ParcelStatus::SIGNED);
        parcelIt->second->setReceiveTime(time(nullptr));
        signedList.push_back(parcelId);
    }
    
    if (!signedList.empty()) {
        saveData();
        return {ErrorCode::SUCCESS, signedList};
    }
    return {ErrorCode::NO_RESULT, signedList};
}


// 查询快递：根据单号/寄件人/收件人/快递员/状态/时间范围多条件筛选
ErrorCode LogisticsSystem::queryParcels(std::vector<Parcel*>& result, const std::string& parcelId, const std::string& senderName, const std::string& receiverName,
    const std::string& courierName, const ParcelStatus& status, const time_t& startTime, const time_t& endTime) {
    result.clear();
    for (auto& parcel : m_parcels) {
        if (!parcelId.empty() && parcel.first != parcelId) continue;
        if (!senderName.empty() && parcel.second->getSenderName() != senderName) continue;
        if (!receiverName.empty() && parcel.second->getReceiverName() != receiverName) continue;
        if (!courierName.empty() && parcel.second->getCourierName() != courierName) continue;
        if (status != ParcelStatus::OTHER && parcel.second->getStatus() != status) continue;
        if (startTime != 0 && parcel.second->getSendTime() < startTime) continue;
        if (endTime != 0 && parcel.second->getReceiveTime() > endTime) continue;
        result.push_back(parcel.second);
    }      
    return ErrorCode::SUCCESS;
}

// 充值：验证金额 > 0 + 用户存在 → 增加余额 → 持久化
ErrorCode LogisticsSystem::rechargeUser(const std::string& username, double amount) {
    if (amount <= 0.0) return ErrorCode::INVALID_AMOUNT;
    auto it = m_users.find(username);
    if (it == m_users.end()) return ErrorCode::USER_NOT_FOUND;
    it->second->recharge(amount);
    saveData();
    return ErrorCode::SUCCESS;
}

// 查询余额：管理员返回公司资金池，其他返回个人余额
ErrorCode LogisticsSystem::getUserBalance(const std::string& username, double& balance) const {
    auto it = m_users.find(username);
    if (it == m_users.end()) return ErrorCode::USER_NOT_FOUND;
    if(it->second->getUserType() == UserType::ADMINISTRATOR) {
        balance = m_adminTotalBalance;
        return ErrorCode::SUCCESS;
    }
    balance = it->second->getBalance();
    return ErrorCode::SUCCESS;
}

// 修改密码：校验旧密码 → 更新 → 持久化
ErrorCode LogisticsSystem::changeUserPassword(const std::string& username, const std::string& oldPwd, const std::string& newPwd) {
    auto it = m_users.find(username);
    if (it == m_users.end()) return ErrorCode::USER_NOT_FOUND;
    bool changed = it->second->changePassword(oldPwd, newPwd);
    if (changed) { saveData(); return ErrorCode::SUCCESS; }
    return ErrorCode::LOGIN_FAILED;
}

// 查询用户：按用户名（可选）和用户类型（可选）筛选
ErrorCode LogisticsSystem::getUsers(std::vector<User*>& users, const std::string& username, const UserType& userType) {
    users.clear();
    for (const auto& pair : m_users) {
        if (!username.empty() && pair.first != username) continue;
        if (userType != UserType::ADMINISTRATOR && pair.second->getUserType() != userType) continue;
        users.push_back(pair.second);
    }
    return ErrorCode::SUCCESS;
}

// 删除用户：用户存在 + 非管理员 + 无未完成快递 → 释放内存 → 删除 → 保存
ErrorCode LogisticsSystem::deleteUser(const std::string& targetUsername) {
    auto targetIt = m_users.find(targetUsername);
    if (targetIt == m_users.end()) return ErrorCode::USER_NOT_FOUND;
    if (targetIt->second->getUserType() == UserType::ADMINISTRATOR) return ErrorCode::DELETE_BLOCKED;
    for (const auto& parcel : m_parcels) {
        if ((parcel.second->getSenderName() == targetUsername || parcel.second->getReceiverName() == targetUsername) &&
            parcel.second->getStatus() != ParcelStatus::SIGNED) {
            return ErrorCode::DELETE_BLOCKED;
        }
    }
    delete targetIt->second;
    m_users.erase(targetIt);
    saveData();
    return ErrorCode::SUCCESS;
}

// 删除快递：仅已签收的快递可删除
ErrorCode LogisticsSystem::deleteParcel(const std::string& parcelId) {
    auto parcelIt = m_parcels.find(parcelId);
    if (parcelIt == m_parcels.end()) return ErrorCode::PARCEL_NOT_FOUND;
    if (parcelIt->second->getStatus() != ParcelStatus::SIGNED) return ErrorCode::INVALID_STATUS;
    m_parcels.erase(parcelIt);
    saveData();
    return ErrorCode::SUCCESS;
}

// 获取统计信息
ErrorCode LogisticsSystem::getStatistics(int& totalUsers, int& totalParcels, int& pendingCollection, int& collected, int& Signed, double& adminTotalBalance) const {
    totalUsers = m_users.size();
    totalParcels = m_parcels.size();
    pendingCollection = 0;
    collected = 0;
    Signed = 0;
    adminTotalBalance = m_adminTotalBalance;
    for (const auto& parcel : m_parcels) {
        if (parcel.second->getStatus() == ParcelStatus::PENDING_COLLECTION) pendingCollection++;
        if (parcel.second->getStatus() == ParcelStatus::PENDING_SIGN) collected++;
        if (parcel.second->getStatus() == ParcelStatus::SIGNED) Signed++;
    }
    return ErrorCode::SUCCESS;
}
