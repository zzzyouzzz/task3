#pragma once
#include <string>
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <vector>
#include <string>
#include <QMessageBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QAbstractItemView>
#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>
#include "QueryPage.h"
#include "Communication.h"
// ------------------------------
// 接收快递页面
// ------------------------------
class ReceivePage : public QWidget {
    QTableWidget* table;          // 快递列表表格
    QueryPage* queryPage;         // 关联的查询页面
    QPushButton* refreshBtn;      // 刷新按钮
    std::string username;          // 当前用户
    Communication* system;        // 通信对象
    
    Q_OBJECT
public:
    ReceivePage(QWidget *parent=nullptr, std::string username="", Communication* sys=nullptr);
    void setQueryPage(QueryPage* qP);
    void refresh() { load_packages(); }
    ~ReceivePage() = default;
private slots:
    void load_packages();         // 加载待签收/待揽收列表
    void receive_packages();      // 签收选中的快递
};