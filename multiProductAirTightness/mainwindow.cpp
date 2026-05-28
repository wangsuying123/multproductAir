#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "commspage.h"
#include "airtightparamsetting.h"
#include "mainControlSetting.h"
#include "realtimemonitor.h"
#include "aboutpage.h"
#include "systemsetting.h"
#include "usermanagement.h"
#include "testresultshow.h"
#include "databasemanager.h"
#include "logmanager.h"
#include "login.h"
#include <QScreen>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QMenuBar>
#include <QTimer>
#include <QMessageBox>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_tcpCommunicationManager(nullptr)
{
    ui->setupUi(this);
    
    // 隐藏菜单栏
    ui->menuBar->setVisible(false);
    
    // 设置窗口居中显示
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - this->width()) / 2;
    int y = (screenGeometry.height() - this->height()) / 2;
    this->move(x, y);
    
    // 设置主窗口的最小大小，防止窗口过小导致内容不可见
    this->setMinimumSize(800, 600);
    
    // 获取 centralWidget 的现有布局并清理
    QLayout *oldLayout = ui->centralWidget->layout();
    if (oldLayout) {
        QLayoutItem *item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            // 不删除 widget，只是从布局中移除
        }
        delete oldLayout;
    }
    
    // 创建新的 QHBoxLayout 作为主布局
    QHBoxLayout *mainLayout = new QHBoxLayout(ui->centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 调用 setupNavigationMenu() 创建导航菜单
    setupNavigationMenu();
    
    // 将 navigationMenu 和 stackedWidget 添加到主布局
    mainLayout->addWidget(navigationMenu);
    mainLayout->addWidget(ui->stackedWidget);
    
    // 设置 stackedWidget 的大小策略为 Expanding
    ui->stackedWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 初始化数据库
    databaseManager = DatabaseManager::getInstance();
    // 使用程序所在目录的绝对路径，解决开机自启动时工作目录不正确的问题
    QString dbPath = QCoreApplication::applicationDirPath() + "/airtight.db";
    if (!databaseManager->connectDatabase(dbPath)) {
        LOG_ERROR("数据库连接失败", "主窗口");
    } else {
        LOG_INFO("数据库连接成功", "主窗口");
    }
    if (!databaseManager->initializeDatabase()) {
        LOG_ERROR("数据库初始化失败", "主窗口");
    } else {
        LOG_INFO("数据库初始化成功", "主窗口");
    }
    
    // 初始化TCP通信管理器
    m_tcpCommunicationManager = new TcpCommunicationManager(this);
    m_tcpCommunicationManager->initialize();
    
    // 创建所有页面实例
    commsPage = new CommsPage(this);
    airtightParamPage = new AirtightParamSetting(this);
    mainControlSettingPage = new MainControlSetting(this);
    realtimeMonitorPage = new RealTimeMonitor(this);
    aboutPage = new AboutPage(this);
    
    // 设置RealTimeMonitor实例到MainControlSetting
    mainControlSettingPage->setRealTimeMonitor(realtimeMonitorPage);
    
    // 设置TCP通信管理器到MainControlSetting
    mainControlSettingPage->setTcpCommunicationManager(m_tcpCommunicationManager);
    
    // 设置MainControlSetting实例到AirtightParamSetting
    airtightParamPage->setMainControlSetting(mainControlSettingPage);
    airtightParamPage->setRealTimeMonitor(realtimeMonitorPage);
    realtimeMonitorPage->setMainControlSetting(mainControlSettingPage);

    // 检测到0006寄存器为1（新测试开始）时，清空RealTimeMonitor的测试结果和图表
    connect(airtightParamPage, &AirtightParamSetting::testStarted,
            realtimeMonitorPage, &RealTimeMonitor::clearTestResult);
    
    // 连接测试完成信号，用于多通道测试
    connect(realtimeMonitorPage, &RealTimeMonitor::updateTestResult,
            airtightParamPage, &AirtightParamSetting::onTestCompleted);

    systemSettingPage = new SystemSetting(this);
    connect(systemSettingPage, &SystemSetting::canceled, this, &MainWindow::showMainControlSettingPage);
    userManagementPage = new UserManagement(this);
    testResultshowPage = new TestResultshow(this);
    
    // 设置 RealtimeMonitor 实例到 TestResultshow，用于自动刷新功能
    testResultshowPage->setRealtimeMonitor(realtimeMonitorPage);
    
    // 设置页面的大小策略，确保它们能正确自适应
    commsPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    airtightParamPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainControlSettingPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    realtimeMonitorPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    aboutPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    systemSettingPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    userManagementPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    testResultshowPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 添加所有页面到stackedWidget
    ui->stackedWidget->addWidget(commsPage);
    ui->stackedWidget->addWidget(airtightParamPage);
    ui->stackedWidget->addWidget(mainControlSettingPage);
    ui->stackedWidget->addWidget(realtimeMonitorPage);
    ui->stackedWidget->addWidget(aboutPage);
    ui->stackedWidget->addWidget(systemSettingPage);
    ui->stackedWidget->addWidget(userManagementPage);
    ui->stackedWidget->addWidget(testResultshowPage);
    
    // 设置stackedWidget的大小策略
    ui->stackedWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 默认显示实时监控页面（需要先登录）
    ui->stackedWidget->setCurrentWidget(realtimeMonitorPage);
    
    // 连接通信状态变化信号
    connect(commsPage, &CommsPage::airTightConnectionChanged, airtightParamPage, &AirtightParamSetting::onAirTightConnectionChanged);
    connect(commsPage, &CommsPage::mainBoardConnectionChanged, airtightParamPage, &AirtightParamSetting::onMainBoardConnectionChanged);
    connect(commsPage, &CommsPage::pressureRegulatorConnectionChanged, airtightParamPage, &AirtightParamSetting::onPressureRegulatorConnectionChanged);
    
    // 连接主控板参数设置页面的通信状态变化信号
    connect(commsPage, &CommsPage::airTightConnectionChanged, mainControlSettingPage, &MainControlSetting::onAirTightConnectionChanged);
    connect(commsPage, &CommsPage::mainBoardConnectionChanged, mainControlSettingPage, &MainControlSetting::onMainBoardConnectionChanged);
    connect(commsPage, &CommsPage::pressureRegulatorConnectionChanged, mainControlSettingPage, &MainControlSetting::onPressureRegulatorConnectionChanged);
    
    // 连接Modbus客户端变化信号
    connect(commsPage, &CommsPage::airTightModbusClientChanged, airtightParamPage, &AirtightParamSetting::setModbusClient);
    connect(commsPage, &CommsPage::mainBoardModbusClientChanged, airtightParamPage, &AirtightParamSetting::setMainBoardModbusClient);
    connect(commsPage, &CommsPage::pressureRegulatorModbusClientChanged, airtightParamPage, &AirtightParamSetting::setPressureRegulatorModbusClient);
    
    // 连接主控板参数设置页面的Modbus客户端变化信号
    connect(commsPage, &CommsPage::airTightModbusClientChanged, mainControlSettingPage, [this](QModbusClient *client) {
        mainControlSettingPage->setModbusClient(client);
        mainControlSettingPage->setAirtightModbusClient(client);
    });
    connect(commsPage, &CommsPage::mainBoardModbusClientChanged, mainControlSettingPage, [this](QModbusClient *client) {
        mainControlSettingPage->setMainBoardModbusClient(client);
    });
    connect(commsPage, &CommsPage::pressureRegulatorModbusClientChanged, mainControlSettingPage, [this](QModbusClient *client) {
        mainControlSettingPage->setPressureRegulatorModbusClient(client);
    });
    
    // 连接从站ID变化信号
    connect(commsPage, &CommsPage::airTightSlaveIdChanged, airtightParamPage, &AirtightParamSetting::setSlaveId);
    connect(commsPage, &CommsPage::mainBoardSlaveIdChanged, airtightParamPage, &AirtightParamSetting::setMainBoardSlaveId);
    connect(commsPage, &CommsPage::pressureRegulatorSlaveIdChanged, airtightParamPage, &AirtightParamSetting::setPressureRegulatorSlaveId);
    
    // 连接主控板参数设置页面的从站ID变化信号
    connect(commsPage, &CommsPage::airTightSlaveIdChanged, mainControlSettingPage, &MainControlSetting::setSlaveId);
    connect(commsPage, &CommsPage::mainBoardSlaveIdChanged, mainControlSettingPage, &MainControlSetting::setSlaveId);
    connect(commsPage, &CommsPage::pressureRegulatorSlaveIdChanged, mainControlSettingPage, &MainControlSetting::setPressureRegulatorSlaveId);
    

    
    // 连接实时监控页面的Modbus客户端变化信号
    connect(commsPage, &CommsPage::airTightModbusClientChanged, realtimeMonitorPage, &RealTimeMonitor::setAirTightModbusClient);
    connect(commsPage, &CommsPage::mainBoardModbusClientChanged, realtimeMonitorPage, &RealTimeMonitor::setMainBoardModbusClient);
    connect(commsPage, &CommsPage::pressureRegulatorModbusClientChanged, realtimeMonitorPage, &RealTimeMonitor::setPressureRegulatorModbusClient);
    
    // 连接实时监控页面的从站ID变化信号
    connect(commsPage, &CommsPage::airTightSlaveIdChanged, realtimeMonitorPage, &RealTimeMonitor::setAirTightSlaveId);
    connect(commsPage, &CommsPage::mainBoardSlaveIdChanged, realtimeMonitorPage, &RealTimeMonitor::setMainBoardSlaveId);
    connect(commsPage, &CommsPage::pressureRegulatorSlaveIdChanged, realtimeMonitorPage, &RealTimeMonitor::setPressureRegulatorSlaveId);
    
    // 初始化Modbus客户端（因为CommsPage构造函数中的自动连接在信号连接之前执行，需要手动初始化）
    // 这确保了即使设备在程序启动时自动连接，MainControlSetting也能获取到Modbus客户端
    if (commsPage->getAirTightModbusClient()) {
        mainControlSettingPage->setModbusClient(commsPage->getAirTightModbusClient());
        mainControlSettingPage->setAirtightModbusClient(commsPage->getAirTightModbusClient());
        realtimeMonitorPage->setAirTightModbusClient(commsPage->getAirTightModbusClient());
        airtightParamPage->setModbusClient(commsPage->getAirTightModbusClient());
    }
    if (commsPage->getMainBoardModbusClient()) {
        mainControlSettingPage->setMainBoardModbusClient(commsPage->getMainBoardModbusClient());
        realtimeMonitorPage->setMainBoardModbusClient(commsPage->getMainBoardModbusClient());
        airtightParamPage->setMainBoardModbusClient(commsPage->getMainBoardModbusClient());
    }
    if (commsPage->getPressureRegulatorModbusClient()) {
        mainControlSettingPage->setPressureRegulatorModbusClient(commsPage->getPressureRegulatorModbusClient());
        realtimeMonitorPage->setPressureRegulatorModbusClient(commsPage->getPressureRegulatorModbusClient());
        airtightParamPage->setPressureRegulatorModbusClient(commsPage->getPressureRegulatorModbusClient());
    }
    // 初始化从站ID
    mainControlSettingPage->setSlaveId(commsPage->getAirTightSlaveId());
    mainControlSettingPage->setMainBoardSlaveId(commsPage->getMainBoardSlaveId());
    mainControlSettingPage->setPressureRegulatorSlaveId(commsPage->getPressureRegulatorSlaveId());
    realtimeMonitorPage->setAirTightSlaveId(commsPage->getAirTightSlaveId());
    realtimeMonitorPage->setMainBoardSlaveId(commsPage->getMainBoardSlaveId());
    realtimeMonitorPage->setPressureRegulatorSlaveId(commsPage->getPressureRegulatorSlaveId());
    airtightParamPage->setSlaveId(commsPage->getAirTightSlaveId());
    airtightParamPage->setMainBoardSlaveId(commsPage->getMainBoardSlaveId());
    airtightParamPage->setPressureRegulatorSlaveId(commsPage->getPressureRegulatorSlaveId());
    
    LOG_INFO("Modbus客户端初始化完成", "主窗口");
    
    // 延迟初始化连接状态，确保所有客户端都已正确设置和连接
    QTimer::singleShot(2000, this, [this]() {
        // 强制刷新主控板设置页面的连接状态
        this->mainControlSettingPage->refreshMainBoardConnectionStatus();
        this->mainControlSettingPage->onAirTightConnectionChanged(this->commsPage->getAirTightConnected());
        this->mainControlSettingPage->onMainBoardConnectionChanged(this->commsPage->getMainBoardConnected());
        this->mainControlSettingPage->onPressureRegulatorConnectionChanged(this->commsPage->getPressureRegulatorConnected());
        

        
        // 同步 airtightParamPage 的连接状态（开机自启动时设备已连接但未触发信号）
        this->airtightParamPage->onAirTightConnectionChanged(this->commsPage->getAirTightConnected());
        this->airtightParamPage->onMainBoardConnectionChanged(this->commsPage->getMainBoardConnected());
        this->airtightParamPage->onPressureRegulatorConnectionChanged(this->commsPage->getPressureRegulatorConnected());
        
        // 如果程序号未设置（默认为0），主动触发程序号1的初始化，更新实时监控页面的参数面板
        if (this->mainControlSettingPage->getProgramNumber() == 0) {
            this->realtimeMonitorPage->onProgramNumberReceived(1);
        }
        
        LOG_INFO(QString("延迟状态初始化完成 - 气密仪:%1, 主控板:%2, 调压装置:%3")
                .arg(this->commsPage->getAirTightConnected() ? "已连接" : "未连接")
                .arg(this->commsPage->getMainBoardConnected() ? "已连接" : "未连接")
                .arg(this->commsPage->getPressureRegulatorConnected() ? "已连接" : "未连接"), "主窗口");
    });
    
    // 初始化登录相关变量
    isLoggedIn = false;
    loginPage = nullptr;
    loginDelayTimer = new QTimer(this);
    
    // 连接所有设备连接成功信号
    connect(commsPage, &CommsPage::allDevicesConnected, this, &MainWindow::onAllDevicesConnected);
    
    // 启动TCP服务器
    m_tcpCommunicationManager->startServer();
    
    // 延时2秒后发送测试数据，验证TCP连接
    QTimer::singleShot(2000, this, [this]() {
        if (m_tcpCommunicationManager && m_tcpCommunicationManager->isServerRunning()) {
            m_tcpCommunicationManager->sendTestData();
        }
    });
    
    // 创建定时器，每5秒发送一次设备连接状态（无论设备是否连接）
    QTimer *statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, [this]() {
        if (m_tcpCommunicationManager && m_tcpCommunicationManager->isServerRunning() 
            && m_tcpCommunicationManager->getClientCount() > 0) {
            // 发送设备连接状态
            m_tcpCommunicationManager->sendConnectionStatus(
                commsPage->getAirTightConnected(),
                commsPage->getMainBoardConnected(),
                commsPage->getPressureRegulatorConnected()
            );
        }
    });
    statusTimer->start(2000); // 每5秒发送一次
    
    // 当有新客户端连接时，立即发送当前设备状态
    connect(m_tcpCommunicationManager, &TcpCommunicationManager::clientConnected, this, [this](QTcpSocket *) {
        // 延迟100ms发送，确保客户端准备好接收
        QTimer::singleShot(100, this, [this]() {
            if (m_tcpCommunicationManager && m_tcpCommunicationManager->isServerRunning()) {
                // 发送测试数据
                m_tcpCommunicationManager->sendTestData();
                // 发送设备连接状态
                m_tcpCommunicationManager->sendConnectionStatus(
                    commsPage->getAirTightConnected(),
                    commsPage->getMainBoardConnected(),
                    commsPage->getPressureRegulatorConnected()
                );
            }
        });
    });
    
    // 连接实时数据信号到TCP发送（发送实时压力、泄漏值等数据给客户端）
    connect(realtimeMonitorPage, &RealTimeMonitor::realtimeDataUpdated, this,
            [this](double pressure, double leak, const QString &pressureUnit, const QString &leakUnit, const QString &processName) {
        if (m_tcpCommunicationManager && m_tcpCommunicationManager->isServerRunning()) {
            // 获取当前程序号（从mainControlSettingPage获取）
            int programNumber = mainControlSettingPage->getProgramNumber();
            m_tcpCommunicationManager->sendRealtimeData(pressure, leak, pressureUnit, leakUnit, processName, programNumber);
        }
    });

    // 连接测试结果保存信号到TCP发送
    connect(realtimeMonitorPage, &RealTimeMonitor::testResultSaved, this,
            [this](const QMap<QString, QVariant>& testResult) {
        if (m_tcpCommunicationManager && m_tcpCommunicationManager->isServerRunning()) {
            // 构造TestResult结构体（字段名映射：testResultData -> TestResult）
            TestResult result;
            result.programNumber = testResult.value("serial_number").toInt();      // 程序号
            result.channelPressure = testResult.value("pressure_value").toFloat(); // 压力值
            result.pressureUnit = testResult.value("pressure_unit").toString();    // 压力单位
            result.channelLeak = testResult.value("leak_value").toFloat();         // 泄漏值
            result.leakUnit = testResult.value("leak_unit").toString();            // 泄漏单位
            
            // 解析测试结果：test_result字段包含"通过"或"不通过"
            QString testResultStr = testResult.value("test_result").toString();
            result.isPassed = (testResultStr == "通过");
            result.isFailed = (testResultStr == "不通过");
            
            // 解析测试时间
            QString testTimeStr = testResult.value("test_time").toString();
            result.createTime = QDateTime::fromString(testTimeStr, "yyyy-MM-dd HH:mm:ss");
            
            m_tcpCommunicationManager->sendTestResult(result);
        }
    });

    // 连接设备连接状态变化信号到TCP发送（带防抖，只在状态真正变化时发送）
    connect(commsPage, &CommsPage::airTightConnectionChanged, this, [this](bool connected) {
        if (connected != m_lastAirTightConnected) {
            m_lastAirTightConnected = connected;
            if (m_tcpCommunicationManager && m_tcpCommunicationManager->isServerRunning()) {
                m_tcpCommunicationManager->sendConnectionStatus(
                    commsPage->getAirTightConnected(),
                    commsPage->getMainBoardConnected(),
                    commsPage->getPressureRegulatorConnected()
                );
            }
        }
    });
    connect(commsPage, &CommsPage::mainBoardConnectionChanged, this, [this](bool connected) {
        if (connected != m_lastMainBoardConnected) {
            m_lastMainBoardConnected = connected;
            if (m_tcpCommunicationManager && m_tcpCommunicationManager->isServerRunning()) {
                m_tcpCommunicationManager->sendConnectionStatus(
                    commsPage->getAirTightConnected(),
                    commsPage->getMainBoardConnected(),
                    commsPage->getPressureRegulatorConnected()
                );
            }
        }
    });
    connect(commsPage, &CommsPage::pressureRegulatorConnectionChanged, this, [this](bool connected) {
        if (connected != m_lastPressureRegulatorConnected) {
            m_lastPressureRegulatorConnected = connected;
            if (m_tcpCommunicationManager && m_tcpCommunicationManager->isServerRunning()) {
                m_tcpCommunicationManager->sendConnectionStatus(
                    commsPage->getAirTightConnected(),
                    commsPage->getMainBoardConnected(),
                    commsPage->getPressureRegulatorConnected()
                );
            }
        }
    });
    
    LOG_INFO("主窗口构造函数执行完成", "主窗口");
    
    // 不在构造函数中自动显示登录页面
    // 登录页面将在主窗口完全显示后由外部触发
}

void MainWindow::showCommunicationsPage()
{
    ui->stackedWidget->setCurrentWidget(commsPage);
}

void MainWindow::showAirtightParamSettingPage()
{
    // 如果已登录但不是管理员，直接提示权限不足
    if (isLoggedIn && currentUserRole != "管理员") {
        QMessageBox::warning(this, "权限不足", "只有管理员才能访问气密参数设置页面！");
        return;
    }
    
    // 如果未登录，弹出登录页面
    if (!isLoggedIn) {
        createLoginPage();
        
        connect(loginPage, &Login::loginSuccess, this, [this](const QString& username, const QString& role) {
            // 先清理登录页面
            cleanupLoginPage();
            
            // 检查是否是管理员
            if (role != "管理员") {
                QMessageBox::warning(nullptr, "权限不足", "只有管理员才能访问气密参数设置页面！");
                return;
            }
            
            // 设置登录状态
            isLoggedIn = true;
            currentUsername = username;
            currentUserRole = role;
            
            // 更新实时监控页面的用户信息
            realtimeMonitorPage->updateOperatorInfo(username, role);
            
            // 设置测试结果页面的用户角色（用于权限控制）
            testResultshowPage->setCurrentUserRole(role);
            
            // 设置Modbus客户端和从站ID
            airtightParamPage->setModbusClient(commsPage->getAirTightModbusClient());
            airtightParamPage->setSlaveId(commsPage->getAirTightSlaveId());
            
            // 设置主控板Modbus客户端和从站ID
            airtightParamPage->setMainBoardModbusClient(commsPage->getMainBoardModbusClient());
            airtightParamPage->setMainBoardSlaveId(commsPage->getMainBoardSlaveId());
            
            // 设置调压装置Modbus客户端和从站ID
            airtightParamPage->setPressureRegulatorModbusClient(commsPage->getPressureRegulatorModbusClient());
            airtightParamPage->setPressureRegulatorSlaveId(commsPage->getPressureRegulatorSlaveId());
            
            // 更新连接状态
            airtightParamPage->onAirTightConnectionChanged(commsPage->getAirTightConnected());
            airtightParamPage->onMainBoardConnectionChanged(commsPage->getMainBoardConnected());
            airtightParamPage->onPressureRegulatorConnectionChanged(commsPage->getPressureRegulatorConnected());
            
            ui->stackedWidget->setCurrentWidget(airtightParamPage);
        });
        connect(loginPage, &Login::loginCancel, this, [this]() {
            // 登录取消时，清理登录页面
            cleanupLoginPage();
        });
        
        loginPage->show();
        return;
    }
    
    // 设置Modbus客户端和从站ID
    airtightParamPage->setModbusClient(commsPage->getAirTightModbusClient());
    airtightParamPage->setSlaveId(commsPage->getAirTightSlaveId());
    
    // 设置主控板Modbus客户端和从站ID
    airtightParamPage->setMainBoardModbusClient(commsPage->getMainBoardModbusClient());
    airtightParamPage->setMainBoardSlaveId(commsPage->getMainBoardSlaveId());
    
    // 设置调压装置Modbus客户端和从站ID
    airtightParamPage->setPressureRegulatorModbusClient(commsPage->getPressureRegulatorModbusClient());
    airtightParamPage->setPressureRegulatorSlaveId(commsPage->getPressureRegulatorSlaveId());
    
    // 更新连接状态
    airtightParamPage->onAirTightConnectionChanged(commsPage->getAirTightConnected());
    airtightParamPage->onMainBoardConnectionChanged(commsPage->getMainBoardConnected());
    airtightParamPage->onPressureRegulatorConnectionChanged(commsPage->getPressureRegulatorConnected());
    
    ui->stackedWidget->setCurrentWidget(airtightParamPage);
}

void MainWindow::showRealtimeMonitorPage()
{
    // 如果未登录，先弹出登录页面
    if (!isLoggedIn) {
        createLoginPage();
        
        connect(loginPage, &Login::loginSuccess, this, [this](const QString& username, const QString& role) {
            // 先清理登录页面
            cleanupLoginPage();
            
            // 设置登录状态
            isLoggedIn = true;
            currentUsername = username;
            currentUserRole = role;
            
            // 更新实时监控页面的用户信息并跳转
            realtimeMonitorPage->updateOperatorInfo(username, role);
            
            // 设置测试结果页面的用户角色（用于权限控制）
            testResultshowPage->setCurrentUserRole(role);
            

            
            // 设置Modbus客户端
            realtimeMonitorPage->setAirTightModbusClient(commsPage->getAirTightModbusClient());
            realtimeMonitorPage->setMainBoardModbusClient(commsPage->getMainBoardModbusClient());
            realtimeMonitorPage->setPressureRegulatorModbusClient(commsPage->getPressureRegulatorModbusClient());
            
            // 设置从站ID
            realtimeMonitorPage->setAirTightSlaveId(commsPage->getAirTightSlaveId());
            realtimeMonitorPage->setMainBoardSlaveId(commsPage->getMainBoardSlaveId());
            realtimeMonitorPage->setPressureRegulatorSlaveId(commsPage->getPressureRegulatorSlaveId());
            

            
            ui->stackedWidget->setCurrentWidget(realtimeMonitorPage);
        });
        connect(loginPage, &Login::loginCancel, this, [this]() {
            cleanupLoginPage();
        });
        
        loginPage->show();
        return;
    }
    

    
    // 设置Modbus客户端
    realtimeMonitorPage->setAirTightModbusClient(commsPage->getAirTightModbusClient());
    realtimeMonitorPage->setMainBoardModbusClient(commsPage->getMainBoardModbusClient());
    realtimeMonitorPage->setPressureRegulatorModbusClient(commsPage->getPressureRegulatorModbusClient());
    
    // 设置从站ID
    realtimeMonitorPage->setAirTightSlaveId(commsPage->getAirTightSlaveId());
    realtimeMonitorPage->setMainBoardSlaveId(commsPage->getMainBoardSlaveId());
    realtimeMonitorPage->setPressureRegulatorSlaveId(commsPage->getPressureRegulatorSlaveId());
    

    
    // 更新用户信息
    realtimeMonitorPage->updateOperatorInfo(currentUsername, currentUserRole);
    
    ui->stackedWidget->setCurrentWidget(realtimeMonitorPage);
}



void MainWindow::showSystemSettingPage()
{
    ui->stackedWidget->setCurrentWidget(systemSettingPage);
}

void MainWindow::showAboutPage()
{
    ui->stackedWidget->setCurrentWidget(aboutPage);
}

void MainWindow::showMainControlSettingPage()
{
    // 设置Modbus客户端和从站ID
    mainControlSettingPage->setModbusClient(commsPage->getMainBoardModbusClient());
    mainControlSettingPage->setSlaveId(commsPage->getMainBoardSlaveId());
    
    // 设置气密仪Modbus客户端和从站ID
    mainControlSettingPage->setAirtightModbusClient(commsPage->getAirTightModbusClient());
    
    // 设置主控板Modbus客户端
    mainControlSettingPage->setMainBoardModbusClient(commsPage->getMainBoardModbusClient());
    
    // 设置调压装置Modbus客户端和从站ID
    mainControlSettingPage->setPressureRegulatorModbusClient(commsPage->getPressureRegulatorModbusClient());
    mainControlSettingPage->setPressureRegulatorSlaveId(commsPage->getPressureRegulatorSlaveId());
    
    // 更新连接状态
    mainControlSettingPage->onAirTightConnectionChanged(commsPage->getAirTightConnected());
    mainControlSettingPage->onMainBoardConnectionChanged(commsPage->getMainBoardConnected());
    mainControlSettingPage->onPressureRegulatorConnectionChanged(commsPage->getPressureRegulatorConnected());
    
    ui->stackedWidget->setCurrentWidget(mainControlSettingPage);
}

void MainWindow::showUserManagementPage()
{
    // 如果已登录但不是管理员，直接提示权限不足
    if (isLoggedIn && currentUserRole != "管理员") {
        QMessageBox::warning(this, "权限不足", "只有管理员才能访问用户管理页面！");
        return;
    }
    
    // 如果未登录，弹出登录页面
    if (!isLoggedIn) {
        createLoginPage();
        
        connect(loginPage, &Login::loginSuccess, this, [this](const QString& username, const QString& role) {
            // 先清理登录页面
            cleanupLoginPage();
            
            // 检查是否是管理员
            if (role != "管理员") {
                QMessageBox::warning(nullptr, "权限不足", "只有管理员才能访问用户管理页面！");
                return;
            }
            
            // 设置登录状态
            isLoggedIn = true;
            currentUsername = username;
            currentUserRole = role;
            
            // 更新实时监控页面的用户信息
            realtimeMonitorPage->updateOperatorInfo(username, role);
            
            // 设置测试结果页面的用户角色（用于权限控制）
            testResultshowPage->setCurrentUserRole(role);
            
            ui->stackedWidget->setCurrentWidget(userManagementPage);
        });
        connect(loginPage, &Login::loginCancel, this, [this]() {
            cleanupLoginPage();
        });
        
        loginPage->show();
        return;
    }
    
    ui->stackedWidget->setCurrentWidget(userManagementPage);
}

void MainWindow::showTestResultshowPage()
{
    ui->stackedWidget->setCurrentWidget(testResultshowPage);
}

void MainWindow::onAllDevicesConnected()
{
    // 所有设备连接成功
    LOG_INFO("所有设备连接成功", "主窗口");
}

void MainWindow::onLoginSuccess(const QString& username, const QString& role)
{
    // 登录成功
    isLoggedIn = true;
    currentUsername = username;
    currentUserRole = role;
    
    // 更新实时监控页面的用户信息
    realtimeMonitorPage->updateOperatorInfo(username, role);
    
    // 设置测试结果页面的用户角色（用于权限控制）
    testResultshowPage->setCurrentUserRole(role);
    
    // 关闭登录页面
    if (loginPage) {
        loginPage->close();
        delete loginPage;
        loginPage = nullptr;
    }
    
    // 停止定时器
    loginDelayTimer->stop();
}

void MainWindow::onLogout()
{
    // 退出登录
    isLoggedIn = false;
    currentUsername = "";
    currentUserRole = "";
    
    // 清理登录页面（重要：确保下次能重新创建）
    cleanupLoginPage();
    
    // 清空实时监控页面的用户信息
    realtimeMonitorPage->updateOperatorInfo("", "");
    
    // 清空测试结果页面的用户角色
    testResultshowPage->setCurrentUserRole("");
    
    // 跳转到通讯连接页面
    ui->stackedWidget->setCurrentWidget(commsPage);
    
    LOG_INFO("用户已退出登录", "主窗口");
    QMessageBox::information(this, "提示", "已成功退出登录");
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        // 退出全屏，恢复正常窗口
        showNormal();
        // 更新导航菜单项文本
        if (navigationMenu && navigationMenu->count() > 9) {
            navigationMenu->item(9)->setText("全屏显示");
        }
        LOG_INFO("已退出全屏模式", "主窗口");
    } else {
        // 进入全屏
        showFullScreen();
        // 更新导航菜单项文本
        if (navigationMenu && navigationMenu->count() > 9) {
            navigationMenu->item(9)->setText("退出全屏");
        }
        LOG_INFO("已进入全屏模式", "主窗口");
    }
}

MainWindow::~MainWindow()
{
    // 释放TCP通信管理器资源
    delete m_tcpCommunicationManager;
    
    // 清理登录页面
    cleanupLoginPage();
    
    delete loginDelayTimer;
    delete ui;
}

DatabaseManager *MainWindow::getDatabaseManager() const
{
    return databaseManager;
}

void MainWindow::initializeLogin()
{
    // 在主窗口完全显示后，延迟一小段时间再显示登录页面
    // 确保所有UI组件都已准备就绪
    QTimer::singleShot(200, this, [this]() {
        showRealtimeMonitorPage();
    });
}

void MainWindow::createLoginPage()
{
    if (loginPage) {
        // 如果登录页面已存在，先清理
        cleanupLoginPage();
    }
    
    loginPage = new Login(nullptr);
    loginPage->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    loginPage->setWindowModality(Qt::ApplicationModal);
}

void MainWindow::cleanupLoginPage()
{
    if (loginPage) {
        loginPage->close();
        delete loginPage;
        loginPage = nullptr;
    }
}

void MainWindow::setupNavigationMenu()
{
    // 创建导航菜单
    navigationMenu = new QListWidget(this);
    navigationMenu->setMinimumWidth(180);
    navigationMenu->setMaximumWidth(220);
    navigationMenu->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    
    // 添加所有菜单项
    QStringList menuTexts = {
        "通讯连接",
        "气密仪参数设置",
        "主控板参数设置",
        "实时监测数据",
        "测试结果查询",
        "系统设置",
        "用户管理",
        "关于我们",
        "退出登录",
        "退出全屏"
    };
    
    for (const QString &text : menuTexts) {
        QListWidgetItem *item = new QListWidgetItem(text);
        item->setToolTip(getTooltipForMenuItem(text));
        navigationMenu->addItem(item);
    }
    
    // 连接信号
    connect(navigationMenu, &QListWidget::itemClicked, 
            this, &MainWindow::onNavigationItemClicked);
    
    // 应用样式
    applyNavigationMenuStyle();
}

void MainWindow::onNavigationItemClicked(QListWidgetItem *item)
{
    int index = navigationMenu->row(item);
    
    switch (index) {
        case 0: // 通讯连接
            showCommunicationsPage();
            break;
        case 1: // 气密仪参数设置
            showAirtightParamSettingPage();
            break;
        case 2: // 主控板参数设置
            showMainControlSettingPage();
            break;
        case 3: // 实时监测数据
            showRealtimeMonitorPage();
            break;
        case 4: // 测试结果查询
            showTestResultshowPage();
            break;
        case 5: // 系统设置
            showSystemSettingPage();
            break;
        case 6: // 用户管理
            showUserManagementPage();
            break;
        case 7: // 关于我们
            showAboutPage();
            break;
        case 8: // 退出登录
            onLogout();
            break;
        case 9: // 退出全屏
            toggleFullscreen();
            break;
    }
}

QString MainWindow::getTooltipForMenuItem(const QString &text)
{
    if (text == "通讯连接") {
        return "配置设备通讯连接";
    } else if (text == "气密仪参数设置") {
        return "设置气密仪测试参数（需要管理员权限）";
    } else if (text == "主控板参数设置") {
        return "配置主控板和气缸阀门控制";
    } else if (text == "实时监测数据") {
        return "查看实时测试数据和状态";
    } else if (text == "测试结果查询") {
        return "查询历史测试结果记录";
    } else if (text == "系统设置") {
        return "系统参数配置";
    } else if (text == "用户管理") {
        return "管理系统用户（需要管理员权限）";
    } else if (text == "关于我们") {
        return "查看系统信息和关于我们";
    } else if (text == "退出登录") {
        return "退出当前登录账户";
    } else if (text == "退出全屏") {
        return "切换全屏显示模式";
    }
    return "";
}

void MainWindow::applyNavigationMenuStyle()
{
    QString styleSheet = R"(
        QListWidget {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                stop:0 #112d4e, stop:0.5 #1e5f74, stop:1 #112d4e);
            color: #e2e8f0;
            border: none;
            border-right: 2px solid #3282b8;
            font-family: 'Microsoft YaHei UI', 'SimHei', sans-serif;
            font-size: 20px;
            font-weight: bold;
            padding: 10px 0;
        }
        
        QListWidget::item {
            background: transparent;
            color: #e2e8f0;
            padding: 15px 20px;
            margin: 5px 10px;
            border-radius: 8px;
            border: 1px solid transparent;
        }
        
        QListWidget::item:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #3282b8, stop:1 #1e5f74);
            color: #ffffff;
            border: 1px solid #64d2ff;
        }
        
        QListWidget::item:selected {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #42a5f5, stop:1 #1e88e5);
            color: #ffffff;
            border: 1px solid #64d2ff;
        }
    )";
    
    navigationMenu->setStyleSheet(styleSheet);
}
