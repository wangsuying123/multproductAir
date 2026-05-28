#include <QApplication>
#include "machinelockauthorizer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("机器锁定授权工具");
    a.setApplicationVersion("1.0");
    
    // 设置应用样式
    a.setStyle("Fusion");
    
    MachineLockAuthorizer window;
    window.setWindowState(Qt::WindowNoState);  // 确保不是全屏或最大化状态
    window.show();
    
    return a.exec();
}
