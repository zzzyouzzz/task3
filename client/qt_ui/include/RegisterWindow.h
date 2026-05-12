#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QComboBox>
#include <QMessageBox>
#include <QVBoxLayout>
#include "Communication.h"

class RegisterWindow : public QDialog {
    Q_OBJECT

    QLineEdit* usernameEdit;      // 用户名
    QLineEdit* passwordEdit;      // 密码
    QLineEdit* confirmEdit;       // 确认密码
    QLineEdit* nameEdit;          // 真实姓名
    QLineEdit* phoneEdit;         // 联系电话
    QLineEdit* addressEdit;       // 地址
    QComboBox* roleCombo;         // 角色选择
    QLabel* errorLabel;           // 错误提示标签
    QLabel* infoTitleLabel;       // 信息标题
    QLabel* infoContentLabel;     // 信息内容
    Communication* system;        // 通信对象

public:
    RegisterWindow(Communication* sys, QWidget* parent = nullptr);

private slots:
    void submit_registration();
};