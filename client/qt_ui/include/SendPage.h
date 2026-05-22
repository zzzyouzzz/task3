#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <string>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QMessageBox>
#include "QueryPage.h"
#include "AccountPage.h"
#include "Communication.h"
// ------------------------------
// 发送页面
// ------------------------------
class SendPage : public QWidget {
    QLineEdit* editReceiver;      // 收件人输入框
    QLineEdit* editPhone;         // 联系电话输入框
    QLineEdit* editAddr;          // 收货地址输入框
    QComboBox* typeBox;           // 快递类型下拉框
    QTextEdit* contentEdit;       // 物品描述输入框
    QDoubleSpinBox* weightSpin;   // 重量输入
    std::string username;          // 当前登录用户名
    QueryPage* queryPage;         // 关联的查询页面（刷新用）
    AccountPage* accountPage;     // 关联的账户页面（刷新用）
    Communication* system;        // 通信对象

    Q_OBJECT   
public:
    SendPage(QWidget *parent = nullptr, std::string username = "", Communication* sys = nullptr);   
    void setQueryPage(QueryPage* qp);
    void setAccountPage(AccountPage* ap);
private slots:
    void submit_package();        // 提交寄件订单
};