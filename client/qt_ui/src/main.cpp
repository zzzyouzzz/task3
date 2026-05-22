#include <QApplication>
#include <fstream>
#include <iostream>
#include <sstream>
#include "LoginWindow.h"
#include "Communication.h"
#include "sys.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    std::string ip, filename = "client_config.txt";
    int port = 0;
    if (!loadStartupConfig(filename, ip, port)) {
        std::cerr << "配置文件加载失败。" << std::endl;
        return 1;
    }

    Communication* system = nullptr;
    try {
        system = new Communication();
        if (!system->connectToServer(ip, port)) {
            g_logger.error("Unable to connect to server at " + ip + ":" + std::to_string(port));
            std::cerr << "连接服务器失败。" << std::endl;
            return 1;
        }
        g_logger.info("Client connected to server at " + ip + ":" + std::to_string(port));
        std::cout << "连接服务器成功！" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "系统异常: " << e.what() << std::endl;
        delete system;
        return 1;
    } catch (...) {
        std::cerr << "未知系统异常" << std::endl;
        delete system;
        return 1;
    }

    LoginWindow login(system, ip, port);
    login.show();
    int result = app.exec();
    if (result == 0) {
        system->logout();
    }
    delete system;
    return result;
}
