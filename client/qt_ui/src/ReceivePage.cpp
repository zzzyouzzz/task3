#include "ReceivePage.h"


ReceivePage::ReceivePage(QWidget *parent, std::string username, Communication* sys) : QWidget(parent), username(username), system(sys){
    setStyleSheet("background:#F5F7FA;");
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(40,30,40,30);
    lay->setSpacing(18);

    auto title = new QLabel("📥 收快递");
    title->setStyleSheet("font-size:20px; font-weight:bold; color:#2C3E50;");

    // 表格 - 显示待签收快递
    table = new QTableWidget;
    table->setColumnCount(6);  // 物流单号, 寄件人, 物品内容, 发送时间, 快递员, 状态
    table->setHorizontalHeaderLabels({"物流单号", "寄件人", "物品内容", "发送时间", "快递员", "状态"});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);  // 只读
    table->setSelectionMode(QAbstractItemView::MultiSelection);
    table->setColumnWidth(0, 130);
    table->setColumnWidth(1, 55);
    table->setColumnWidth(2, 75);
    table->setColumnWidth(3, 150);
    table->setColumnWidth(4, 55);
    table->setColumnWidth(5, 50);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    table->setStyleSheet(R"(
        QTableWidget{
            background:white;
            border:1px solid #DCDFE6;
            border-radius:6px;
            font-size:14px;
            min-height:300px;
        }
        QHeaderView::section{
            background-color:#E8EDF2;
            padding:8px;
            font-weight:bold;
        }
    )");

    auto btn = new QPushButton("确认签收选中快递");
    btn->setStyleSheet("QPushButton{background:#3498DB; color:white; border-radius:6px; padding:10px; font-size:14px;}");
    
    refreshBtn = new QPushButton("🔄 刷新");
    refreshBtn->setStyleSheet("QPushButton{background:#27AE60; color:white; border-radius:6px; padding:10px; font-size:14px;}");

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(btn);
    buttonLayout->addWidget(refreshBtn);
    buttonLayout->addStretch();

    lay->addWidget(title);
    lay->addWidget(table);
    lay->addLayout(buttonLayout);
    lay->addStretch();

    connect(btn, &QPushButton::clicked, this, &ReceivePage::receive_packages);
    connect(refreshBtn, &QPushButton::clicked, this, &ReceivePage::load_packages);
    
    // 加载待签收快递
    load_packages();
}

void ReceivePage::load_packages() {
    // 使用 LogisticsSystem 查询快递
    std::vector<Parcel> parcels;
    system->queryParcels("", "", username, "", ParcelStatus::PENDING_SIGN, 0, 0, parcels);
    std::vector<Parcel> pending_collect_parcels;
    system->queryParcels("", "", username, "", ParcelStatus::PENDING_COLLECTION, 0, 0, pending_collect_parcels);
    parcels.insert(parcels.end(), pending_collect_parcels.begin(), pending_collect_parcels.end());

    table->setRowCount(parcels.size());
    for (size_t i = 0; i < parcels.size(); i++) {
        const auto& pkg = parcels[i];
        
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(pkg.getParcelId())));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(pkg.getSenderName())));
        table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(pkg.getDescription())));

        time_t sendTime = pkg.getSendTime();
        QDateTime sendDateTime = QDateTime::fromSecsSinceEpoch(sendTime);
        QString sendTimeStr = sendDateTime.toString("yyyy-MM-dd hh:mm:ss");

        table->setItem(i, 3, new QTableWidgetItem(sendTimeStr));
        
        table->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(pkg.getCourierName())));
        QString statusText = pkg.getStatus() == ParcelStatus::PENDING_SIGN ? "待签收" : "待揽收";
        table->setItem(i, 5, new QTableWidgetItem(statusText));
    }
}
    
void ReceivePage::receive_packages() {
    QList<QTableWidgetSelectionRange> selected = table->selectedRanges();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择要签收的快递");
        return;
    }
    
    int received_count = 0;
    
    // 获取所有选中的行
    QSet<int> selectedRows;
    for (const auto& range : selected) {
        for (int row = range.topRow(); row <= range.bottomRow(); row++) {
            selectedRows.insert(row);
        }
    }
    
    for (int row : selectedRows) {
        // 获取物流单号（第一列）
        QTableWidgetItem* trackingItem = table->item(row, 0);
        if (trackingItem) {
            std::string parcelId = trackingItem->text().toStdString();
            
            std::vector<std::string> signedList;
            if (system->signParcels({parcelId}, signedList)) {
                if (!signedList.empty()) {
                    received_count++;
                }
            }
        }
    }
    
    if (received_count > 0) {
        QMessageBox::information(this, "成功", QString("成功签收 %1 个快递").arg(received_count));
        load_packages();
        if(queryPage) queryPage->refresh();
    } else {
        ErrorCode ec = system->getLastError();
        QString msg;
        switch (ec) {
            case ErrorCode::NO_RESULT:
                msg = "没有可签收的快递（可能已签收或不属于你）";
                break;
            default:
                msg = "签收失败，请重试";
                break;
        }
        QMessageBox::warning(this, "签收失败", msg);
    }
}

void ReceivePage::setQueryPage(QueryPage* qP) {
    queryPage = qP;
}