#pragma once
#include <string>
#include <QMainWindow>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include "CourierPage.h"
#include "QueryPage.h"
#include "Communication.h"

// ------------------------------
// 快递员主窗口
// ------------------------------
class CourierWindow : public QMainWindow {
    std::string courier_id;        // 快递员用户名
    Communication* system;        // 通信对象
public:
    CourierWindow(const std::string& courier_id, Communication* sys = nullptr);
};