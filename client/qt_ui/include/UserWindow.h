#pragma once
#include <string>
#include <QMainWindow>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QListWidget>
#include "new.h"
#include "QueryPage.h"
#include "ReceivePage.h"
#include "SendPage.h"
#include "AccountPage.h"
#include "Communication.h"


// ------------------------------
// 用户主窗口
// ------------------------------
class UserWindow : public QMainWindow {
    std::string username;          // 当前用户名
    Communication* system;        // 通信对象
public:
    UserWindow(std::string username, Communication* sys = nullptr);
};