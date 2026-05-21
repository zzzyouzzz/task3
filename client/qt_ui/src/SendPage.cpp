#include "SendPage.h"


SendPage::SendPage(QWidget *parent, std::string username, Communication* sys) : QWidget(parent), username(username), system(sys) {
    setStyleSheet("background:#F5F7FA;");
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(40,30,40,30);
    layout->setSpacing(18);

    auto title = new QLabel("📦 寄快递");
    title->setStyleSheet("font-size:20px; font-weight:bold; color:#2C3E50;");
    title->setAlignment(Qt::AlignCenter);

    editReceiver = new QLineEdit; editReceiver->setPlaceholderText("收件人姓名");
    //editPhone = new QLineEdit; editPhone->setPlaceholderText("联系电话");
    //editAddr = new QLineEdit; editAddr->setPlaceholderText("收货地址");
    typeBox = new QComboBox; typeBox->addItems({"普通快递","易碎品","书籍"});
    contentEdit = new QTextEdit; contentEdit->setPlaceholderText("物品描述");
    auto submitBtn = new QPushButton("提交寄件订单");

    QString editStyle = R"(
        QLineEdit,QComboBox,QTextEdit{
            border:1px solid #DCDFE6; border-radius:6px; padding:8px 12px; background:white; font-size:14px;
        }
    )";
    editReceiver->setStyleSheet(editStyle);
    //editPhone->setStyleSheet(editStyle);
    //editAddr->setStyleSheet(editStyle);
    typeBox->setStyleSheet(editStyle);
    contentEdit->setStyleSheet(editStyle);
    submitBtn->setStyleSheet(R"(
        QPushButton{background:#3498DB; color:white; border:none; border-radius:6px; padding:10px; font-size:14px;}
        QPushButton:hover{background:#2980B9;}
    )");

    layout->addWidget(title);
    layout->addWidget(new QLabel("收件人："));
    layout->addWidget(editReceiver);
    /*layout->addWidget(new QLabel("电话："));
    layout->addWidget(editPhone);
    layout->addWidget(new QLabel("地址："));
    layout->addWidget(editAddr);*/
    
    layout->addWidget(new QLabel("物品类型："));
    layout->addWidget(typeBox);
    layout->addWidget(new QLabel("物品内容："));
    layout->addWidget(contentEdit);
    layout->addStretch();
    layout->addWidget(submitBtn);

    connect(submitBtn, &QPushButton::clicked, this, &SendPage::submit_package);
}
    
void SendPage::submit_package() {
    QString receiver = editReceiver->text().trimmed();
    //QString phone = editPhone->text().trimmed();
    //QString addr = editAddr->text().trimmed();
    QString content = contentEdit->toPlainText().trimmed();
    
    if (receiver.isEmpty()  || content.isEmpty()) {
        QMessageBox::warning(this, "提示", "请填写完整信息");
        return;
    }
    
    // 使用 Communication 发送快递
    ParcelType type = ParcelType::NORMAL;
    if (typeBox->currentText() == "易碎品") type = ParcelType::FRAGILE;
    else if (typeBox->currentText() == "书籍") type = ParcelType::BOOK;
    
    std::string parcelId;
    ErrorCode res = system->sendParcel(receiver.toStdString(), type, 1.0, content.toStdString(), parcelId);
    if (res != ErrorCode::SUCCESS) {
        QString msg;
        switch (res) {
            case ErrorCode::USER_NOT_FOUND:
                msg = "寄件失败：收件人 " + receiver + " 不存在";
                break;
            case ErrorCode::INSUFFICIENT_BALANCE:
                msg = "寄件失败：账户余额不足，请先充值";
                break;
            default:
                msg = "寄件失败，请检查输入信息";
                break;
        }
        QMessageBox::critical(this, "寄件失败", msg);
        return;
    }
    
    QMessageBox::information(this, "成功", 
        QString("寄件订单已提交！\n运单号：%1\n收件人：%2")
            .arg(QString::fromStdString(parcelId))
            .arg(receiver));
    
    // 清空表单
    editReceiver->clear();
    //editPhone->clear();
    //editAddr->clear();
    contentEdit->clear();
    // 刷新查询页面
    if (queryPage) {
        queryPage->refresh();
    }
    // 刷新账户页面
    if (accountPage) {
        accountPage->refresh();
    }
}

void SendPage::setQueryPage(QueryPage* qp) {
    queryPage = qp;
}

void SendPage::setAccountPage(AccountPage* ap) {
    accountPage = ap;
}
