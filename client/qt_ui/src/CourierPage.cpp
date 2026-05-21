#include "CourierPage.h"
#include "ReconnectHelper.h"
#include <set>
#include <vector>

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
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
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
    ErrorCode balRes = system->queryBalance(balance);
    if (balRes == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
        balRes = system->queryBalance(balance);
    }
    if (balRes == ErrorCode::SUCCESS) {
        balanceLabel->setText(QString("账户余额：¥ %1").arg(QString::number(balance)));
    } else {
        balanceLabel->setText("账户余额：获取失败");
        if (balRes != ErrorCode::INTERNAL_ERROR) {
            QString balMsg;
            switch (balRes) {
                case ErrorCode::USER_NOT_FOUND:
                    balMsg = "获取余额失败：用户不存在";
                    break;
                case ErrorCode::INVALID_ARGS:
                    balMsg = "获取余额失败：数据格式异常";
                    break;
                default:
                    balMsg = QString("获取余额失败（错误码 %1）").arg(static_cast<int>(balRes));
                    break;
            }
            QMessageBox::warning(this, "余额获取失败", balMsg);
        }
    }

    std::vector<Parcel> packages;
    ErrorCode qe = system->queryParcels("", "", "", courier_id, ParcelStatus::PENDING_COLLECTION, 0, 0, packages);
    if (qe == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
        system->queryParcels("", "", "", courier_id, ParcelStatus::PENDING_COLLECTION, 0, 0, packages);
    }
    table->setRowCount(packages.size());
    for (size_t i = 0; i < packages.size(); i++) {
        const auto& pkg = packages[i];
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(pkg.getParcelId())));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(pkg.getSenderName())));
        table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(pkg.getReceiverName())));

        QString typeText;
        double price = 0.0;
        switch (pkg.getParcelType()) {
            case ParcelType::NORMAL:
                typeText = "普通快递";
                price = 5.0 * pkg.getWeight();
                break;
            case ParcelType::FRAGILE:
                typeText = "易碎品";
                price = 8.0 * pkg.getWeight();
                break;
            case ParcelType::BOOK:
                typeText = "书籍";
                price = 2.0 * pkg.getWeight();
                break;
            default:
                typeText = "未知";
                break;
        }
        table->setItem(i, 3, new QTableWidgetItem(typeText));
        table->setItem(i, 4, new QTableWidgetItem(QString::number(pkg.getWeight(), 'f', 2)));
        table->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(pkg.getDescription())));
        table->setItem(i, 6, new QTableWidgetItem(QString::fromStdString(pkg.getCourierName())));
        table->setItem(i, 7, new QTableWidgetItem(QString("¥ %1").arg(price * 0.5, 0, 'f', 2)));
        table->setRowHidden(i, false);
    }
}

void CourierHomePage::take_package() {
    auto selected = table->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择要揽收的快递");
        return;
    }

    std::set<int> rows;
    for (auto item : selected) {
        rows.insert(item->row());
    }

    std::vector<std::string> parcelIds;
    for (int row : rows) {
        if (auto cell = table->item(row, 0)) {
            parcelIds.push_back(cell->text().toStdString());
        }
    }

    if (parcelIds.empty()) {
        QMessageBox::warning(this, "提示", "请选择有效的快递");
        return;
    }

    std::vector<std::string> collected;
    ErrorCode colRes = system->collectParcels(parcelIds, collected);
    if (colRes == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
        colRes = system->collectParcels(parcelIds, collected);
    }
    if (colRes == ErrorCode::SUCCESS) {
        if (!collected.empty()) {
            QMessageBox::information(this, "成功", QString("已揽收 %1 件快递！").arg(collected.size()));
            load_packages();
            if (query_page) query_page->refresh();
        } else {
            QMessageBox::warning(this, "失败", "揽收失败，请检查快递状态");
        }
    } else {
        QString msg;
        switch (colRes) {
            case ErrorCode::USER_NOT_FOUND:
                msg = "揽收失败：快递员账号不存在";
                break;
            case ErrorCode::NO_RESULT:
                msg = "揽收失败：所选快递不满足揽收条件（非待揽收状态或无可用佣金）";
                break;
            case ErrorCode::INVALID_ARGS:
                msg = "揽收失败：响应数据异常";
                break;
            case ErrorCode::INTERNAL_ERROR:
                msg = "揽收失败：网络连接异常，请重试";
                break;
            default:
                msg = QString("揽收失败（错误码 %1）").arg(static_cast<int>(colRes));
                break;
        }
        QMessageBox::critical(this, "揽收失败", msg);
    }
}