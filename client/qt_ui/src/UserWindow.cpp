#include "UserWindow.h"

UserWindow::UserWindow(std::string username, Communication* sys)  : username(username), system(sys ? sys : new Communication()) {
    setWindowTitle("用户端 - 快递系统");
    setMinimumSize(850,600);
    auto c = new QWidget; 
    setCentralWidget(c);
    auto mainLayout = new QHBoxLayout(c);
    mainLayout->setContentsMargins(0,0,0,0);

    auto nav = new QListWidget;
    nav->setFixedWidth(180);
    nav->addItem("寄快递");
    nav->addItem("收快递");
    nav->addItem("查询快递");
    nav->addItem("账户管理");
    nav->setStyleSheet(R"(
        QListWidget{background:#2C3E50; color:white; font-size:15px;}
        QListWidget::item{height:45px; padding-left:15px;}
        QListWidget::item:selected{background:#3498DB;}
    )");

    auto stack = new QStackedWidget;
    auto RP = new ReceivePage(nullptr, username, system);
    auto QP = new QueryPage(nullptr, username, system);
    auto SP = new SendPage(nullptr, username, system);
    auto AP = new AccountPage(nullptr, username, system);
    SP->setQueryPage(QP);
    RP->setQueryPage(QP);
    SP->setAccountPage(AP);
    stack->addWidget(SP);
    stack->addWidget(RP);
    stack->addWidget(QP);
    stack->addWidget(AP);
    
    connect(nav, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);

    mainLayout->addWidget(nav);
    mainLayout->addWidget(stack);
}