#include "test_server_helper.h"
#include "Communication.h"
#include "common.h"
#include "User.h"
#include "Parcel.h"

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <string>
#include <memory>
#include <cstdlib>

extern Logger g_logger;

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::cerr << "  [CHECK FAILED] " << #cond << " (line " << __LINE__ << ")\n"; \
        return false; \
    } \
} while(0)

struct ThreadResult {
    int opsCompleted = 0;
    long long elapsedMs = 0;
    bool error = false;
};

// 注册用户并登录，返回已连接的 Communication
static std::unique_ptr<Communication> registerAndLogin(
    const TestServerHelper& helper,
    const std::string& user, const std::string& pass, UserType type)
{
    auto comm = std::make_unique<Communication>();
    if (!comm->connectToServer(helper.getClientIp(), helper.getPort())) {
        std::cerr << "  连接失败: " << user << "\n";
        return nullptr;
    }
    // 尝试注册（忽略 USER_EXISTS 错误）
    ErrorCode ec = comm->registerUser(user, pass, "Test", "000", "Addr", type);
    if (ec != ErrorCode::SUCCESS && ec != ErrorCode::USER_EXISTS) {
        std::cerr << "  注册失败 " << user << ": " << static_cast<int>(ec) << "\n";
        return nullptr;
    }
    std::string userId;
    ec = comm->loginUser(user, pass, type, userId);
    if (ec != ErrorCode::SUCCESS) {
        std::cerr << "  登录失败 " << user << ": " << static_cast<int>(ec) << "\n";
        return nullptr;
    }
    return comm;
}

// ===================== Test 1: 基础多连接 =====================
static bool testBasicMultiConnect(const TestServerHelper& helper) {
    Communication c1, c2, c3;

    CHECK(c1.connectToServer(helper.getClientIp(), helper.getPort()));
    CHECK(c1.isConnected());
    CHECK(c2.connectToServer(helper.getClientIp(), helper.getPort()));
    CHECK(c2.isConnected());
    CHECK(c3.connectToServer(helper.getClientIp(), helper.getPort()));
    CHECK(c3.isConnected());

    std::cout << "  3 个客户端同时连接成功\n";
    // 析构函数自动 disconnect
    return true;
}

// ===================== Test 2: 并发业务操作 =====================
static bool testConcurrentOperations(const TestServerHelper& helper) {
    Communication aliceConn, adminConn, bobConn, charlieConn;

    CHECK(aliceConn.connectToServer(helper.getClientIp(), helper.getPort()));
    CHECK(adminConn.connectToServer(helper.getClientIp(), helper.getPort()));
    CHECK(bobConn.connectToServer(helper.getClientIp(), helper.getPort()));
    CHECK(charlieConn.connectToServer(helper.getClientIp(), helper.getPort()));

    std::string userId;
    ErrorCode ec = aliceConn.loginUser("alice", "pwd123", UserType::CUSTOMER, userId);
    CHECK(ec == ErrorCode::SUCCESS);

    std::string parcelId;
    ec = aliceConn.sendParcel("charlie", ParcelType::NORMAL, 2.0, "concurrent_test", parcelId);
    CHECK(ec == ErrorCode::SUCCESS);
    CHECK(!parcelId.empty());
    parcelId.erase(std::remove(parcelId.begin(), parcelId.end(), '\n'), parcelId.end());
    parcelId.erase(std::remove(parcelId.begin(), parcelId.end(), '\r'), parcelId.end());
    std::cout << "  包裹已创建: " << parcelId << "\n";

    double balance = 0.0;
    ec = aliceConn.queryBalance(balance);
    CHECK(ec == ErrorCode::SUCCESS);
    CHECK(std::abs(balance - 190.0) < 0.001);

    ec = adminConn.loginUser("admin", "admin123", UserType::ADMINISTRATOR, userId);
    CHECK(ec == ErrorCode::SUCCESS);
    ec = adminConn.assignParcel(parcelId, "bob");
    CHECK(ec == ErrorCode::SUCCESS);
    std::cout << "  包裹已分配: " << parcelId << " -> bob\n";

    ec = bobConn.loginUser("bob", "pwd456", UserType::COURIER, userId);
    CHECK(ec == ErrorCode::SUCCESS);
    std::vector<std::string> collectedList;
    ec = bobConn.collectParcels({parcelId}, collectedList);
    CHECK(ec == ErrorCode::SUCCESS);
    CHECK(collectedList.size() == 1);

    ec = bobConn.queryBalance(balance);
    CHECK(ec == ErrorCode::SUCCESS);
    CHECK(std::abs(balance - 105.0) < 0.001);

    ec = charlieConn.loginUser("charlie", "pwd789", UserType::CUSTOMER, userId);
    CHECK(ec == ErrorCode::SUCCESS);
    std::vector<std::string> signedList;
    ec = charlieConn.signParcels({parcelId}, signedList);
    CHECK(ec == ErrorCode::SUCCESS);
    CHECK(signedList.size() == 1);

    ec = charlieConn.queryBalance(balance);
    CHECK(ec == ErrorCode::SUCCESS);
    CHECK(std::abs(balance - 300.0) < 0.001);

    int totalUsers = 0, totalParcels = 0, pending = 0, collected = 0, signedP = 0;
    double adminBalance = 0.0;
    ec = adminConn.getStatistics(totalUsers, totalParcels, pending, collected, signedP, adminBalance);
    CHECK(ec == ErrorCode::SUCCESS);
    CHECK(totalUsers == 4);
    CHECK(totalParcels == 1);
    CHECK(pending == 0);
    CHECK(collected == 0);
    CHECK(signedP == 1);
    CHECK(std::abs(adminBalance - 5005.0) < 0.001);

    // 显式登出，释放服务端登录状态
    aliceConn.logout();
    adminConn.logout();
    bobConn.logout();
    charlieConn.logout();
    return true;
}

// ===================== Test 3: Select 公平性 =====================
static bool testSelectFairness(const TestServerHelper& helper) {
    const int NUM_CLIENTS = 5;
    const int OPS_PER_CLIENT = 20;

    // 注册并登录 5 个不同用户
    std::vector<std::unique_ptr<Communication>> comms;
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        std::string name = "fuser" + std::to_string(i);
        auto c = registerAndLogin(helper, name, "pass", UserType::CUSTOMER);
        if (!c) return false;
        comms.push_back(std::move(c));
    }

    // 在线程中并发执行 queryBalance
    std::vector<ThreadResult> results(NUM_CLIENTS);
    std::vector<std::thread> threads;
    std::atomic<int> startGate(NUM_CLIENTS);

    for (int i = 0; i < NUM_CLIENTS; ++i) {
        threads.emplace_back([&, i]() {
            ThreadResult tr;

            startGate--;
            while (startGate.load() > 0) std::this_thread::yield();

            auto t0 = std::chrono::steady_clock::now();
            for (int j = 0; j < OPS_PER_CLIENT; ++j) {
                double bal = 0.0;
                ErrorCode ec = comms[i]->queryBalance(bal);
                if (ec == ErrorCode::SUCCESS) tr.opsCompleted++;
            }
            auto t1 = std::chrono::steady_clock::now();
            tr.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            results[i] = tr;
        });
    }

    for (auto& t : threads) t.join();

    int totalOps = 0, minOps = OPS_PER_CLIENT, maxOps = 0;
    long long maxElapsed = 0;
    for (const auto& r : results) {
        totalOps += r.opsCompleted;
        minOps = std::min(minOps, r.opsCompleted);
        maxOps = std::max(maxOps, r.opsCompleted);
        maxElapsed = std::max(maxElapsed, r.elapsedMs);
        std::cout << "  客户端 " << (&r - &results[0])
                  << ": " << r.opsCompleted << "/" << OPS_PER_CLIENT << " ops, "
                  << r.elapsedMs << "ms\n";
    }

    CHECK(totalOps > 0);
    CHECK(minOps >= maxOps * 0.8);
    std::cout << "  公平性: min=" << minOps << " max=" << maxOps
              << " (" << (minOps * 100 / maxOps) << "%)\n";
    std::cout << "  最慢客户端: " << maxElapsed << "ms\n";

    // 登出
    for (auto& c : comms) { c->logout(); }
    return true;
}

// ===================== Test 4: 重/轻客户端公平性 =====================
static bool testHeavyVsLight(const TestServerHelper& helper) {
    // 使用不同用户避免单会话限制
    auto heavy = registerAndLogin(helper, "heavy1", "pass", UserType::CUSTOMER);
    auto light = registerAndLogin(helper, "light1", "pass", UserType::CUSTOMER);
    CHECK(heavy != nullptr);
    CHECK(light != nullptr);

    // 充值便于大量发件
    ErrorCode ec = heavy->rechargeBalance(1000.0);
    CHECK(ec == ErrorCode::SUCCESS);

    const int HEAVY_OPS = 40;

    std::atomic<bool> stopLight(false);
    std::atomic<int> lightOps(0);

    // 轻客户端在独立线程中持续查询余额
    std::thread lightThread([&]() {
        while (!stopLight.load()) {
            double bal = 0.0;
            light->queryBalance(bal);
            lightOps++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < HEAVY_OPS; ++i) {
        std::string pid;
        ec = heavy->sendParcel("charlie", ParcelType::NORMAL, 2.0, "heavy_test", pid);
        if (ec != ErrorCode::SUCCESS) {
            std::cerr << "  重客户端第 " << (i + 1) << " 次发件失败\n";
            break;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    auto heavyMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    stopLight = true;
    lightThread.join();

    std::cout << "  重客户端: " << HEAVY_OPS << " 次发件, " << heavyMs << "ms\n";
    std::cout << "  轻客户端: " << lightOps.load() << " 次查询\n";

    CHECK(lightOps.load() > 0);
    CHECK(heavyMs < 10000);

    heavy->logout();
    light->logout();
    return true;
}

// ===================== Test 5: 连接洪泛 =====================
static bool testConnectionBurst(const TestServerHelper& helper) {
    const int NUM_CLIENTS = 10;

    std::vector<std::unique_ptr<Communication>> comms;
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        std::string name = "buser" + std::to_string(i);
        auto c = registerAndLogin(helper, name, "pass", UserType::CUSTOMER);
        if (!c) continue; // 容忍个别失败

        double bal = 0.0;
        ErrorCode ec = c->queryBalance(bal);
        if (ec == ErrorCode::SUCCESS && std::abs(bal) < 0.001) {
            comms.push_back(std::move(c));
        }
    }

    int successCount = static_cast<int>(comms.size());
    std::cout << "  洪泛结果: " << successCount << "/" << NUM_CLIENTS << " 成功\n";
    CHECK(successCount >= 8);
    return true;
}

// ===================== main =====================
int main(int argc, char* argv[]) {
    g_logger.initialize("multi_client_tests.log", LOG_FATAL, false);

    TestServerHelper::Options opts;
    opts.serverPath = "output/server/server.exe";
    opts.workDir = "output/test_multi_run";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto eq = arg.find('=');
        if (eq == std::string::npos) continue;
        std::string key = arg.substr(0, eq);
        std::string val = arg.substr(eq + 1);
        if (key == "--server") opts.serverPath = val;
        else if (key == "--workdir") opts.workDir = val;
        else if (key == "--port") opts.clientPort = std::stoi(val);
        else if (key == "--ip") opts.clientIp = val;
    }

    TestServerHelper helper(opts);

    if (!helper.setup()) { std::cerr << "环境初始化失败\n"; return 1; }
    if (!helper.start()) { helper.cleanup(); return 1; }
    if (!helper.waitForReady()) { helper.stop(); helper.cleanup(); return 1; }

    struct TestCase {
        const char* name;
        bool (*fn)(const TestServerHelper&);
    };
    TestCase tests[] = {
        {"BasicMultiConnect",       testBasicMultiConnect},
        {"ConcurrentOperations",    testConcurrentOperations},
        {"SelectFairness",          testSelectFairness},
        {"HeavyVsLight",            testHeavyVsLight},
        {"ConnectionBurst",         testConnectionBurst},
    };

    int passed = 0, failed = 0;
    for (const auto& t : tests) {
        std::cout << "\n=== " << t.name << " ===\n";
        if (t.fn(helper)) {
            std::cout << "[PASS] " << t.name << "\n";
            passed++;
        } else {
            std::cout << "[FAIL] " << t.name << "\n";
            failed++;
        }
    }

    std::cout << "\n=== MultiClientTests Results: "
              << passed << "/" << (passed + failed) << " passed ===\n";

    helper.stop();
    helper.cleanup();

    return failed > 0 ? 1 : 0;
}
