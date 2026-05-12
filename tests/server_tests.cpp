#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Logger.h"
#include "FileManager.h"
#include "LogisticsSystem.h"

Logger g_logger;

static void cleanupFile(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
}

static void testFileManagerPersistence() {
    namespace fs = std::filesystem;
    fs::path userFile = fs::current_path() / "file_manager_users.dat";
    fs::path parcelFile = fs::current_path() / "file_manager_parcels.dat";
    fs::path configFile = fs::current_path() / "file_manager_config.dat";

    cleanupFile(userFile);
    cleanupFile(parcelFile);
    cleanupFile(configFile);

    std::map<std::string, User*> users;
    users["alice"] = new Customer("alice", "alicepwd", "Alice", "111", "Street A", 50.0);
    users["bob"] = new Courier("bob", "bobpwd", "Bob", "222", "Street B", 10.0);

    assert(FileManager::saveUsers(userFile.string(), users));
    auto loadedUsers = FileManager::loadUsers(userFile.string());
    assert(loadedUsers.size() == users.size());
    assert(loadedUsers.find("alice") != loadedUsers.end());
    assert(loadedUsers.find("bob") != loadedUsers.end());
    assert(loadedUsers["alice"]->getUsername() == "alice");
    assert(loadedUsers["bob"]->getUsername() == "bob");

    for (auto& pair : users) delete pair.second;
    for (auto& pair : loadedUsers) delete pair.second;

    std::map<std::string, Parcel*> parcels;
    auto* parcel = new NormalParcel("PID001", "alice", "bob", 3.5, "documents");
    parcel->setStatus(ParcelStatus::PENDING_COLLECTION);
    parcel->setReceiveTime(0);
    parcel->setCourier("bob");
    parcels[parcel->getParcelId()] = parcel;

    assert(FileManager::saveParcels(parcelFile.string(), parcels));
    auto loadedParcels = FileManager::loadParcels(parcelFile.string());
    assert(loadedParcels.size() == parcels.size());
    assert(loadedParcels.find("PID001") != loadedParcels.end());
    assert(loadedParcels["PID001"]->getSenderName() == "alice");
    assert(loadedParcels["PID001"]->getReceiverName() == "bob");
    assert(loadedParcels["PID001"]->getCourierName() == "bob");

    for (auto& pair : parcels) delete pair.second;
    for (auto& pair : loadedParcels) delete pair.second;

    assert(FileManager::saveConfig(configFile.string(), 1234.5));
    double loadedBalance = FileManager::loadConfig(configFile.string());
    assert(loadedBalance == 1234.5);

    cleanupFile(userFile);
    cleanupFile(parcelFile);
    cleanupFile(configFile);
}

static void testLogisticsSystemOperations() {
    namespace fs = std::filesystem;
    fs::path userFile = fs::current_path() / "logistics_users.dat";
    fs::path parcelFile = fs::current_path() / "logistics_parcels.dat";
    fs::path configFile = fs::current_path() / "logistics_config.dat";

    cleanupFile(userFile);
    cleanupFile(parcelFile);
    cleanupFile(configFile);

    LogisticsSystem system(userFile.string(), parcelFile.string(), configFile.string(), false);
    assert(system.registerUser("alice", "pwd", "Alice", "123", "Addr", UserType::CUSTOMER) == ErrorCode::SUCCESS);
    assert(system.registerUser("charlie", "pwd", "Charlie", "456", "Addr", UserType::CUSTOMER) == ErrorCode::SUCCESS);
    assert(system.registerUser("bob", "pwd", "Bob", "789", "Addr", UserType::COURIER) == ErrorCode::SUCCESS);

    assert(system.rechargeUser("alice", 100.0) == ErrorCode::SUCCESS);
    auto [ecParcel, parcelId] = system.sendParcel("alice", "charlie", ParcelType::NORMAL, 2.0, "books");
    assert(ecParcel == ErrorCode::SUCCESS && !parcelId.empty());

    auto [ecQuery, queryResult] = system.queryParcels(parcelId);
    assert(ecQuery == ErrorCode::SUCCESS && queryResult.size() == 1);
    assert(queryResult[0]->getReceiverName() == "charlie");

    assert(system.assignCourier(parcelId, "bob") == ErrorCode::SUCCESS);
    auto [ecCollected, collected] = system.collectParcels("bob", {parcelId});
    assert(ecCollected == ErrorCode::SUCCESS && collected.size() == 1);
    auto [ecSigned, signedList] = system.signParcels("charlie", {parcelId});
    assert(ecSigned == ErrorCode::SUCCESS && signedList.size() == 1);
    assert(system.deleteParcel(parcelId) == ErrorCode::SUCCESS);

    assert(system.changeUserPassword("alice", "pwd", "newpwd") == ErrorCode::SUCCESS);
    auto [ecLogin, user] = system.loginUser("alice", "newpwd", UserType::CUSTOMER);
    assert(ecLogin == ErrorCode::SUCCESS && user != nullptr);

    auto [ecStats, stats] = system.getStatistics();
    assert(ecStats == ErrorCode::SUCCESS && stats["totalUsers"] >= 3.0);

    cleanupFile(userFile);
    cleanupFile(parcelFile);
    cleanupFile(configFile);
}

int main() {
    try {
        testFileManagerPersistence();
        testLogisticsSystemOperations();
    } catch (const std::exception& ex) {
        std::cerr << "Test failed with exception: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "All server tests passed." << std::endl;
    return 0;
}
