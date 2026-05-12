#include "Cli.h"
#include "sys.h"
#include <fstream>

int main() {
    std::string ip, filename = "client_config.txt";
    int port = 0;
    if (!loadStartupConfig(filename, ip, port)) {
        std::cerr << "配置文件加载失败。" << std::endl;
        return 1;
    }
    LogisticsClient client(ip, port);
    client.run();
    return 0;
}