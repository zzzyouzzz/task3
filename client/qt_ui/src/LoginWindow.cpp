#include "LoginWindow.h"

LoginWindow::LoginWindow(Communication* sys, const std::string& ip, int port, QWidget *parent) : QMainWindow(parent), currentRole(UserType::CUSTOMER), system(sys), ownSystem(false), m_ip(ip), m_port(port) {
    if (!system) {
        system = new Communication();
        ownSystem = true;
    }
    setWindowTitle("系统登录");
    setFixedSize(520,420);
    auto c = new QWidget; setCentralWidget(c);
    auto lay = new QVBoxLayout(c);
    lay->setSpacing(20);
    lay->setContentsMargins(40,40,40,40);
    
    // 其余界面构造与默认构造函数相同
    auto title = new QLabel("快递管理系统");
    title->setStyleSheet("font-size:24px; font-weight:bold; color:#2C3E50;");
    title->setAlignment(Qt::AlignCenter);

    auto roleLayout = new QHBoxLayout;
    auto userRoleBtn = new QPushButton("👤 用户");
    auto courierRoleBtn = new QPushButton("🚚 快递员");
    auto adminRoleBtn = new QPushButton("🔧 管理员");
    QString roleBtnStyle = R"(
        QPushButton{background:#ECF0F1; color:#2C3E50; border:2px solid #2C3E50; border-radius:6px; padding:10px; font-size:14px;}
        QPushButton:hover{background:#3498DB; color:white;}
    )";
    userRoleBtn->setStyleSheet(roleBtnStyle);
    courierRoleBtn->setStyleSheet(roleBtnStyle);
    adminRoleBtn->setStyleSheet(roleBtnStyle);
    roleLayout->addWidget(userRoleBtn);
    roleLayout->addWidget(courierRoleBtn);
    roleLayout->addWidget(adminRoleBtn);
    roleLabel = new QLabel("请选择登录角色");
    roleLabel->setStyleSheet("font-size:14px; color:#666; padding:8px; background:#F0F4F8; border-radius:4px;");
    roleLabel->setAlignment(Qt::AlignCenter);
    auto form = new QFormLayout;
    usernameEdit = new QLineEdit;
    usernameEdit->setPlaceholderText("请输入用户名");
    passwordEdit = new QLineEdit;
    passwordEdit->setPlaceholderText("请输入密码");
    passwordEdit->setEchoMode(QLineEdit::Password);
    QString editStyle = R"(
        QLineEdit{border:1px solid #DCDFE6; border-radius:6px; padding:10px; font-size:14px;}
    )";
    usernameEdit->setStyleSheet(editStyle);
    passwordEdit->setStyleSheet(editStyle);
    form->addRow("用户名：", usernameEdit);
    form->addRow("密码：", passwordEdit);

    auto buttonLayout = new QHBoxLayout;
    auto loginBtn = new QPushButton("🔐 登录");
    loginBtn->setStyleSheet(R"(
        QPushButton{background:#3498DB; color:white; border:none; border-radius:6px; padding:12px; font-size:16px; font-weight:bold;}
        QPushButton:hover{background:#2980B9;}
    )");

    auto registerBtn = new QPushButton("📝 注册新账号");
    registerBtn->setStyleSheet(R"(
        QPushButton{background:#27AE60; color:white; border:none; border-radius:6px; padding:12px; font-size:16px; font-weight:bold;}
        QPushButton:hover{background:#229954;}
    )");
    buttonLayout->addWidget(loginBtn);
    buttonLayout->addWidget(registerBtn);
    auto reconnectBtn = new QPushButton("🔁 重连");
    reconnectBtn->setStyleSheet(R"(
        QPushButton{background:#95A5A6; color:white; border:none; border-radius:6px; padding:12px; font-size:14px;}
        QPushButton:hover{background:#7F8C8D;}
    )");
    buttonLayout->addWidget(reconnectBtn);
    lay->addWidget(title);
    lay->addWidget(roleLabel);
    lay->addLayout(roleLayout);
    lay->addLayout(form , 2);
    lay->addLayout(buttonLayout);
    lay->addStretch();
    connect(userRoleBtn, &QPushButton::clicked, this, [=](){
        currentRole = UserType::CUSTOMER;
        roleLabel->setText("当前角色：用户");
        roleLabel->setStyleSheet("font-size:14px; color:#2C3E50; padding:8px; background:#E8F5E9; border-radius:4px;");
    });
    connect(courierRoleBtn, &QPushButton::clicked, this, [=](){
        currentRole = UserType::COURIER;
        roleLabel->setText("当前角色：快递员");
        roleLabel->setStyleSheet("font-size:14px; color:#27AE60; padding:8px; background:#D4EDDA; border-radius:4px;");
    });
    connect(adminRoleBtn, &QPushButton::clicked, this, [=](){
        currentRole = UserType::ADMINISTRATOR;
        roleLabel->setText("当前角色：管理员");
        roleLabel->setStyleSheet("font-size:14px; color:#E67E22; padding:8px; background:#FADBD8; border-radius:4px;");
    });
    connect(loginBtn, &QPushButton::clicked, this, [=](){ perform_login(); });

    connect(registerBtn, &QPushButton::clicked, this, [=](){
        RegisterWindow dlg(system, this);
        dlg.exec();
    });
    connect(reconnectBtn, &QPushButton::clicked, this, [=](){
        attemptReconnect();
    });
}

LoginWindow::~LoginWindow() {
    if (ownSystem) {
        delete system;
    }
}

void LoginWindow::perform_login() {
    QString username = usernameEdit->text().trimmed();
    QString password = passwordEdit->text().trimmed();
    
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "提示", "用户名和密码不能为空");
        return;
    }
    
    // 保存凭据以便重连时重试登录
    last_username = username.toStdString();
    last_password = password.toStdString();
    last_type = currentRole;

    // 使用 Communication 登录
    std::string userId;
    ErrorCode res = system->loginUser(username.toStdString(), password.toStdString(), currentRole, userId);
    if (res != ErrorCode::SUCCESS) {
        QString msg;
        switch (res) {
            case ErrorCode::USER_NOT_FOUND:
                msg = "用户 " + username + " 不存在";
                break;
            case ErrorCode::LOGIN_FAILED:
                msg = "密码错误或身份不匹配";
                break;
            case ErrorCode::INTERNAL_ERROR:
                // 尝试重连并重试登录一次
                if (attemptReconnect()) {
                    ErrorCode r2 = system->loginUser(last_username, last_password, last_type, userId);
                    if (r2 == ErrorCode::SUCCESS) {
                        QMessageBox::information(this, "登录成功", 
                            QString("欢迎回来，%1！\n角色：%2")
                                .arg(QString::fromStdString(userId))
                                .arg(currentRole == UserType::CUSTOMER ? "用户" : 
                                     currentRole == UserType::COURIER ? "快递员" : "管理员"));
                        if (currentRole == UserType::CUSTOMER) {
                            (new UserWindow(last_username, system))->show();
                        } else if (currentRole == UserType::COURIER) {
                            (new CourierWindow(last_username, system))->show();
                        } else {
                            (new AdminWindow(last_username, system))->show();
                        }
                        this->close();
                        return;
                    }
                }
                msg = "登录失败，请重试";
                break;
            default:
                msg = "登录失败，请重试";
                break;
        }
        QMessageBox::critical(this, "登录失败", msg);
        return;
    }
    
    
    QMessageBox::information(this, "登录成功", 
        QString("欢迎回来，%1！\n角色：%2")
            .arg(QString::fromStdString(userId))
            .arg(currentRole == UserType::CUSTOMER ? "用户" : 
                 currentRole == UserType::COURIER ? "快递员" : "管理员"));
    
    // 根据角色打开对应窗口
    if (currentRole == UserType::CUSTOMER) {
        (new UserWindow(username.toStdString(), system))->show();
    } else if (currentRole == UserType::COURIER) {
        (new CourierWindow(username.toStdString(), system))->show();
    } else {
        (new AdminWindow(username.toStdString(), system))->show();
    }
    this->close();
}

bool LoginWindow::attemptReconnect() {
    if (!m_ip.empty() && m_port > 0) {
        if (!system) {
            system = new Communication();
            ownSystem = true;
        } else {
            system->disconnect();
        }
        if (system->connectToServer(m_ip, m_port)) {
            QMessageBox::information(this, "重连成功", "已连接到服务器");
            return true;
        } else {
            QMessageBox::critical(this, "重连失败", "无法连接到服务器，请检查配置和网络");
            return false;
        }
    } else {
        QMessageBox::warning(this, "重连失败", "未配置服务器地址或端口");
        return false;
    }
}