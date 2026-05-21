#include "RegisterWindow.h"


RegisterWindow::RegisterWindow(Communication* sys, QWidget* parent)
    : QDialog(parent), system(sys) {
    setWindowTitle("注册新账号");
    setFixedSize(420, 420);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(16);

    auto title = new QLabel("创建新账号");
    title->setStyleSheet("font-size:20px; font-weight:bold; color:#2C3E50;");
    title->setAlignment(Qt::AlignCenter);

    auto form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignCenter);

    usernameEdit = new QLineEdit;
    passwordEdit = new QLineEdit;
    confirmEdit = new QLineEdit;
    nameEdit = new QLineEdit;
    phoneEdit = new QLineEdit;
    addressEdit = new QLineEdit;
    roleCombo = new QComboBox;
    roleCombo->addItems({"用户", "快递员"});

    passwordEdit->setEchoMode(QLineEdit::Normal);
    confirmEdit->setEchoMode(QLineEdit::Normal);

    QString editStyle = R"(
        QLineEdit{
            border:1px solid #DCDFE6; 
            border-radius:6px; 
            padding:0px 5px; 
            font-size:14px;
            background:#F8F9FA;
            min-height:25px;
        }
        QComboBox{
            border:1px solid #DCDFE6; 
            border-radius:6px; 
            padding:0px 5px; 
            font-size:14px;
            background:#F8F9FA;
            min-height:25px;
        }
    )";
    usernameEdit->setStyleSheet(editStyle);
    passwordEdit->setStyleSheet(editStyle);
    confirmEdit->setStyleSheet(editStyle);
    nameEdit->setStyleSheet(editStyle);
    phoneEdit->setStyleSheet(editStyle);
    addressEdit->setStyleSheet(editStyle);
    roleCombo->setStyleSheet(editStyle);

    form->addRow("用户名：", usernameEdit);
    form->addRow("登录密码：", passwordEdit);
    form->addRow("确认密码：", confirmEdit);
    form->addRow("真实姓名：", nameEdit);
    form->addRow("联系电话：", phoneEdit);
    form->addRow("地址：", addressEdit);
    form->addRow("角色：", roleCombo);

    auto submitBtn = new QPushButton("注册");
    submitBtn->setStyleSheet(R"(
        QPushButton{background:#3498DB; color:white; border:none; border-radius:6px; padding:12px; font-size:16px; font-weight:bold;}
        QPushButton:hover{background:#2980B9;}
    )");

    layout->addWidget(title);
    layout->addLayout(form);
    layout->addWidget(submitBtn);
    layout->addStretch();

    connect(submitBtn, &QPushButton::clicked, this, &RegisterWindow::submit_registration);
    

}

void RegisterWindow::submit_registration() {
    QString username = usernameEdit->text().trimmed();
    QString password = passwordEdit->text();
    QString confirm = confirmEdit->text();
    QString name = nameEdit->text().trimmed();
    QString phone = phoneEdit->text().trimmed();
    QString address = addressEdit->text().trimmed();
    UserType type = roleCombo->currentIndex() == 0 ? UserType::CUSTOMER : UserType::COURIER;

    if (username.isEmpty() || password.isEmpty() || confirm.isEmpty() || name.isEmpty() || phone.isEmpty() || address.isEmpty()) {
        QMessageBox::warning(this, "提示", "请填写完整注册信息");
        return;
    }

    if (password != confirm) {
        QMessageBox::warning(this, "提示", "两次输入的密码不一致");
        return;
    }

    ErrorCode res = system->registerUser(username.toStdString(), password.toStdString(), name.toStdString(), phone.toStdString(), address.toStdString(), type);
    if (res != ErrorCode::SUCCESS) {
        QString msg;
        switch (res) {
            case ErrorCode::USER_EXISTS:
                msg = "该用户名已被注册，请更换用户名";
                break;
            case ErrorCode::INVALID_ARGS:
                msg = "不允许注册管理员账号";
                break;
            case ErrorCode::INTERNAL_ERROR:
                msg = "注册失败：网络连接异常，请重试";
                break;
            default:
                msg = QString("注册失败（错误码 %1）").arg(static_cast<int>(res));
                break;
        }
        QMessageBox::critical(this, "注册失败", msg);
        return;
    }

    QMessageBox::information(this, "注册成功", "注册成功，请返回登录页面进行登录");
    accept();
}