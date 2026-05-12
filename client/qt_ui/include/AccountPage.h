#pragma once
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QComboBox>
#include <string>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QHeaderView>
#include <QInputDialog>
#include <string>
#include "Communication.h"
// ------------------------------
// 账户页面
// ------------------------------
class AccountPage : public QWidget {
    QTableWidget* infoTable;      // 信息表格
    QLabel* balanceLabel;         // 余额显示标签
    std::string username;          // 当前用户
    Communication* system;        // 通信对象
    Q_OBJECT
public:
    AccountPage(QWidget *p=nullptr, std::string username="", Communication* sys=nullptr);
    void refresh() { load_user_info(); }
private slots:
    void load_user_info();        // 加载用户信息
    void update_profile();        // 更新个人资料
    void recharge(unsigned int amount); // 充值指定金额
    void show_recharge_dialog();  // 弹出充值对话框
    void change_password();       // 修改密码
};