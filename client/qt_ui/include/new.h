#pragma once
#include <string>
#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QFormLayout>
#include <QInputDialog>
#include "Communication.h"


// 全局样式
namespace GlobalStyle
{
    const QString MainBg     = "#F5F7FA";
    const QString NavBg      = "#2C3E50";
    const QString NavText    = "#ECF0F1";
    const QString NavChecked = "#3498DB";
    const QString CardBg     = "#FFFFFF";
    const QString BtnNormal  = "#3498DB";
    const QString BtnHover   = "#2980B9";
    const QString BtnDanger  = "#E74C3C";
    const QString LineBorder = "#DCDFE6";
}



// ------------------------------
// 管理员页面
// ------------------------------
// 管理员—快递管理页面
class AdminExpressPage : public QWidget {
    Q_OBJECT
    QTableWidget *table;          // 快递列表表格
    std::string username;          // 管理员用户名
    Communication* system;        // 通信对象
public:
    AdminExpressPage(QWidget *p=nullptr, std::string username="", Communication* sys=nullptr);
    void refresh() { load_packages(); }
private:
    void load_packages();         // 加载快递列表
    void assign_courier();        // 分配快递员
    void delete_package();        // 删除快递
    void add_package();           // 新增快递
};

// 管理员—用户管理页面
class AdminUserPage : public QWidget {
    Q_OBJECT
    QTableWidget *table;          // 用户列表表格
    std::string username;          // 管理员用户名
    Communication* system;        // 通信对象
public:
    AdminUserPage(QWidget *p=nullptr, std::string username="", Communication* sys=nullptr);
    void refresh() { load_users(); }
private:
    void load_users();            // 加载用户列表
    void edit_user();             // 编辑用户
    void delete_user();           // 删除用户
    void add_user();              // 新增用户
};

// ------------------------------
// 管理员 - 统计页面
// ------------------------------
class AdminStatsPage : public QWidget {
    Q_OBJECT
    QTableWidget* table;        // 统计数据表格
    std::string username;       // 当前管理员用户名
    Communication* system;        // 通信对象
public:
    // 构造函数：初始化统计页面
    AdminStatsPage(QWidget* p = nullptr, std::string username = "", Communication* sys = nullptr);
    // 刷新页面
    void refresh() { load_stats(); }

private:
    void load_stats();   // 加载统计数据
};



// ------------------------------
// 三个主窗口
// ------------------------------

// 管理员主窗口
class AdminWindow : public QMainWindow {
    std::string username;          // 管理员用户名
    Communication* system;        // 通信对象
public:
    AdminWindow(std::string username, Communication* sys=nullptr);
};


