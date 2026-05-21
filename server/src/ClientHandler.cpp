#include "ClientHandler.h"
bool ClientHandler::handleRequest(std::string& response) {
    size_t pos = buf.find('\n');
    if (pos == std::string::npos) return false;
    std::string request = buf.substr(0, pos+1);
    buf.erase(0, pos+1);
    g_logger.debug("Raw request received: " + request);
    std::string cmd;
    std::vector<std::string> args;
    Protocol::parseRequest(request, cmd, args);

    std::string argSummary = "args_count=" + std::to_string(args.size());
    if (cmd != Command::LOGIN && cmd != Command::REGISTER && cmd != Command::CHANGE_PASSWORD) {
        argSummary = "args=[";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) argSummary += ", ";
            argSummary += args[i].substr(0, 64);
            if (args[i].size() > 64) argSummary += "...";
        }
        argSummary += "]";
    }
    g_logger.info("req = " + std::to_string(requestId) + " Processing command: " + cmd + " from socket, " + argSummary);

    try {
        response = processCommand(cmd, args);
    } catch (const std::exception& ex) {
        g_logger.error("req=" + std::to_string(requestId) + " Unhandled exception processing command: " + std::string(ex.what()));
        response = buildResponse(ErrorCode::UNKNOWN, {"Internal server error"});
    } catch (...) {
        g_logger.error("req=" + std::to_string(requestId) + " Unhandled unknown exception processing command");
        response = buildResponse(ErrorCode::UNKNOWN, {"Internal server error"});
    }
    return true;
}
std::string ClientHandler::processCommand(const std::string& cmd, const std::vector<std::string>& args) {
    if (cmd == Command::LOGIN) {
        return handleLogin(args);
    } else if (cmd == Command::REGISTER) {       
        return handleRegister(args);
    } else {
        if (m_currentUser.empty()) {
            g_logger.warning("User not logged in");
            return buildResponse(ErrorCode::PERMISSION_DENIED, {"Not logged in"});
        } else {
            if (cmd == Command::SEND_PARCEL) {
                if (m_userType != UserType::CUSTOMER) {
                    g_logger.warning("SendParcel command: Permission denied for user: " + 
                        m_currentUser);
                    return buildResponse(ErrorCode::PERMISSION_DENIED, {"Only customers can send parcels"});
                }
                return handleSendParcel(args);
            } else if (cmd == Command::ASSIGN_PARCEL) {
                if (m_userType != UserType::ADMINISTRATOR) {
                    g_logger.warning("AssignParcel command: Permission denied for user: " + 
                        m_currentUser);
                    return buildResponse(ErrorCode::PERMISSION_DENIED, {"Only administrators can assign parcels"});
                }
                return handleAssignParcel(args);
            } else if (cmd == Command::COLLECT_PARCEL) {
                if (m_userType != UserType::COURIER) {
                    g_logger.warning("CollectParcel command: Permission denied for user: " + 
                        m_currentUser);
                    return buildResponse(ErrorCode::PERMISSION_DENIED, {"Only couriers can collect parcels"});
                }
                return handleCollectParcel(args);
            } else if (cmd == Command::SIGN_PARCEL) {
                if (m_userType != UserType::CUSTOMER) {
                    g_logger.warning("SignParcel command: Permission denied for user: " + 
                        m_currentUser);    
                    return buildResponse(ErrorCode::PERMISSION_DENIED, {"Only customers can sign parcels"});
                }
                return handleSignParcel(args);
            } else if (cmd == Command::QUERY_PARCEL) {    
                return handleQueryParcel(args);
            } else if (cmd == Command::QUERY_USER) {
                return handleQueryUser(args);   
            } else if (cmd == Command::RECHARGE_BALANCE) {
                if (m_userType != UserType::CUSTOMER) {
                    g_logger.warning("RechargeBalance command: Permission denied for user: " + 
                        m_currentUser);
                    return buildResponse(ErrorCode::PERMISSION_DENIED, {"Only customers can recharge balance"});
                }
                return handleRechargeBalance(args);
            } else if (cmd == Command::QUERY_BALANCE) {
                return handleQueryBalance(args);
            } else if (cmd == Command::CHANGE_PASSWORD) {
                return handleChangePassword(args);
            } else if (cmd == Command::DELETE_ACCOUNT) {
                if (m_userType != UserType::ADMINISTRATOR) {
                    g_logger.warning("DeleteAccount command: Permission denied for user: " + 
                        m_currentUser);
                    return buildResponse(ErrorCode::PERMISSION_DENIED, {"Only administrators can delete accounts"});
                }
                return handleDeleteAccount(args);
            } else if (cmd == Command::DELETE_PARCEL) {
                if (m_userType != UserType::ADMINISTRATOR) {
                    g_logger.warning("DeleteParcel command: Permission denied for user: " + 
                        m_currentUser);
                    return buildResponse(ErrorCode::PERMISSION_DENIED, {"Only administrators can delete parcels"});
                }
                return handleDeleteParcel(args);
            } else if (cmd == Command::LOGOUT) {
                return handleLogout(args);
            } else if (cmd == Command::GET_STATISTICS) {
                if (m_userType != UserType::ADMINISTRATOR) {
                    g_logger.warning("GetStatistics command: Permission denied for user: " + 
                        m_currentUser);
                    return buildResponse(ErrorCode::PERMISSION_DENIED, {"Only administrators can get statistics"});
                }
                return handleGetStatistics(args);
            } else {
                return buildResponse(ErrorCode::UNKNOWN, {"Unknown command"});
            }
        }
    }
}


std::string ClientHandler::buildResponse(ErrorCode code, const std::vector<std::string>& data) {
    std::string status = std::to_string(static_cast<int>(code));
    std::string resp = Command::RESPONSE + std::string(1, DELIMITER) + status;

    for (const auto& d : data) {
        resp += std::string(1, DELIMITER) + d;
    }
    resp += '\n';

    g_logger.info("req = " + std::to_string(requestId) + " resp = " + resp);

    return resp;
}

std::string ClientHandler::handleRegister(const std::vector<std::string>& args) {
    if (args.size() < 6) {
        g_logger.warning("Register command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }
    std::string username = args[0];
    std::string password = args[1];
    std::string name = args[2];
    std::string phone = args[3];
    std::string address = args[4];
    int typeInt = 0;
    if (!parseInt(args[5], typeInt) || typeInt < 0 || typeInt > static_cast<int>(UserType::COURIER)) {
        g_logger.warning("Register command: Invalid user type: " + args[5]);
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid user type"});
    }
    UserType type = static_cast<UserType>(typeInt);
    
    g_logger.info("Register attempt for user: " + username + " type: " + std::to_string(typeInt));
    
    // 只允许注册客户或快递员
    if (type != UserType::CUSTOMER && type != UserType::COURIER) {
        g_logger.warning("Register command: Invalid user type: " + std::to_string(typeInt));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid user type"});
    }
    
    ErrorCode ec = m_system->registerUser(username, password, name, phone, address, type);
    
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("User registered successfully: " + username + " as " + (type == UserType::CUSTOMER ? "Customer" : "Courier"));
        return buildResponse(ec, {"Registration successful"});
    } else {
        g_logger.warning("User registration failed - username already exists: " + username);
        return buildResponse(ec, {"Username already exists"});
    }
}

std::string ClientHandler::handleLogout(const std::vector<std::string>& args) {
    if (args.size() != 0) {
        g_logger.warning("Logout command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }

    ErrorCode ec = m_system->logoutUser(m_currentUser);
    
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("User logged out successfully: " + m_currentUser);
        m_currentUser.clear();
        return buildResponse(ec, {"Logout successful"});
    } else if (ec == ErrorCode::USER_NOT_LOGIN) {
        g_logger.warning("User not logged in: " + m_currentUser);
        return buildResponse(ec, {"User not logged in"});
    } else if (ec == ErrorCode::USER_NOT_FOUND) {
        g_logger.warning("User not found: " + m_currentUser);
        return buildResponse(ec, {"User not found"});
    } else {
        return buildResponse(ec, {"Logout failed"});
    }
}

std::string ClientHandler::handleLogin(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        g_logger.warning("Login command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }

    std::string username = args[0];
    std::string password = args[1];
    int typeInt = 0;
    if (!parseInt(args[2], typeInt) || typeInt < 0 || typeInt > static_cast<int>(UserType::ADMINISTRATOR)) {
        g_logger.warning("Login command: Invalid user type: " + args[2]);
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid user type"});
    }
    UserType type = static_cast<UserType>(typeInt);
    
    g_logger.info("Login attempt for user: " + username + " type: " + std::to_string(typeInt));
    
    ErrorCode ec = m_system->loginUser(username, password, type);
    
    if (ec == ErrorCode::SUCCESS) {
        m_currentUser = username;
        m_userType = type;
        g_logger.info("User logged in successfully: " + m_currentUser);
        return buildResponse(ec, {m_currentUser, std::to_string(typeInt)});
    } else {
        g_logger.warning("Login failed for user: " + username);
        return buildResponse(ec, {"Login failed"});
    }
}

std::string ClientHandler::handleSendParcel(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        g_logger.warning("SendParcel command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }
    std::string receiver = args[0];
    int ptype = 0;
    if (!parseInt(args[1], ptype)) {
        g_logger.warning("SendParcel command: Invalid parcel type: " + args[1]);
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid parcel type"});
    }
    double weight = 0.0;
    if (!parseDouble(args[2], weight)) {
        g_logger.warning("SendParcel command: Invalid weight: " + args[2]);
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid weight"});
    }
    std::string desc = args[3];
    ParcelType type = static_cast<ParcelType>(ptype);
    
    g_logger.info("SendParcel attempt - Sender: " + m_currentUser + 
                    ", Receiver: " + receiver + ", Type: " + std::to_string(ptype) + 
                    ", Weight: " + std::to_string(weight));
    
    auto [ec, pid] = m_system->sendParcel(m_currentUser, receiver, type, weight, desc);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Parcel sent successfully - ID: " + pid + ", Sender: " + m_currentUser);
        return buildResponse(ec, {pid});
    } else {
        g_logger.warning("SendParcel failed - Sender: " + m_currentUser + 
                        ", Receiver: " + receiver + " (receiver not found or insufficient balance)");
        return buildResponse(ec, {"Failed to send parcel"});
    }
}

std::string ClientHandler::handleAssignParcel(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        g_logger.warning("AssignParcel command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }
    std::string parcelId = args[0];
    std::string courier = args[1];
    
    g_logger.info("AssignParcel attempt - Admin: " + m_currentUser + 
                    ", Parcel: " + parcelId + ", Courier: " + courier);
    
    ErrorCode ec = m_system->assignCourier(parcelId, courier);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Parcel assigned successfully - Parcel: " + parcelId + ", Courier: " + courier);
        return buildResponse(ec, {"Assigned"});
    } else {
        g_logger.warning("Parcel assignment failed - Parcel: " + parcelId + ", Courier: " + courier);
        return buildResponse(ec, {"Assignment failed"});
    }
}

std::string ClientHandler::handleCollectParcel(const std::vector<std::string>& args) {
    if (args.empty()) {
        g_logger.warning("CollectParcel command: No parcel IDs provided");
        return buildResponse(ErrorCode::INVALID_ARGS, {"No parcel IDs"});
    }
    std::vector<std::string> ids = args;
    
    g_logger.info("CollectParcel attempt - Courier: " + m_currentUser + 
                    ", Parcel IDs count: " + std::to_string(ids.size()));
    
    auto [ec, collected] = m_system->collectParcels(m_currentUser, ids);
    if (ec == ErrorCode::SUCCESS) {
        std::string list;
        for (size_t i = 0; i < collected.size(); ++i) {
            if (i > 0) list += DELIMITER;
            list += collected[i];
        }
        g_logger.info("Parcels collected successfully - Courier: " + m_currentUser + 
                        ", Collected count: " + std::to_string(collected.size()));
        return buildResponse(ec, {list});
    }
    g_logger.warning("No parcels collected - Courier: " + m_currentUser);
    return buildResponse(ec, {"No parcels collected"});
}

std::string ClientHandler::handleSignParcel(const std::vector<std::string>& args) {
    if (args.empty()) {
        g_logger.warning("SignParcel command: No parcel IDs provided");
        return buildResponse(ErrorCode::INVALID_ARGS, {"No parcel IDs"});
    }
    std::vector<std::string> ids = args;
    
    g_logger.info("SignParcel attempt - User: " + m_currentUser + 
                    ", Parcel IDs count: " + std::to_string(ids.size()));
    
    auto [ec, signedList] = m_system->signParcels(m_currentUser, ids);
    if (ec == ErrorCode::SUCCESS) {
        std::string list;
        for (size_t i = 0; i < signedList.size(); ++i) {
            if (i > 0) list += DELIMITER;
            list += signedList[i];
        }
        g_logger.info("Parcels signed successfully - User: " + m_currentUser + 
                        ", Signed count: " + std::to_string(signedList.size()));
        return buildResponse(ec, {list});
    }
    g_logger.warning("No parcels signed - User: " + m_currentUser);
    return buildResponse(ec, {"No parcels signed"});
}

std::string ClientHandler::handleQueryParcel(const std::vector<std::string>& args) {
    g_logger.info("QueryParcel request - User: " + m_currentUser + 
                    ", UserType: " + std::to_string(static_cast<int>(m_userType)));
    
    if (args.size() < 7) {
        g_logger.warning("QueryParcel command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }

    std::string parcelId = args[0];
    std::string sender = args[1];
    std::string receiver = args[2];
    std::string courier = args[3];
    int statusInt = 0;
    if (!parseInt(args[4], statusInt) || statusInt < 0 || statusInt > static_cast<int>(ParcelStatus::OTHER)) {
        g_logger.warning("QueryParcel command: invalid status value: " + args[4]);
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid status value"});
    }
    long long startValue = 0;
    long long endValue = 0;
    if (!parseLongLong(args[5], startValue) || !parseLongLong(args[6], endValue)) {
        g_logger.warning("QueryParcel command: invalid time values: " + args[5] + ", " + args[6]);
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid time values"});
    }
    ParcelStatus status = static_cast<ParcelStatus>(statusInt);
    time_t startTime = static_cast<time_t>(startValue);
    time_t endTime = static_cast<time_t>(endValue);

    g_logger.info("QueryParcel attempt - User: " + m_currentUser + 
                    ", Parcel ID: " + parcelId + ", Sender: " + sender + ", Receiver: " + receiver + ", Courier: " + courier + ", Status: " + std::to_string(statusInt) + ", Start Time: " + std::to_string(startTime) + ", End Time: " + std::to_string(endTime));
    
    std::vector<Parcel*> parcels;
    ErrorCode ec;
    
    if (m_userType == UserType::ADMINISTRATOR) {
        ec = m_system->queryParcels(parcels, parcelId, sender, receiver, "", status, startTime, endTime);
        if (ec != ErrorCode::SUCCESS) {
            return buildResponse(ec, {"Query failed"});
        }
    } else if (m_userType == UserType::COURIER) {
        ec = m_system->queryParcels(parcels, parcelId, sender, receiver, m_currentUser, status, startTime, endTime);
        if (ec != ErrorCode::SUCCESS) {
            return buildResponse(ec, {"Query failed"});
        }
    } else {
        if (sender.empty() && receiver.empty()) {
            std::vector<Parcel*> q1Vec, q2Vec;
            ec = m_system->queryParcels(q1Vec, parcelId, m_currentUser, "", "", status, startTime, endTime);
            if (ec != ErrorCode::SUCCESS) {
                return buildResponse(ec, {"Query failed"});
            }
            ec = m_system->queryParcels(q2Vec, parcelId, "", m_currentUser, "", status, startTime, endTime);
            if (ec != ErrorCode::SUCCESS) {
                return buildResponse(ec, {"Query failed"});
            }
            parcels = q1Vec;
            parcels.insert(parcels.end(), q2Vec.begin(), q2Vec.end());
        } else {
            if (!sender.empty() && sender != m_currentUser && !receiver.empty() && receiver != m_currentUser) {
                g_logger.warning("QueryParcel command: Sender and receiver must be the current user");
                return buildResponse(ErrorCode::INVALID_ARGS, {"Sender and receiver must be the current user"});
            }

            if (sender.empty() && m_currentUser != receiver) sender = m_currentUser;
            if (receiver.empty() && m_currentUser != sender) receiver = m_currentUser;
            
            ec = m_system->queryParcels(parcels, parcelId, sender, receiver, "", status, startTime, endTime);
            if (ec != ErrorCode::SUCCESS) {
                return buildResponse(ec, {"Query failed"});
            }
        }
    }
    
    g_logger.debug("QueryParcel result - User: " + m_currentUser + 
                    ", Parcel count: " + std::to_string(parcels.size()));
    
    std::string data;
    for (size_t i = 0; i < parcels.size(); ++i) {
        if (i > 0) data += DELIMITER;
        data += parcels[i]->serialize();
    }
    return buildResponse(ec, {data});
}

std::string ClientHandler::handleRechargeBalance(const std::vector<std::string>& args) {
    if (args.size() < 1) {
        g_logger.warning("RechargeBalance command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }

    double amount = 0.0;
    if (!parseDouble(args[0], amount)) {
        g_logger.warning("RechargeBalance command: Invalid amount: " + args[0]);
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid amount"});
    }
    g_logger.info("RechargeBalance attempt - User: " + m_currentUser + ", Amount: " + std::to_string(amount));
    if (amount <= 0.0) {
        g_logger.warning("RechargeBalance command: Invalid amount: " + args[0]);
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid amount"});
    }
    ErrorCode ec = m_system->rechargeUser(m_currentUser, amount);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("User recharge successful - User: " + m_currentUser + ", Amount: " + std::to_string(amount));
        return buildResponse(ec, {"Recharge successful"});
    }
    g_logger.warning("RechargeBalance failed - User: " + m_currentUser);
    return buildResponse(ec, {"Recharge failed"});
}

std::string ClientHandler::handleQueryBalance(const std::vector<std::string>& args) {
    double balance = 0.0;
    ErrorCode ec = m_system->getUserBalance(m_currentUser, balance);
    if (ec != ErrorCode::SUCCESS) {
        g_logger.warning("QueryBalance failed - User not found: " + m_currentUser);
        return buildResponse(ec, {"User not found"});
    }
    g_logger.info("QueryBalance request - User: " + m_currentUser + ", Balance: " + std::to_string(balance));
    return buildResponse(ec, {std::to_string(balance)});
}

std::string ClientHandler::handleChangePassword(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        g_logger.warning("ChangePassword command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }
    std::string oldPwd = args[0];
    std::string newPwd = args[1];
    g_logger.info("ChangePassword attempt - User: " + m_currentUser + ", password change requested");

    ErrorCode ec = m_system->changeUserPassword(m_currentUser, oldPwd, newPwd);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Password changed successfully for user: " + m_currentUser);
        return buildResponse(ec, {"Password changed"});
    }
    g_logger.warning("ChangePassword failed for user: " + m_currentUser);
    return buildResponse(ec, {"Password change failed"});
}

std::string ClientHandler::handleDeleteAccount(const std::vector<std::string>& args) {
    if (args.size() < 1) {
        g_logger.warning("DeleteAccount command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }
    
    std::string targetUsername = args[0];
    
    g_logger.info("DeleteAccount attempt - Admin: " + m_currentUser + 
                    ", Target: " + targetUsername);
    
    ErrorCode ec = m_system->deleteUser(targetUsername);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Account deleted successfully - Admin: " + m_currentUser + 
                        ", Target: " + targetUsername);
        return buildResponse(ec, {"Account deleted successfully"});
    } else {
        g_logger.warning("DeleteAccount failed - Admin: " + m_currentUser + 
                        ", Target: " + targetUsername + " (user not found, has unfinished parcels, or self-deletion attempt)");
        return buildResponse(ec, {"Delete failed"});
    }
}

std::string ClientHandler::handleQueryUser(const std::vector<std::string>& args) {    
    if (args.size() < 2) {
        g_logger.warning("QueryUser command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }
    
    std::string username = args[0];
    int typeInt = 0;
    if (!parseInt(args[1], typeInt) || typeInt < 0 || typeInt > static_cast<int>(UserType::ADMINISTRATOR)) {
        g_logger.warning("QueryUser command: Invalid user type: " + args[1]);
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid user type"});
    }
    UserType userType = static_cast<UserType>(typeInt);

    if (m_userType == UserType::CUSTOMER || m_userType == UserType::COURIER) {
        if (username != m_currentUser) {
            g_logger.warning("QueryUser command: Permission denied for user: " + 
                m_currentUser);
            return buildResponse(ErrorCode::INVALID_ARGS, {"Only administrators can query other users"});
        } else if (userType != m_userType) {
            g_logger.warning("QueryUser command: Invalid user type: " + std::to_string(typeInt));
            return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid user type"});
        } else {
            std::string userTypeStr = m_userType == UserType::CUSTOMER ? "Customer" : "Courier";
            g_logger.info("QueryUser request - " + userTypeStr + " " + username + ", Type: " + std::to_string(typeInt));
        }
    } else if (m_userType == UserType::ADMINISTRATOR) {
        g_logger.info("QueryUser request - Admin: " + m_currentUser + ", User: " + username + ", Type: " + std::to_string(typeInt));
    }
    
    ErrorCode ec = ErrorCode::SUCCESS;
    std::vector<User*> users;
    ec = m_system->getUsers(users, username, userType);
    if (ec != ErrorCode::SUCCESS) {
        g_logger.warning("QueryUser failed - User not found: " + username);
        return buildResponse(ec, {"User not found"});
    }
    std::string data;
    for (size_t i = 0; i < users.size(); ++i) {
        if (!data.empty()) data += DELIMITER;
        data += users[i]->serialize();
    }
    
    g_logger.debug("QueryUser result - Admin: " + m_currentUser + 
                    ", User count: " + std::to_string(users.size()));
    
    return buildResponse(ec, {data});
}

std::string ClientHandler::handleDeleteParcel(const std::vector<std::string>& args) {
    if (args.size() < 1) {
        g_logger.warning("DeleteParcel command: Invalid arguments count: " + std::to_string(args.size()));
        return buildResponse(ErrorCode::INVALID_ARGS, {"Invalid arguments"});
    }
    
    std::string parcelId = args[0];
    ErrorCode ec = m_system->deleteParcel(parcelId);
    if (ec == ErrorCode::SUCCESS) {
        g_logger.info("Parcel deleted successfully - Parcel ID: " + parcelId);
        return buildResponse(ec, {"Parcel deleted successfully"});
    } else {
        g_logger.warning("DeleteParcel failed - Parcel ID: " + parcelId);
        return buildResponse(ec, {"Delete failed: parcel not found"});
    }
}

std::string ClientHandler::handleGetStatistics(const std::vector<std::string>& args) {
    g_logger.info("GetStatistics request - Admin: " + m_currentUser);  
    int totalUsers = 0, totalParcels = 0, pendingCollection = 0, collected = 0, Signed = 0;
    double adminTotalBalance = 0.0;
    ErrorCode ec = m_system->getStatistics(totalUsers, totalParcels, pendingCollection, collected, Signed, adminTotalBalance);
    std::vector<std::string> data;
    if (ec != ErrorCode::SUCCESS) {
        return buildResponse(ec, {"No statistics available"});
    }
    try {
        data.push_back(std::to_string(totalUsers));
        data.push_back(std::to_string(totalParcels));
        data.push_back(std::to_string(pendingCollection));
        data.push_back(std::to_string(collected));
        data.push_back(std::to_string(Signed));
        data.push_back(std::to_string(adminTotalBalance));
    } catch (const std::out_of_range& e) {
        return buildResponse(ec, {"Statistics key not found"});
    }
    return buildResponse(ec, data);
}
