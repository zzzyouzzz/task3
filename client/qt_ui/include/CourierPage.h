#pragma once
#include <string>
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QHeaderView>
#include "QueryPage.h"
#include <QPushButton>
#include "Communication.h"

// ------------------------------
// 快递员端（新增）
// ------------------------------
class CourierHomePage : public QWidget {
    Q_OBJECT
    QTableWidget *table;          // 待揽收快递列表
    QLabel *balanceLabel;         // 余额显示标签
    QPushButton *refreshBtn;      // 刷新按钮
    std::string courier_id;        // 快递员用户名
    QueryPage *query_page;        // 关联的查询页面
    Communication* system;        // 通信对象
    
public:
    CourierHomePage(QWidget *p=nullptr, std::string courier_id="", QueryPage *query_page=nullptr, Communication* sys=nullptr);
    // 刷新列表
    void refresh() { load_packages(); }
private:
    void load_packages();         // 加载待揽收快递
    void take_package();          // 揽收选中的快递
};