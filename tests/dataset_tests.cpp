#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "FileManager.h"
#include "Logger.h"

Logger g_logger;

static std::optional<std::filesystem::path> findDataDir() {
    using namespace std::filesystem;
    path candidates[] = {
        current_path() / "tests" / "data",
        current_path() / ".." / "tests" / "data",
        current_path() / ".." / ".." / "tests" / "data",
        current_path() / "data",
        current_path() / ".." / "data"
    };

    for (const path& p : candidates) {
        if (exists(p) && is_directory(p)) {
            return p;
        }
    }
    return std::nullopt;
}

static void testLoadUsersFromFullDataset(const std::filesystem::path& baseDir) {
    auto userFile = baseDir / "full_users.dat";
    assert(std::filesystem::exists(userFile));

    int version = -1;
    auto users = FileManager::loadUsers(userFile.string(), version);
    assert(version == 1);
    assert(users.size() == 4);
    assert(users.count("alice") == 1);
    assert(users.count("bob") == 1);
    assert(users.count("admin") == 1);
    assert(users.count("charlie") == 1);
    assert(users["bob"]->getUsername() == "bob");
    assert(users["alice"]->getBalance() == 120.5);
    assert(users["admin"]->getAddress() == "Central Office");
    for (auto& pair : users) {
        delete pair.second;
    }
}

static void testLoadParcelsFromFullDataset(const std::filesystem::path& baseDir) {
    auto parcelFile = baseDir / "full_parcels.dat";
    assert(std::filesystem::exists(parcelFile));

    int version = -1;
    auto parcels = FileManager::loadParcels(parcelFile.string(), version);
    assert(version == 1);
    assert(parcels.size() == 3);
    assert(parcels.count("PCL1001") == 1);
    assert(parcels.count("PCL1002") == 1);
    assert(parcels.count("PCL1003") == 1);
    assert(parcels["PCL1002"]->getStatus() == ParcelStatus::SIGNED);
    assert(parcels["PCL1003"]->getDescription() == "books");
    assert(parcels["PCL1001"]->getCourierName() == "bob");
    for (auto& pair : parcels) {
        delete pair.second;
    }
}

static void testLoadConfigFromFullDataset(const std::filesystem::path& baseDir) {
    auto configFile = baseDir / "full_config.dat";
    assert(std::filesystem::exists(configFile));

    int version = -1;
    double balance = FileManager::loadConfig(configFile.string(), version);
    assert(version == 1);
    assert(balance == 7390.25);
}

int main() {
    try {
        auto dataDir = findDataDir();
        if (!dataDir) {
            std::cerr << "Data directory not found. Expected under tests/data." << std::endl;
            return 1;
        }

        testLoadUsersFromFullDataset(*dataDir);
        testLoadParcelsFromFullDataset(*dataDir);
        testLoadConfigFromFullDataset(*dataDir);
    } catch (const std::exception& ex) {
        std::cerr << "Dataset test failed with exception: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "All dataset tests passed." << std::endl;
    return 0;
}
