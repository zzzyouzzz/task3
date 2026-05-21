#pragma once
#include "Communication.h"
#include <QMessageBox>
#include <QWidget>

// 尝试断线重连 + 自动登录，成功/失败均弹出提示，返回是否成功
inline bool tryReconnect(Communication* system, QWidget* parent) {
    if (!system) return false;
    QMessageBox::information(parent, "连接断开",
        "与服务器的连接已断开，正在尝试重新连接...");
    if (system->reconnectAndRelogin()) {
        QMessageBox::information(parent, "重连成功",
            "已成功重新连接服务器");
        return true;
    }
    QMessageBox::critical(parent, "重连失败",
        "无法重新连接服务器，请检查网络");
    return false;
}
