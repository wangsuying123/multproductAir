#include "mainwindow.h"
#include "databasemanager.h"
#include "logmanager.h"
#include "splashscreen.h"
#include "machinelock/machinelockmanager.h"

#include <QApplication>
#include <QIcon>
#include <QDebug>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QTimer>
#include <QProcess>

// 全局frpc进程指针
static QProcess *frpcProcess = nullptr;

// 自定义消息处理器，过滤Qt StyleSheet警告
void customMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    // 过滤掉Qt StyleSheet不支持的CSS属性警告
    if (msg.contains("Unknown property") || 
        msg.contains("box-shadow") ||
        msg.contains("text-shadow") ||
        msg.contains("transition") ||
        msg.contains("backdrop-filter") ||
        msg.contains("overflow")) {
        return; // 忽略这些警告
    }
    
    // 其他消息正常输出
    QString txt;
    switch (type) {
    case QtDebugMsg:
        txt = QString("Debug: %1").arg(msg);
        break;
    case QtInfoMsg:
        txt = QString("Info: %1").arg(msg);
        break;
    case QtWarningMsg:
        txt = QString("Warning: %1").arg(msg);
        break;
    case QtCriticalMsg:
        txt = QString("Critical: %1").arg(msg);
        break;
    case QtFatalMsg:
        txt = QString("Fatal: %1").arg(msg);
        break;
    }
    
    // 输出到控制台
    fprintf(stderr, "%s\n", txt.toLocal8Bit().constData());
}

// 启动frpc内网穿透服务
void startFrpcService()
{
    // 获取程序所在目录
    QString appDir = QCoreApplication::applicationDirPath();
    QString frpDir = appDir + "/frp";
    QString frpcPath = frpDir + "/frpc.exe";
    QString configPath = frpDir + "/frpc.toml";
    
    LOG_DEBUG("程序目录: " + appDir, "FRP服务");
    LOG_DEBUG("FRP目录: " + frpDir, "FRP服务");
    
    // 如果程序目录下没有frp文件夹，尝试使用源码目录（开发调试用）
    if (!QFile::exists(frpcPath)) {
        // 尝试源码目录
        QString srcFrpDir = "E:/shenhua/study_notepad/QTcode/SingleAirTightNess/frp";
        if (QFile::exists(srcFrpDir + "/frpc.exe")) {
            frpDir = srcFrpDir;
            frpcPath = frpDir + "/frpc.exe";
            configPath = frpDir + "/frpc.toml";
            LOG_DEBUG("使用源码目录FRP: " + frpDir, "FRP服务");
        }
    }
    
    // 检查frpc.exe是否存在
    if (!QFile::exists(frpcPath)) {
        LOG_WARNING("frpc.exe不存在: " + frpcPath, "FRP服务");
        return;
    }
    
    // 检查配置文件是否存在
    if (!QFile::exists(configPath)) {
        LOG_WARNING("frpc.toml配置文件不存在: " + configPath, "FRP服务");
        return;
    }
    
    LOG_INFO("正在启动frpc: " + frpcPath, "FRP服务");
    
    // 创建进程
    frpcProcess = new QProcess();
    frpcProcess->setWorkingDirectory(frpDir);
    
    // 启动frpc
    QStringList args;
    args << "-c" << "frpc.toml";
    frpcProcess->start(frpcPath, args);
    
    if (frpcProcess->waitForStarted(3000)) {
        LOG_INFO("frpc内网穿透服务已启动, PID: " + QString::number(frpcProcess->processId()), "FRP服务");
    } else {
        LOG_ERROR("frpc启动失败: " + frpcProcess->errorString(), "FRP服务");
        delete frpcProcess;
        frpcProcess = nullptr;
    }
}

// 停止frpc服务
void stopFrpcService()
{
    if (frpcProcess) {
        frpcProcess->terminate();
        if (!frpcProcess->waitForFinished(3000)) {
            frpcProcess->kill();
        }
        delete frpcProcess;
        frpcProcess = nullptr;
        LOG_INFO("frpc内网穿透服务已停止", "FRP服务");
    }
}

// 设置开机自启动
void setAutoStart(bool enable)
{
    QString appName = "SingleAirTightNess";
    QString appPath = QCoreApplication::applicationFilePath();
    appPath = QDir::toNativeSeparators(appPath);
    
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    
    if (enable) {
        reg.setValue(appName, QString("\"%1\"").arg(appPath));
        LOG_INFO("已设置开机自启动", "主程序");
    } else {
        reg.remove(appName);
        LOG_INFO("已取消开机自启动", "主程序");
    }
}

int main(int argc, char *argv[])
{
    // 安装自定义消息处理器，过滤StyleSheet警告
    qInstallMessageHandler(customMessageHandler);
    
    QApplication a(argc, argv);
    
    // 设置应用程序图标
    a.setWindowIcon(QIcon(":/app_icon.ico"));
    
    // 创建并显示启动界面
    SplashScreen *splash = new SplashScreen();
    splash->show();
    a.processEvents();
    
    // 模拟加载过程
    splash->setMessage("正在初始化日志系统...");
    splash->setProgress(10);
    a.processEvents();
    
    // 初始化日志系统
    LogManager* logManager = LogManager::getInstance();
    if (!logManager->init()) {
        qDebug() << "日志系统初始化失败";
    } else {
        LOG_INFO("应用程序已启动", "主程序");
    }
    
    splash->setMessage("正在连接数据库...");
    splash->setProgress(30);
    a.processEvents();
    
    // 初始化数据库
    DatabaseManager* dbManager = DatabaseManager::getInstance();
    // 使用AppData目录的数据库路径
    QString dbPath = DatabaseManager::getDatabasePath();
    if (!dbManager->connectDatabase(dbPath)) {
        LOG_ERROR("数据库连接失败: " + dbManager->getLastError(), "数据库");
    } else {
        LOG_INFO("数据库连接成功", "数据库");
        
        splash->setMessage("正在初始化数据库...");
        splash->setProgress(50);
        a.processEvents();
        
        if (!dbManager->initializeDatabase()) {
            LOG_ERROR("数据库初始化失败: " + dbManager->getLastError(), "数据库");
        } else {
            LOG_INFO("数据库初始化成功", "数据库");
        }
    }
    
    splash->setMessage("正在加载系统组件...");
    splash->setProgress(70);
    a.processEvents();
    
    // 启动frpc内网穿透服务
    startFrpcService();
    
    splash->setMessage("正在验证机器授权...");
    splash->setProgress(75);
    a.processEvents();
    
    // 开发模式开关：设为true跳过机器授权验证
    bool debugMode = false;  // 发布时改为 false
    
    if (!debugMode) {
        // 初始化机器锁定管理器
        MachineLockManager lockManager;
        
        // 验证当前机器是否已授权
        if (!lockManager.verifyMachineLicense()) {
            LOG_ERROR("机器授权验证失败", "机器锁定");
            splash->showError("授权验证失败", 
                "程序未在授权的计算机上运行！\n"
                "请联系管理员获取授权。");
            return a.exec();
        }
        
        LOG_INFO("机器授权验证通过", "机器锁定");
        
        // 记录授权信息
        QString licenseInfo = lockManager.getLicenseInfo();
        LOG_INFO(licenseInfo, "机器锁定");
    } else {
        LOG_WARNING("开发模式：跳过机器授权验证", "机器锁定");
    }
    
    splash->setMessage("正在初始化主界面...");
    splash->setProgress(90);
    a.processEvents();
    
    MainWindow *w = new MainWindow();
    
    splash->setMessage("启动完成！");
    splash->setProgress(100);
    a.processEvents();
    
    // 延迟关闭启动界面并显示主窗口
    QTimer::singleShot(500, [splash, w]() {
        // 连接启动界面关闭信号
        QObject::connect(splash, &SplashScreen::loadingFinished, [w]() {
            // 设置全屏显示
            w->showFullScreen();
            
            // 主窗口完全显示后，初始化登录流程
            QTimer::singleShot(300, w, [w]() {
                w->initializeLogin();
            });
        });
        splash->finish();
    });
    
    // 设置开机自启动
    setAutoStart(true);
    
    LOG_INFO("主窗口已全屏显示", "主程序");
    
    int result = a.exec();
    
    // 停止frpc服务
    stopFrpcService();
    
    delete w;
    
    LOG_INFO("应用程序退出，退出码: " + QString::number(result), "主程序");
    return result;
}
