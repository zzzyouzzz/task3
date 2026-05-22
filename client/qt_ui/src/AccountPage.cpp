#include "AccountPage.h"
#include "ReconnectHelper.h"

using namespace std;
AccountPage::AccountPage(QWidget *p, std::string username, Communication* sys):QWidget(p), username(username), system(sys){
    setStyleSheet("background:#F5F7FA;");
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(40,30,40,30);
    lay->setSpacing(18);

    auto title = new QLabel("👤 账户管理");
    title->setStyleSheet("font-size:20px; font-weight:bold; color:#2C3E50;");

    // 余额显示
    auto balanceLayout = new QHBoxLayout;
    balanceLabel = new QLabel("¥ 0.00");
    balanceLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#E74C3C;");
    auto rechargeBtn = new QPushButton("充值");
    rechargeBtn->setStyleSheet("QPushButton{background:#27AE60; color:white; border-radius:6px; padding:8px 16px;}");
    balanceLayout->addWidget(new QLabel("账户余额："));
    balanceLayout->addWidget(balanceLabel);
    balanceLayout->addStretch();
    balanceLayout->addWidget(rechargeBtn);

    // 用户信息表格 - 融合当前值和修改值，取消水平表头
    infoTable = new QTableWidget;
    infoTable->setColumnCount(1);  // 融合当前值和修改值
    infoTable->horizontalHeader()->setVisible(false);  // 取消水平表头显示
    infoTable->setRowCount(4);  // 用户名, 姓名, 电话, 地址
    infoTable->verticalHeader()->setVisible(true);
    infoTable->setVerticalHeaderLabels({"用户名", "姓名", "电话", "地址"});
    infoTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    infoTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    infoTable->setStyleSheet(R"(
        QTableWidget{
            background:white;
            border:1px solid #DCDFE6;
            border-radius:6px;
            font-size:14px;
        }
        QHeaderView::section{
            background-color:#E8EDF2;
            padding:8px;
            font-weight:bold;
        }
        QTableWidget::item{
            padding:8px;
        }
        QTableCornerButton::section{
            background-color:#E8EDF2;
        }
    )");

    auto updateBtn = new QPushButton("保存修改");
    updateBtn->setStyleSheet("QPushButton{background:#3498DB; color:white; border-radius:6px; padding:10px; font-size:14px;}");

    // 密码修改
    auto pwdBtn = new QPushButton("修改密码");
    pwdBtn->setStyleSheet("QPushButton{background:#F39C12; color:white; border-radius:6px; padding:10px; font-size:14px;}");

    lay->addWidget(title);
    lay->addLayout(balanceLayout);
    lay->addWidget(new QLabel("账户信息："));
    lay->addWidget(infoTable);
    lay->addWidget(updateBtn);
    lay->addWidget(pwdBtn);
    lay->addStretch();

    connect(updateBtn, &QPushButton::clicked, this, &AccountPage::update_profile);
    connect(rechargeBtn, &QPushButton::clicked, this, &AccountPage::show_recharge_dialog);
    connect(pwdBtn, &QPushButton::clicked, this, &AccountPage::change_password);
    
    // 加载用户信息
    load_user_info();
}
    
void AccountPage::load_user_info() {
    // 获取用户信息
    std::vector<User*> users;
    ErrorCode quRes = system->queryUsers(username, UserType::CUSTOMER, users);
    if (quRes == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
        quRes = system->queryUsers(username, UserType::CUSTOMER, users);
    }
    if (quRes != ErrorCode::SUCCESS || users.empty()) {
        QString msg;
        switch (quRes) {
            case ErrorCode::USER_NOT_FOUND:
                msg = "查询用户信息失败：用户不存在";
                break;
            case ErrorCode::INVALID_ARGS:
                msg = "查询用户信息失败：数据格式异常";
                break;
            case ErrorCode::INTERNAL_ERROR:
                msg = "查询用户信息失败：网络连接异常，请重试";
                break;
            default:
                msg = QString("查询用户信息失败（错误码 %1）").arg(static_cast<int>(quRes));
                break;
        }
        QMessageBox::critical(this, "失败", msg);
        return;
    }
    User* user = users.front();
    // 使用 queryUsers 返回的 User 对象中的余额显示，避免服务端 queryBalance 行为差异
    double balance = user->getBalance();
    balanceLabel->setText(QString("¥%1").arg(balance, 0, 'f', 2));
    
    // 用户名（只读）
    infoTable->setItem(0, 0, new QTableWidgetItem(QString::fromStdString(user->getUsername())));
    infoTable->item(0, 0)->setFlags(Qt::NoItemFlags);  // 设置为不可编辑
    
    // 姓名（可编辑输入框）
    auto nameEdit = new QLineEdit(QString::fromStdString(user->getName()));
    nameEdit->setPlaceholderText("请输入姓名");
    nameEdit->setStyleSheet("border:1px solid #DCDFE6; border-radius:4px; padding:4px;");
    infoTable->setCellWidget(1, 0, nameEdit);
    
    // 电话（可编辑输入框）
    auto phoneEdit = new QLineEdit(QString::fromStdString(user->getPhone()));
    phoneEdit->setPlaceholderText("请输入电话");
    phoneEdit->setStyleSheet("border:1px solid #DCDFE6; border-radius:4px; padding:4px;");
    infoTable->setCellWidget(2, 0, phoneEdit);
    
    // 地址（可编辑输入框）
    auto addrEdit = new QLineEdit(QString::fromStdString(user->getAddress()));
    addrEdit->setPlaceholderText("请输入地址");
    addrEdit->setStyleSheet("border:1px solid #DCDFE6; border-radius:4px; padding:4px;");
    infoTable->setCellWidget(3, 0, addrEdit);
    
}
    
void AccountPage::update_profile() {
    // 从表格中获取修改的值
    QLineEdit* nameEdit = qobject_cast<QLineEdit*>(infoTable->cellWidget(1, 0));
    QLineEdit* phoneEdit = qobject_cast<QLineEdit*>(infoTable->cellWidget(2, 0));
    QLineEdit* addrEdit = qobject_cast<QLineEdit*>(infoTable->cellWidget(3, 0));
    
    if (!nameEdit || !phoneEdit || !addrEdit) {
        QMessageBox::warning(this, "错误", "界面初始化异常");
        return;
    }
    
    QString name = nameEdit->text().trimmed();
    QString phone = phoneEdit->text().trimmed();
    QString addr = addrEdit->text().trimmed();
    
    if (name.isEmpty() || phone.isEmpty() || addr.isEmpty()) {
        QMessageBox::warning(this, "提示", "请填写完整信息");
        return;
    }
    
    // LogisticsSystem 没有提供修改用户资料的功能，所以这里只是显示信息
    QMessageBox::information(this, "提示", "当前系统不支持修改用户资料");
}
    
void AccountPage::recharge(unsigned int amount) {
    if (amount <= 0) {
        QMessageBox::warning(this, "提示", "充值金额必须大于0");
        return;
    }   
    // 调用 LogisticsSystem 充值
    ErrorCode res = system->rechargeBalance(static_cast<double>(amount));
    if (res == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
        res = system->rechargeBalance(static_cast<double>(amount));
    }
    if (res == ErrorCode::SUCCESS) {
        // 重新查询用户信息以获取最新余额
        std::vector<User*> users;
        ErrorCode quRes = system->queryUsers(username, UserType::CUSTOMER, users);
        if (quRes == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
            quRes = system->queryUsers(username, UserType::CUSTOMER, users);
        }
        if (quRes == ErrorCode::SUCCESS && !users.empty()) {
            double newBalance = users.front()->getBalance();
            QMessageBox::information(this, "成功", QString("充值成功！\n新余额：¥%1").arg(newBalance, 0, 'f', 2));
            balanceLabel->setText(QString("¥%1").arg(newBalance, 0, 'f', 2));
        } else {
            QMessageBox::information(this, "成功", "充值成功！");
        }
    } else {
        QString msg;
        switch (res) {
            case ErrorCode::USER_NOT_FOUND:
                msg = "充值失败：用户不存在";
                break;
            case ErrorCode::INVALID_AMOUNT:
                msg = "充值失败：金额不合法";
                break;
            case ErrorCode::INVALID_ARGS:
                msg = "充值失败：响应数据异常";
                break;
            case ErrorCode::INTERNAL_ERROR:
                msg = "充值失败：网络连接异常，请重试";
                break;
            default:
                msg = QString("充值失败（错误码 %1）").arg(static_cast<int>(res));
                break;
        }
        QMessageBox::critical(this, "充值失败", msg);
    }
}
    
void AccountPage::change_password() {
    bool ok = false;
    QString old_pwd = QInputDialog::getText(this, "修改密码", "请输入旧密码", QLineEdit::Password, "", &ok);
    if (!ok) {
        return;
    }
    QString new_pwd = QInputDialog::getText(this, "修改密码", "请输入新密码", QLineEdit::Password, "", &ok);
    if (!ok) {
        return;
    }
    if (old_pwd == new_pwd) {
        QMessageBox::warning(this, "提示", "新密码不能与旧密码相同");
        return;
    }
    ErrorCode res = system->changePassword(old_pwd.toStdString(), new_pwd.toStdString());
    if (res == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
        res = system->changePassword(old_pwd.toStdString(), new_pwd.toStdString());
    }
    if (res == ErrorCode::SUCCESS) {
        QMessageBox::information(this, "成功", "密码修改成功！");
    } else {
        QString msg;
        switch (res) {
            case ErrorCode::USER_NOT_FOUND:
                msg = "密码修改失败：用户不存在";
                break;
            case ErrorCode::LOGIN_FAILED:
                msg = "密码修改失败：旧密码错误";
                break;
            case ErrorCode::INVALID_ARGS:
                msg = "密码修改失败：响应数据异常";
                break;
            case ErrorCode::INTERNAL_ERROR:
                msg = "密码修改失败：网络连接异常，请重试";
                break;
            default:
                msg = QString("密码修改失败（错误码 %1）").arg(static_cast<int>(res));
                break;
        }
        QMessageBox::critical(this, "密码修改失败", msg);
    }
}

void AccountPage::show_recharge_dialog() {
    bool ok = false;
    unsigned int amount = QInputDialog::getInt(this, "充值", "请输入充值金额", 0, 1, 1000000, 1, &ok);
    if (!ok) {
        return;
    }
    recharge(amount);
}