#pragma once
#include <QMainWindow>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include "Communication.h"
#include "RegisterWindow.h"
#include "new.h"
#include "UserWindow.h"
#include "CourierWindow.h"
// ------------------------------
// 登录页面（带角色检查）
// ------------------------------
class LoginWindow : public QMainWindow {
    QLineEdit* usernameEdit;      // 用户名输入框
    QLineEdit* passwordEdit;      // 密码输入框
    QLabel* roleLabel;            // 角色提示标签
    QLabel* errorLabel;           // 错误提示标签
    UserType currentRole;         // 当前选中的角色
    Communication* system;        // 通信对象
    bool ownSystem;               // 是否拥有 system 所有权
public:
    LoginWindow(QWidget *parent = nullptr);//界面绘制
    LoginWindow(Communication* sys, QWidget *parent = nullptr);
    ~LoginWindow();
private:
    void perform_login();//登录操作
};