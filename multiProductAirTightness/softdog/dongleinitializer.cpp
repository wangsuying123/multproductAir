#include <QApplication>
#include "dongleinitializerwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("加密狗授权工具");
    a.setApplicationVersion("2.0");
    
    // 设置应用样式
    a.setStyle("Fusion");
    
    DongleInitializerWindow window;
    window.show();
    
    return a.exec();
}
