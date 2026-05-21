#include "QueryPage.h"
#include "ReconnectHelper.h"


using namespace std;

QueryPage::QueryPage(QWidget *parent, std::string username, Communication* sys) : QWidget(parent), username(username), system(sys) {
    setStyleSheet("background:#F5F7FA;");
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(40,30,40,30);
    lay->setSpacing(15);

    auto title = new QLabel("🔍 快递查询");
    title->setStyleSheet("font-size:20px; font-weight:bold; color:#2C3E50;");

    // 筛选表单
    auto filterLayout = new QHBoxLayout;
    editTrackingNum = new QLineEdit; editTrackingNum->setPlaceholderText("运单号");
    editSender = new QLineEdit; editSender->setPlaceholderText("寄件人");
    editReceiver = new QLineEdit; editReceiver->setPlaceholderText("收件人");
    statusBox = new QComboBox; statusBox->addItems({"全部", "待处理", "待签收", "已签收"});
    
    auto filterBtn = new QPushButton("筛选");
    filterBtn->setStyleSheet("QPushButton{background:#3498DB; color:white; border-radius:4px; padding:6px;}");
    
    refreshBtn = new QPushButton("🔄 刷新");
    refreshBtn->setStyleSheet("QPushButton{background:#27AE60; color:white; border-radius:4px; padding:6px;}");
    
    filterLayout->addWidget(new QLabel("运单号:"));
    filterLayout->addWidget(editTrackingNum);
    filterLayout->addWidget(new QLabel("寄件人:"));
    filterLayout->addWidget(editSender);
    filterLayout->addWidget(new QLabel("收件人:"));
    filterLayout->addWidget(editReceiver);
    filterLayout->addWidget(new QLabel("状态:"));
    filterLayout->addWidget(statusBox);
    filterLayout->addWidget(filterBtn);
    filterLayout->addWidget(refreshBtn);

    // 表格 - 只读
    table = new QTableWidget;
    table->setColumnCount(8);  // 运单号, 寄件人, 收件人, 物品描述, 快递员, 发送时间, 接受时间, 状态
    table->setHorizontalHeaderLabels({"运单号", "寄件人", "收件人", "物品描述", "快递员", "发送时间", "接受时间", "状态"});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);  // 只读
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setColumnWidth(0, 140);
    table->setColumnWidth(1, 55);
    table->setColumnWidth(2, 55);
    table->setColumnWidth(3, 75);
    table->setColumnWidth(4, 55);
    table->setColumnWidth(7, 50);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setStyleSheet(R"(
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
        QTableWidget{
            selection-background-color: #e3f2fd;
            selection-color: #1976d2;
        }
        QTableWidget{
            show-decoration-selected: 0;
        }
    )");

    lay->addWidget(title);
    lay->addLayout(filterLayout);
    lay->addWidget(table);

    connect(filterBtn, &QPushButton::clicked, this, &QueryPage::load_packages);
    connect(refreshBtn, &QPushButton::clicked, this, &QueryPage::load_packages);
    connect(editTrackingNum, &QLineEdit::textChanged, this, &QueryPage::load_packages);
    connect(editSender, &QLineEdit::textChanged, this, &QueryPage::load_packages);
    connect(editReceiver, &QLineEdit::textChanged, this, &QueryPage::load_packages);
    connect(statusBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QueryPage::load_packages);
    
    load_packages();
}

void QueryPage::load_packages() {
    // 读取当前用户类型，用于展示相关快递
    std::vector<User*> users;
    bool isCourier = false;
    bool isAdmin = false;
    ErrorCode queryUserRes = system->queryUsers(username, UserType::CUSTOMER, users);
    if (queryUserRes == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
        queryUserRes = system->queryUsers(username, UserType::CUSTOMER, users);
    }
    if (queryUserRes == ErrorCode::SUCCESS && !users.empty()) {
        UserType type = users.front()->getUserType();
        isCourier = (type == UserType::COURIER);
        isAdmin = (type == UserType::ADMINISTRATOR);
    } else if (queryUserRes != ErrorCode::SUCCESS) {
        // 非关键错误，继续执行，按默认客户模式查询
        g_logger.warning("QueryPage: queryUsers returned " + std::to_string(static_cast<int>(queryUserRes)));
    }

    QString senderFilter = editSender->text().trimmed();
    QString receiverFilter = editReceiver->text().trimmed();
    QString trackingFilter = editTrackingNum->text().trimmed();
    int statusIndex = statusBox->currentIndex();
    ParcelStatus status = ParcelStatus::OTHER;
    if (statusIndex == 1) status = ParcelStatus::PENDING_COLLECTION;
    else if (statusIndex == 2) status = ParcelStatus::PENDING_SIGN;
    else if (statusIndex == 3) status = ParcelStatus::SIGNED;

    std::vector<Parcel> parcels;
    if (isCourier) {
        ErrorCode qe = system->queryParcels("", senderFilter.toStdString(), receiverFilter.toStdString(), username, status, 0, 0, parcels);
        if (qe == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
            system->queryParcels("", senderFilter.toStdString(), receiverFilter.toStdString(), username, status, 0, 0, parcels);
        }
    } else if (isAdmin) {
        ErrorCode qe = system->queryParcels("", senderFilter.toStdString(), receiverFilter.toStdString(), "", status, 0, 0, parcels);
        if (qe == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
            system->queryParcels("", senderFilter.toStdString(), receiverFilter.toStdString(), "", status, 0, 0, parcels);
        }
    } else {
        std::vector<Parcel> allParcels;
        ErrorCode qe = system->queryParcels("", senderFilter.toStdString(), receiverFilter.toStdString(), "", status, 0, 0, allParcels);
        if (qe == ErrorCode::INTERNAL_ERROR && tryReconnect(system, this)) {
            system->queryParcels("", senderFilter.toStdString(), receiverFilter.toStdString(), "", status, 0, 0, allParcels);
        }
        for (auto& pkg : allParcels) {
            if (pkg.getSenderName() == username || pkg.getReceiverName() == username) {
                parcels.push_back(pkg);
            }
        }
    }

    table->setRowCount(parcels.size());
    for (size_t i = 0; i < parcels.size(); i++) {
        const auto& pkg = parcels[i];
        
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(pkg.getParcelId())));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(pkg.getSenderName())));
        table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(pkg.getReceiverName())));
        table->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(pkg.getDescription())));
        table->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(pkg.getCourierName())));

        time_t sendTime = pkg.getSendTime();
        QDateTime sendDateTime = QDateTime::fromSecsSinceEpoch(sendTime);
        QString sendTimeStr = sendDateTime.toString("yyyy-MM-dd hh:mm:ss");
        table->setItem(i, 5, new QTableWidgetItem(sendTimeStr));
        
        time_t receiveTime = pkg.getReceiveTime();
        QDateTime receiveDateTime = QDateTime::fromSecsSinceEpoch(receiveTime);
        QString receiveTimeStr = receiveDateTime.toString("yyyy-MM-dd hh:mm:ss");
        table->setItem(i, 6, new QTableWidgetItem(receiveTimeStr));

        QString status_text;
        switch (pkg.getStatus()) {
            case ParcelStatus::PENDING_COLLECTION: status_text = "待处理"; break;
            case ParcelStatus::PENDING_SIGN: status_text = "待签收"; break;
            case ParcelStatus::SIGNED: status_text = "已签收"; break;
            default: status_text = "其他"; break;
        }
        table->setItem(i, 7, new QTableWidgetItem(status_text));
    }
}

void QueryPage::filter_packages() {
    QString tracking_num_filter = editTrackingNum->text();
    QString sender_filter = editSender->text();
    QString receiver_filter = editReceiver->text();
    int status_index = statusBox->currentIndex();
    
    for (int i = 0; i < table->rowCount(); i++) {
        bool show = true;
        
        // 运单号筛选
        if (!tracking_num_filter.isEmpty()) {
            show = table->item(i, 0)->text().contains(tracking_num_filter);
        }
        
        // 发送人筛选
        if (show && !sender_filter.isEmpty()) {
            show = table->item(i, 1)->text().contains(sender_filter);
        }
        
        // 收件人筛选
        if (show && !receiver_filter.isEmpty()) {
            show = table->item(i, 2)->text().contains(receiver_filter);
        }
        
        // 状态筛选
        if (show && status_index > 0) {
            QString status_text = table->item(i, 6)->text();
            QString target_status = statusBox->currentText();
            show = (status_text == target_status);
        }
        
        table->setRowHidden(i, !show);
    }
}