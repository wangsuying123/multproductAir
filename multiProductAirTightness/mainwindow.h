#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include "tcpcommunicationmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class CommsPage;
class AirtightParamSetting;
class RealTimeMonitor;
class AboutPage;
class SystemSetting;
class MainControlSetting;
class UserManagement;
class TestResultshow;
class DatabaseManager;
class Login;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    DatabaseManager *getDatabaseManager() const;
    
    // 初始化登录流程 - 在主窗口完全显示后调用
    void initializeLogin();

private slots:
    void showCommunicationsPage();
    void showAirtightParamSettingPage();
    void showMainControlSettingPage();
    void showRealtimeMonitorPage();
    void showTestResultshowPage();
    void showAboutPage();
    void showSystemSettingPage();
    void showUserManagementPage();
    void onAllDevicesConnected();
    void onLoginSuccess(const QString& username, const QString& role);
    void onLogout();
    void toggleFullscreen();
    void onNavigationItemClicked(QListWidgetItem *item);

private:
    Ui::MainWindow *ui;
    QListWidget *navigationMenu;  // 竖向导航菜单
    CommsPage *commsPage;
    AirtightParamSetting *airtightParamPage;
    MainControlSetting *mainControlSettingPage;
    RealTimeMonitor *realtimeMonitorPage;
    AboutPage *aboutPage;
    SystemSetting *systemSettingPage;
    UserManagement *userManagementPage;
    TestResultshow *testResultshowPage;
    DatabaseManager *databaseManager;
    Login *loginPage;
    bool isLoggedIn;
    QString currentUsername;
    QString currentUserRole;
    QTimer *loginDelayTimer;
    
    // TCP通信管理器
    TcpCommunicationManager *m_tcpCommunicationManager;
    
    // 设备连接状态缓存（用于防止重复发送）
    bool m_lastAirTightConnected = false;
    bool m_lastMainBoardConnected = false;
    bool m_lastPressureRegulatorConnected = false;
    
    // 登录管理方法
    void createLoginPage();
    void cleanupLoginPage();
    
    // 导航菜单相关方法
    void setupNavigationMenu();
    void applyNavigationMenuStyle();
    QString getTooltipForMenuItem(const QString &text);
};

#endif // MAINWINDOW_H
