#include "CourierWindow.h"



CourierWindow::CourierWindow(const std::string& courier_id, Communication* sys) : courier_id(courier_id), system(sys) {
    setWindowTitle("快递员端 - 快递系统");
    setMinimumSize(900,600);
    auto c = new QWidget; setCentralWidget(c);
    auto mainLayout = new QHBoxLayout(c);
    mainLayout->setContentsMargins(0,0,0,0);

    auto nav = new QListWidget;
    nav->setFixedWidth(180);
    nav->addItem("揽收快递");
    nav->addItem("查询快递");
    nav->setStyleSheet(R"(
        QListWidget{background:#2C3E50; color:white; font-size:15px;}
        QListWidget::item{height:45px; padding-left:15px;}
        QListWidget::item:selected{background:#3498DB;}
    )");

    auto stack = new QStackedWidget;
    auto query_page = new QueryPage(nullptr, courier_id, system);
    auto home_page = new CourierHomePage(nullptr, courier_id, query_page, system);
    
    stack->addWidget(home_page);
    stack->addWidget(query_page);

    connect(nav,&QListWidget::currentRowChanged,stack,&QStackedWidget::setCurrentIndex);

    mainLayout->addWidget(nav);
    mainLayout->addWidget(stack);
}