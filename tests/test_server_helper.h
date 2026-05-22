#pragma once
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// 服务器生命周期管理辅助类
// 封装了 CreateProcess / TerminateProcess / TCP 端口轮询 / 测试数据文件创建
// 从 TestRunner（test_runner.cpp）中提取的独立版本
class TestServerHelper {
public:
    struct Options {
        std::string serverPath;
        std::string workDir;
        std::string serverIp = "0.0.0.0";
        int serverPort = 8888;
        std::string clientIp = "127.0.0.1";
        int clientPort = 8888;
    };

    explicit TestServerHelper(const Options& opts);
    ~TestServerHelper();

    // 创建测试环境 (kill port -> create dir -> write data files + config)
    bool setup();

    // 启动服务器进程
    bool start();

    // 等待服务器就绪 (TCP connect 轮询)
    bool waitForReady(int maxRetries = 30, int delayMs = 500);

    // 停止服务器进程
    void stop();

    // 清理测试目录
    void cleanup();

    // 访问器
    const std::string& getWorkDir() const { return m_opts.workDir; }
    int getPort() const { return m_opts.clientPort; }
    const std::string& getClientIp() const { return m_opts.clientIp; }

private:
    Options m_opts;

#ifdef _WIN32
    PROCESS_INFORMATION m_procInfo = {0};
    bool m_serverRunning = false;
#endif

    void killProcessOnPort(int port);
    bool createTestDataFiles();
    bool createServerConfig();
};
