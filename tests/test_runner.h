#pragma once
#include <string>
#include <vector>
#include "Communication.h"
#include "common.h"

#ifdef _WIN32
#include <windows.h>
#endif

class TestRunner {
public:
    struct Options {
        std::string inputFile;
        std::string serverPath;
        std::string workDir;
        std::string clientIp = "127.0.0.1";
        int clientPort = 8888;
    };

    TestRunner(const Options& opts);
    ~TestRunner();

    int run();

private:
    // Server management
    void killProcessOnPort(int port);
    bool startServer();
    void stopServer();
    bool waitForServerReady(int maxRetries = 30, int delayMs = 500);

    // Environment setup
    bool setupTestEnvironment();
    void cleanupTestEnvironment();
    bool createTestDataFiles();
    bool createServerConfig();

    // .in file processing
    bool processFile();
    bool executeCommand(const std::string& line, int lineNum);
    std::vector<std::string> split(const std::string& str, char delim);

    // Command handlers
    bool cmdRegister(const std::vector<std::string>& args);
    bool cmdLogin(const std::vector<std::string>& args);
    bool cmdSend(const std::vector<std::string>& args);
    bool cmdAssign(const std::vector<std::string>& args);
    bool cmdCollect(const std::vector<std::string>& args);
    bool cmdSign(const std::vector<std::string>& args);
    bool cmdRecharge(const std::vector<std::string>& args);
    bool cmdChangePwd(const std::vector<std::string>& args);
    bool cmdQueryParcels(const std::vector<std::string>& args);
    bool cmdQueryUsers(const std::vector<std::string>& args);
    bool cmdQueryBalance(const std::vector<std::string>& args);
    bool cmdDeleteUser(const std::vector<std::string>& args);
    bool cmdDeleteParcel(const std::vector<std::string>& args);
    bool cmdGetStats(const std::vector<std::string>& args);
    bool cmdLogout(const std::vector<std::string>& args);
    bool cmdWait(const std::vector<std::string>& args);
    bool cmdCheckFile(const std::vector<std::string>& args);
    bool cmdCheckBalance(const std::vector<std::string>& args);
    bool cmdPrint(const std::vector<std::string>& args);

    // Utilities
    std::string resolveParcelId(const std::string& raw);
    ErrorCode parseErrorCode(const std::string& str);
    std::string errorCodeToString(ErrorCode ec);
    bool expectErrorCode(ErrorCode actual, const std::string& expectedStr);
    bool expectInt(int actual, const std::string& expectedStr);
    bool expectDouble(double actual, const std::string& expectedStr);
    void report(bool passed, const std::string& desc, int lineNum);

    Options m_opts;
    Communication m_comm;

    // Runtime state
    std::string m_lastParcelId;
    std::string m_currentUser;
    UserType m_currentUserType;
    int m_totalTests = 0;
    int m_passedTests = 0;
    int m_failedTests = 0;
    std::string m_origConfigBakPath;

    // Server process
#ifdef _WIN32
    PROCESS_INFORMATION m_serverProcInfo = {0};
    bool m_serverRunning = false;
#endif
};
