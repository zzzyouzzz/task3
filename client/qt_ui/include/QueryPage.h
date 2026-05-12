#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <string>
#include <QHeaderView>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDateTime>
#include "Communication.h"

// ------------------------------
// 查询快递页面
// ------------------------------
class QueryPage : public QWidget {
    QTableWidget* table;          // 快递查询结果表格
    QLineEdit* editTrackingNum;   // 运单号搜索框
    QLineEdit* editSender;        // 寄件人搜索框
    QLineEdit* editReceiver;      // 收件人搜索框
    QComboBox* statusBox;         // 状态筛选下拉框
    QPushButton* refreshBtn;      // 刷新按钮
    std::string username;          // 当前用户
    Communication* system;        // 通信对象
    Q_OBJECT
public:
    explicit QueryPage(QWidget *parent = nullptr, std::string username="", Communication* sys=nullptr);
    void refresh() { load_packages(); }
private slots:
    void load_packages();         // 加载快递列表
    void filter_packages();       // 按条件筛选
};