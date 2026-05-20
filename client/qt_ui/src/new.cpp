#include "new.h"
#include "RegisterWindow.h"



// ------------------------------
// 管理员页面
// ------------------------------
AdminExpressPage::AdminExpressPage(QWidget *p, std::string username, Communication* sys) : QWidget(p), username(username), system(sys) {
    setStyleSheet("background:#F5F7FA;");
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(30,30,30,30);
    lay->setSpacing(15);

    auto title = new QLabel("🔧 快递管理（可修改/分配/增删）");
    title->setStyleSheet("font-size:20px; font-weight:bold; color:#2C3E50;");

    auto btnLayout = new QHBoxLayout;
    auto addBtn = new QPushButton("新增快递");
    auto editBtn = new QPushButton("分配快递员/修改");
    auto delBtn = new QPushButton("删除快递");
    auto refreshBtn = new QPushButton("刷新列表");
    addBtn->setStyleSheet("background:#3498DB;color:white;border-radius:6px;padding:8px;");
    editBtn->setStyleSheet(addBtn->styleSheet());
    delBtn->setStyleSheet("background:#E74C3C;color:white;border-radius:6px;padding:8px;");
    refreshBtn->setStyleSheet("background:#27AE60;color:white;border-radius:6px;padding:8px;");
    btnLayout->addWidget(addBtn);btnLayout->addWidget(editBtn);btnLayout->addWidget(delBtn);btnLayout->addWidget(refreshBtn);

    table = new QTableWidget;
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({"运单号","寄件人","收件人","状态","快递员"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setStyleSheet("QTableWidget{background:white;border:1px solid #DCDFE6;border-radius:6px;}");

    lay->addWidget(title);lay->addLayout(btnLayout);lay->addWidget(table);

    connect(editBtn,&QPushButton::clicked,this,&AdminExpressPage::assign_courier);
    connect(delBtn,&QPushButton::clicked,this,&AdminExpressPage::delete_package);
    connect(addBtn,&QPushButton::clicked,this,&AdminExpressPage::add_package);
    connect(refreshBtn,&QPushButton::clicked,this,&AdminExpressPage::load_packages);
    
    // 加载快递列表
    load_packages();
}
    
void AdminExpressPage::load_packages() {
    std::vector<Parcel> packages;
    system->queryParcels("", "", "", "", ParcelStatus::OTHER, 0, 0, packages);
    table->setRowCount(packages.size());
    for (size_t i = 0; i < packages.size(); i++) {
        const auto& pkg = packages[i];
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(pkg.getParcelId())));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(pkg.getSenderName())));
        table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(pkg.getReceiverName())));
        
        QString status_text;
        switch (pkg.getStatus()) {
            case ParcelStatus::PENDING_COLLECTION: status_text = "待处理"; break;
            case ParcelStatus::PENDING_SIGN: status_text = "待签收"; break;
            case ParcelStatus::SIGNED: status_text = "已签收"; break;
            default: status_text = "其他"; break;
        }
        table->setItem(i, 3, new QTableWidgetItem(status_text));
        table->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(pkg.getCourierName())));
    }
}

void AdminExpressPage::assign_courier() {
    int r=table->currentRow();if(r<0)return;
    int tracking_num = table->item(r, 0)->text().toInt();
    
    bool ok;
    QString courier_name = QInputDialog::getText(this, "分配快递员", "输入快递员姓名：", QLineEdit::Normal, "", &ok);
    if (!ok || courier_name.isEmpty()) return;
    QString parcelId = table->item(r, 0)->text();
    
    // 调用 LogisticsSystem 分配快递员
    bool success = system->assignParcel(parcelId.toStdString(), courier_name.toStdString());
    
    if (success) {
        table->item(r, 4)->setText(courier_name);
        QMessageBox::information(this,"成功","快递员分配成功");
    } else {
        ErrorCode ec = system->getLastError();
        QString msg;
        switch (ec) {
            case ErrorCode::USER_NOT_FOUND:
                msg = "分配失败：快递员 " + courier_name + " 不存在";
                break;
            case ErrorCode::PARCEL_NOT_FOUND:
                msg = "分配失败：运单号 " + parcelId + " 不存在";
                break;
            default:
                msg = "分配失败，请重试";
                break;
        }
        QMessageBox::critical(this,"失败", msg);
    }
}

void AdminExpressPage::delete_package() {
    int r = table->currentRow();
    if (r < 0) return;
    if (QMessageBox::question(this, "确认", "删除该快递？") == QMessageBox::Yes) {
        QString parcelId = table->item(r, 0)->text();
        if (system->deleteParcel(parcelId.toStdString())) {
            table->removeRow(r);
            QMessageBox::information(this, "成功", "快递已删除");
        } else {
            ErrorCode ec = system->getLastError();
            QString msg;
            switch (ec) {
                case ErrorCode::PARCEL_NOT_FOUND:
                    msg = "删除失败：运单号 " + parcelId + " 不存在";
                    break;
                case ErrorCode::INVALID_STATUS:
                    msg = "删除失败：仅已签收快递可删除";
                    break;
                default:
                    msg = "删除失败，请重试";
                    break;
            }
            QMessageBox::critical(this, "失败", msg);
        }
    }
}

void AdminExpressPage::add_package() {
    bool ok;
    QString receiver = QInputDialog::getText(this, "新增快递", "收件人用户名：", QLineEdit::Normal, "", &ok);
    if (!ok || receiver.isEmpty()) return;
    QStringList types = {"普通快递", "易碎品", "书籍"};
    QString typeText = QInputDialog::getItem(this, "新增快递", "选择快递类型：", types, 0, false, &ok);
    if (!ok || typeText.isEmpty()) return;
    QString description = QInputDialog::getText(this, "新增快递", "物品描述：", QLineEdit::Normal, "", &ok);
    if (!ok || description.isEmpty()) return;

    ParcelType type = ParcelType::NORMAL;
    if (typeText == "易碎品") type = ParcelType::FRAGILE;
    else if (typeText == "书籍") type = ParcelType::BOOK;

    std::string parcelId;
    if (!system->sendParcel(receiver.toStdString(), type, 1.0, description.toStdString(), parcelId)) {
        ErrorCode ec = system->getLastError();
        QString msg;
        switch (ec) {
            case ErrorCode::USER_NOT_FOUND:
                msg = "新增快递失败：收件人 " + receiver + " 不存在";
                break;
            case ErrorCode::INSUFFICIENT_BALANCE:
                msg = "新增快递失败：余额不足";
                break;
            default:
                msg = "新增快递失败，请重试";
                break;
        }
        QMessageBox::critical(this, "失败", msg);
        return;
    }
    QMessageBox::information(this, "成功", QString("新增快递成功，运单号：%1").arg(QString::fromStdString(parcelId)));
    load_packages();
}

AdminUserPage::AdminUserPage(QWidget *p, std::string username, Communication* sys) : QWidget(p), username(username), system(sys) {
    setStyleSheet("background:#F5F7FA;");
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(30,30,30,30);
    lay->setSpacing(15);

    auto title = new QLabel("👥 用户账户管理（可增删改）");
    title->setStyleSheet("font-size:20px; font-weight:bold; color:#2C3E50;");

    auto btnLayout = new QHBoxLayout;
    auto addBtn = new QPushButton("新增用户");
    auto editBtn = new QPushButton("修改用户");
    auto delBtn = new QPushButton("删除用户");
    auto refreshBtn = new QPushButton("刷新列表");
    addBtn->setStyleSheet("background:#3498DB;color:white;border-radius:6px;padding:8px;");
    editBtn->setStyleSheet(addBtn->styleSheet());
    delBtn->setStyleSheet("background:#E74C3C;color:white;border-radius:6px;padding:8px;");
    refreshBtn->setStyleSheet("background:#27AE60;color:white;border-radius:6px;padding:8px;");
    btnLayout->addWidget(addBtn);btnLayout->addWidget(editBtn);btnLayout->addWidget(delBtn);btnLayout->addWidget(refreshBtn);

    table = new QTableWidget;
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"用户名","姓名","角色","余额"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setStyleSheet("QTableWidget{background:white;border:1px solid #DCDFE6;border-radius:6px;}");

    lay->addWidget(title);lay->addLayout(btnLayout);lay->addWidget(table);

    connect(editBtn,&QPushButton::clicked,this,&AdminUserPage::edit_user);
    connect(delBtn,&QPushButton::clicked,this,&AdminUserPage::delete_user);
    connect(addBtn,&QPushButton::clicked,this,&AdminUserPage::add_user);
    connect(refreshBtn,&QPushButton::clicked,this,&AdminUserPage::load_users);
    
    // 加载用户列表
    load_users();
}


void AdminUserPage::load_users() {
    std::vector<User> users;
    system->queryUsers("", UserType::ADMINISTRATOR, users);
    table->setRowCount(users.size());
    for (size_t i = 0; i < users.size(); i++) {
        const auto& user = users[i];
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(user.getUsername())));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(user.getName())));
        
        QString role_text;
        switch (user.getUserType()) {
            case UserType::CUSTOMER: role_text = "用户"; break;
            case UserType::COURIER: role_text = "快递员"; break;
            case UserType::ADMINISTRATOR: role_text = "管理员"; break;
            default: role_text = "未知"; break;
        }
        table->setItem(i, 2, new QTableWidgetItem(role_text));
        table->setItem(i, 3, new QTableWidgetItem(QString("¥%1").arg(user.getBalance())));
    }
}

void AdminUserPage::edit_user() {
    int r=table->currentRow();
    if (r < 0) return;
    QString username = table->item(r, 0)->text();
    bool ok;
    QString oldPwd = QInputDialog::getText(this, "修改用户密码", "请输入旧密码：", QLineEdit::Password, "", &ok);
    if (!ok) return;
    QString newPwd = QInputDialog::getText(this, "修改用户密码", "请输入新密码：", QLineEdit::Password, "", &ok);
    if (!ok || newPwd.isEmpty()) return;

    // 管理员不能修改其他用户密码，使用 changePassword 但需要切换用户？
    // 或许不支持，显示不支持
    QMessageBox::information(this, "提示", "当前系统不支持管理员修改用户密码");
}

void AdminUserPage::delete_user() {
    int r = table->currentRow();
    if (r < 0) return;
    QString usernameText = table->item(r, 0)->text();
    if (QMessageBox::question(this, "确认", QString("删除用户 %1？").arg(usernameText)) == QMessageBox::Yes) {
        if (system->deleteAccount(usernameText.toStdString())) {
            table->removeRow(r);
            QMessageBox::information(this, "成功", "用户已删除");
        } else {
            ErrorCode ec = system->getLastError();
            QString msg;
            switch (ec) {
                case ErrorCode::USER_NOT_FOUND:
                    msg = "删除失败：用户 " + usernameText + " 不存在";
                    break;
                case ErrorCode::DELETE_BLOCKED:
                    msg = "删除失败：用户存在未完成快递或是管理员，无法删除";
                    break;
                default:
                    msg = "删除失败，请重试";
                    break;
            }
            QMessageBox::critical(this, "失败", msg);
        }
    }
}

void AdminUserPage::add_user() {
    // 使用现有的注册窗口来新增用户
    RegisterWindow *registerDlg = new RegisterWindow(system, this);
    registerDlg->setWindowTitle("管理员 - 新增用户");
    
    // 连接注册成功的信号
    connect(registerDlg, &RegisterWindow::accepted, this, [=]() {
        QMessageBox::information(this, "成功", "用户新增成功");
        load_users();  // 刷新用户列表
        registerDlg->deleteLater();  // 清理对话框
    });
    
    connect(registerDlg, &RegisterWindow::rejected, this, [=]() {
        registerDlg->deleteLater();  // 清理对话框
    });
    
    registerDlg->exec();  // 模态显示注册窗口
}

// ------------------------------
// 管理员 - 统计页面
// ------------------------------
// 构造函数：创建管理员统计页面
AdminStatsPage::AdminStatsPage(QWidget *p, std::string username, Communication* sys) : QWidget(p), username(username), system(sys) {
    setStyleSheet("background:#F5F7FA;");
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(30,30,30,30);
    lay->setSpacing(20);

    auto title = new QLabel("📊 系统统计数据");
    title->setStyleSheet("font-size:20px; font-weight:bold; color:#2C3E50;");

    auto refreshBtn = new QPushButton("刷新统计");
    refreshBtn->setStyleSheet("background:#27AE60;color:white;border-radius:6px;padding:8px;");
    refreshBtn->setMaximumWidth(150);

    table = new QTableWidget;
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"统计项","数值"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setStyleSheet("QTableWidget{background:white;border:1px solid #DCDFE6;border-radius:6px;}");
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setRowCount(6);

    lay->addWidget(title);
    lay->addWidget(refreshBtn);
    lay->addWidget(table);

    connect(refreshBtn, &QPushButton::clicked, this, &AdminStatsPage::load_stats);

    load_stats();
}

// 加载统计数据
void AdminStatsPage::load_stats() {
    int totalUsers, totalParcels, pendingCollection, collected, Signed;
    double adminTotalBalance;

    if (!system->getStatistics(totalUsers, totalParcels, pendingCollection, collected, Signed, adminTotalBalance)) {
        QMessageBox::critical(this, "失败", "获取统计信息失败");
        return;
    }
    
    table->setItem(0, 0, new QTableWidgetItem("总用户数"));
    table->setItem(0, 1, new QTableWidgetItem(QString::number(totalUsers)));

    table->setItem(1, 0, new QTableWidgetItem("总快递数"));
    table->setItem(1, 1, new QTableWidgetItem(QString::number(totalParcels)));

    table->setItem(2, 0, new QTableWidgetItem("待揽收快递数"));
    table->setItem(2, 1, new QTableWidgetItem(QString::number(pendingCollection)));

    table->setItem(3, 0, new QTableWidgetItem("已揽收快递数"));
    table->setItem(3, 1, new QTableWidgetItem(QString::number(collected)));

    table->setItem(4, 0, new QTableWidgetItem("已签收快递数"));
    table->setItem(4, 1, new QTableWidgetItem(QString::number(Signed)));

    table->setItem(5, 0, new QTableWidgetItem("公司资金池余额"));
    table->setItem(5, 1, new QTableWidgetItem(QString("¥%1").arg(adminTotalBalance, 0, 'f', 2)));
}




// ------------------------------
// 三个主窗口
// ------------------------------

AdminWindow::AdminWindow(std::string username, Communication* sys) : username(username), system(sys) {
    setWindowTitle("管理员端 - 快递系统");
    setMinimumSize(850,600);
    auto c = new QWidget; setCentralWidget(c);
    auto mainLayout = new QHBoxLayout(c);
    mainLayout->setContentsMargins(0,0,0,0);

    auto nav = new QListWidget;
    nav->setFixedWidth(180);
    nav->addItem("快递管理");
    nav->addItem("账户管理");
    nav->addItem("统计信息");
    nav->setStyleSheet(R"(
        QListWidget{background:#2C3E50; color:white; font-size:15px;}
        QListWidget::item{height:45px; padding-left:15px;}
        QListWidget::item:selected{background:#3498DB;}
    )");

    auto stack = new QStackedWidget;
    auto express_page = new AdminExpressPage(this, username, system);
    auto user_page = new AdminUserPage(this, username, system);
    auto stats_page = new AdminStatsPage(this, username, system);
    stack->addWidget(express_page);
    stack->addWidget(user_page);
    stack->addWidget(stats_page);

    connect(nav,&QListWidget::currentRowChanged,stack,&QStackedWidget::setCurrentIndex);

    mainLayout->addWidget(nav);
    mainLayout->addWidget(stack);
}