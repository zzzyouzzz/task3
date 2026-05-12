#include "common.h"

// 构造请求：命令|参数1|参数2|...\n
std::string Protocol::buildRequest(const std::string& cmd, const std::vector<std::string>& args) {
    std::string msg = cmd;
    for (const auto& a : args) {
        msg += DELIMITER + a;
    }
    msg += '\n';
    return msg;
}

// 解析请求报文：按 | 分割，第一个 token 为命令，其余为参数
void Protocol::parseRequest(const std::string& raw, std::string& cmd, std::vector<std::string>& args) {
    cmd.clear();
    args.clear();
    if (raw.empty()) return;
    size_t pos = 0;
    std::string msg = raw;
    if (!msg.empty() && msg.back() == '\n') msg.pop_back();
    while (pos < msg.size()) {
        size_t next = msg.find(DELIMITER, pos);
        if (next == std::string::npos) {//没有找到分隔符
            if (cmd.empty()) cmd = msg.substr(pos);
            else args.push_back(msg.substr(pos));
            break; 
        } else {
            if (cmd.empty()) cmd = msg.substr(pos, next - pos);
            else args.push_back(msg.substr(pos, next - pos));
            pos = next + 1;
        }
    }
}