#include "CourierPage.h"


// ------------------------------
// 快递员端（新增）
// ------------------------------
CourierHomePage::CourierHomePage(QWidget *p, std::string courier_id, QueryPage *query_page, Communication* sys) : courier_id(courier_id), query_page(query_page), system(sys), QWidget(p) {
    setStyleSheet("background:#F5F7FA;");
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30,30,30,30);
    mainLayout->setSpacing(20);

    // 标题
    auto title = new QLabel("🚚 快递员工作台");
    title->setStyleSheet("font-size:22px; font-weight:bold; color:#2C3E50;");
    title->setAlignment(Qt::AlignCenter);

    // 余额显示
    auto balanceLayout = new QHBoxLayout;
    balanceLabel = new QLabel("账户余额：¥ 568.00");
    balanceLabel->setStyleSheet("font-size:16px; color:#E74C3C; font-weight:bold;");
    balanceLayout->addStretch();
    balanceLayout->addWidget(balanceLabel);
    balanceLayout->addStretch();

    // 待揽收快递表格
    auto tipLabel = new QLabel("📦 待揽收快递列表：");
    tipLabel->setStyleSheet("font-size:15px; color:#333;");

    table = new QTableWidget;
    table->setColumnCount(8);
    table->setHorizontalHeaderLabels({"运单号","寄件人","收件人","快递类型","重量","物品描述","快递员","预计佣金"});
    
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setStyleSheet(R"(
        QTableWidget{
            background:white;
            border:1px solid #DCDFE6;
            border-radius:6px;
            font-size:14px;
            min-height:200px;
        }
        QHeaderView::section{
            background-color:#E8EDF2;
            padding:8px;
            font-weight:bold;
        }
    )");

    // 揽收按钮
    auto takeBtn = new QPushButton("✅ 确认揽收选中快递");
    takeBtn->setStyleSheet(R"(
        QPushButton{
            background-color:#27AE60;
            color:white;
            border:none;
            border-radius:6px;
            padding:12px;
            font-size:15px;
        }
        QPushButton:hover{background-color:#219653;}
    )");

    refreshBtn = new QPushButton("🔄 刷新");
    refreshBtn->setStyleSheet(R"(
        QPushButton{
            background-color:#3498DB;
            color:white;
            border:none;
            border-radius:6px;
            padding:12px;
            font-size:15px;
        }
        QPushButton:hover{background-color:#2980B9;}
    )"
    );

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(takeBtn);
    buttonLayout->addWidget(refreshBtn);
    buttonLayout->addStretch();

    // 布局
    mainLayout->addWidget(title);
    mainLayout->addLayout(balanceLayout);
    mainLayout->addWidget(tipLabel);
    mainLayout->addWidget(table);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();

    // 揽收功能
    connect(takeBtn, &QPushButton::clicked, this, &CourierHomePage::take_package);
    connect(refreshBtn, &QPushButton::clicked, this, &CourierHomePage::load_packages);
    
    // 加载待揽收快递
    load_packages();
}


void CourierHomePage::load_packages() {
    double balance;
    if (system->queryBalance(balance)) {
        balanceLabel->setText(QString("账户余额：¥ %1").arg(QString::number(balance)));
    } else {
        balanceLabel->setText("账户余额：获取失败");
    }
    
    std::vector<Parcel> packages;
    system->queryParcels("", "", "", courier_id, ParcelStatus::PENDING_COLLECTION, 0, 0, packages);
    table->setRowCount(packages.size());
    for (size_t i = 0; i < packages.size(); i++) {
        const auto& pkg = packages[i];
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(pkg.getParcelId())));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(pkg.getSenderName())));
        table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(pkg.getReceiverName())));

        QString typeText;
        switch (pkg.getParcelType()) {
            case ParcelType::NORMAL: typeText = "普通快递"; break;
            case ParcelType::FRAGILE: typeText = "易碎品"; break;
            case ParcelType::BOOK: typeText = "书籍"; break;
            default: typeText = "未知"; break;
        }
        table->setItem(i, 3, new QTableWidgetItem(typeText));
        table->setItem(i, 4, new QTableWidgetItem(QString::number(pkg.getWeight())));
        table->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(pkg.getDescription())));
        table->setItem(i, 6, new QTableWidgetItem(QString::fromStdString(pkg.getCourierName())));
        table->setItem(i, 7, new QTableWidgetItem(QString("¥ %1").arg(pkg.getPrice() * 0.5)));
        table->setRowHidden(i, false);
    }
}

void CourierHomePage::take_package() {
    int row = table->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请先选择要揽收的快递");
        return;
    }

    QString parcelId = table->item(row, 0)->text();
    std::vector<std::string> collected;
    if (system->collectParcels({parcelId.toStdString()}, collected)) {
        if (!collected.empty()) {
            QMessageBox::information(this, "成功", "快递揽收完成！");
            load_packages();
            if (query_page) query_page->refresh();
        } else {
            QMessageBox::warning(this, "失败", "揽收失败，请检查快递状态");
        }
    } else {
        ErrorCode ec = system->getLastError();
        QString msg = (ec == ErrorCode::USER_NOT_FOUND) 
            ? "揽收失败：当前账户无效" : "揽收失败，请重试";
        QMessageBox::critical(this, "失败", msg);
    }
}