#include "aboutpage.h"
#include "ui_aboutpage.h"

AboutPage::AboutPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AboutPage)
{
    ui->setupUi(this);
    
    // 移除固定几何尺寸，确保页面能正确适应窗口大小变化
    this->setGeometry(QRect());
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 设置布局管理器的属性，确保它能正确适应窗口大小变化
    if (this->layout()) {
        this->layout()->setSizeConstraint(QLayout::SetNoConstraint);
        this->layout()->setContentsMargins(5, 5, 5, 5);
        this->layout()->setSpacing(5);
    }
}

AboutPage::~AboutPage()
{
    delete ui;
}
