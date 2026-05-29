#include "mainControlSetting.h"
#include "ui_mainControlSetting.h"
#include "realtimemonitor.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QEventLoop>
#include <QThread>
#include <QSettings>
#include "databasemanager.h"
#include "airtightparamsetting.h"
#include "tcpcommunicationmanager.h"
#include "logmanager.h"
#include "enum/leakUnit.h"
#include "enum/VolumeEncoding.h"

MainControlSetting::MainControlSetting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainControlSetting),
    m_airTightConnected(false),
    m_mainBoardConnected(false),
    m_pressureRegulatorConnected(false),
    m_testChannel1(false),
    m_testChannel2(false),
    m_testChannel3(false),
    modbusClient(nullptr),
    airtightModbusClient(nullptr),
    mainBoardModbusClient(nullptr),
    pressureRegulatorModbusClient(nullptr),
    slaveId(1),
    mainBoardSlaveId(1),
    pressureRegulatorSlaveId(1),
    m_tcpCommunicationManager(nullptr),
    m_realTimeMonitor(nullptr),
    m_programNumber1(1),
    m_programNumber2(1),
    m_programNumber3(1)
{
    ui->setupUi(this);
    
    // 移除固定几何尺寸，确保页面能正确适应窗口大小变化
    this->setGeometry(QRect());
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 设置布局管理器的属性，确保它能正确适应窗口大小变化
    this->layout()->setSizeConstraint(QLayout::SetNoConstraint);
    this->layout()->setContentsMargins(5, 5, 5, 5);
    this->layout()->setSpacing(5);
    
    // 从数据库加载测试通道配置
    QList<QMap<QString, QVariant>> channelConfigs = DatabaseManager::getInstance()->getAllTestChannelConfigs();
    for (const QMap<QString, QVariant>& config : channelConfigs) {
        int channelNumber = config["channel_number"].toInt();
        bool isEnabled = config["is_enabled"].toInt() == 1;
        int programNumber = config["program_number"].toInt();
        
        switch (channelNumber) {
        case 1:
            m_testChannel1 = isEnabled;
            m_programNumber1 = programNumber;
            ui->programNumberComboBox1->setCurrentIndex(programNumber - 1);
            break;
        case 2:
            m_testChannel2 = isEnabled;
            m_programNumber2 = programNumber;
            ui->programNumberComboBox2->setCurrentIndex(programNumber - 1);
            break;
        case 3:
            m_testChannel3 = isEnabled;
            m_programNumber3 = programNumber;
            ui->programNumberComboBox3->setCurrentIndex(programNumber - 1);
            break;
        }
    }
    
    // 更新按钮状态颜色
    updateChannelButtonStates();
    
    // 连接信号槽
    connect(ui->testChannel1OpenButton, &QPushButton::clicked, this, &MainControlSetting::onTestChannel1OpenButtonClicked);
    connect(ui->testChannel1CloseButton, &QPushButton::clicked, this, &MainControlSetting::onTestChannel1CloseButtonClicked);
    connect(ui->testChannel2OpenButton, &QPushButton::clicked, this, &MainControlSetting::onTestChannel2OpenButtonClicked);
    connect(ui->testChannel2CloseButton, &QPushButton::clicked, this, &MainControlSetting::onTestChannel2CloseButtonClicked);
    connect(ui->testChannel3OpenButton, &QPushButton::clicked, this, &MainControlSetting::onTestChannel3OpenButtonClicked);
    connect(ui->testChannel3CloseButton, &QPushButton::clicked, this, &MainControlSetting::onTestChannel3CloseButtonClicked);
    
    // 程序号下拉框改变时保存到数据库
    connect(ui->programNumberComboBox1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainControlSetting::onProgramNumberComboBox1Changed);
    connect(ui->programNumberComboBox2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainControlSetting::onProgramNumberComboBox2Changed);
    connect(ui->programNumberComboBox3, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainControlSetting::onProgramNumberComboBox3Changed);
}

MainControlSetting::~MainControlSetting()
{
    delete ui;
}

void MainControlSetting::setModbusClient(QModbusClient *client)
{
    modbusClient = client;
}

void MainControlSetting::setAirtightModbusClient(QModbusClient *client)
{
    // 如果已经有旧的客户端，断开连接状态信号
    if (airtightModbusClient) {
        disconnect(airtightModbusClient, &QModbusClient::stateChanged, this, nullptr);
    }
    
    airtightModbusClient = client;
    
    // 监听新客户端的连接状态变化
    if (client) {
        // 初始设置连接状态
        m_airTightConnected = (client->state() == QModbusDevice::ConnectedState);
        // 连接状态变化信号
        connect(client, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            m_airTightConnected = (state == QModbusDevice::ConnectedState);
            LOG_DEBUG(QString("气密仪连接状态变化: %1").arg(state == QModbusDevice::ConnectedState ? "已连接" : "未连接"), "主控设置");
        });
    } else {
        // 客户端为空，设置为未连接状态
        m_airTightConnected = false;
    }
}

void MainControlSetting::setMainBoardModbusClient(QModbusClient *client)
{
    // 如果已经有旧的客户端，断开连接状态信号
    if (mainBoardModbusClient) {
        disconnect(mainBoardModbusClient, &QModbusClient::stateChanged, this, nullptr);
    }
    
    mainBoardModbusClient = client;
    
    // 监听新客户端的连接状态变化
    if (client) {
        // 初始设置连接状态
        m_mainBoardConnected = (client->state() == QModbusDevice::ConnectedState);
        LOG_DEBUG(QString("设置主控板Modbus客户端，初始连接状态: %1").arg(m_mainBoardConnected ? "已连接" : "未连接"), "主控设置");
        
        // 连接状态变化信号
        connect(client, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            bool wasConnected = m_mainBoardConnected;
            m_mainBoardConnected = (state == QModbusDevice::ConnectedState);
            
            if (wasConnected != m_mainBoardConnected) {
                LOG_INFO(QString("主控板连接状态变化: %1 -> %2").arg(
                    wasConnected ? "已连接" : "未连接",
                    m_mainBoardConnected ? "已连接" : "未连接"), "主控设置");
                
                // 更新UI状态
                onMainBoardConnectionChanged(m_mainBoardConnected);
            }
        });
        
        // 立即更新UI状态
        onMainBoardConnectionChanged(m_mainBoardConnected);
    } else {
        // 客户端为空，设置为未连接状态
        m_mainBoardConnected = false;
        LOG_DEBUG("主控板Modbus客户端设置为空，连接状态设为未连接", "主控设置");
        onMainBoardConnectionChanged(false);
    }
}

void MainControlSetting::setPressureRegulatorModbusClient(QModbusClient *client)
{
    // 如果已经有旧的客户端，断开连接状态信号
    if (pressureRegulatorModbusClient) {
        disconnect(pressureRegulatorModbusClient, &QModbusClient::stateChanged, this, nullptr);
    }
    
    pressureRegulatorModbusClient = client;
    
    // 监听新客户端的连接状态变化
    if (client) {
        // 初始设置连接状态
        m_pressureRegulatorConnected = (client->state() == QModbusDevice::ConnectedState);
        // 连接状态变化信号
        connect(client, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            m_pressureRegulatorConnected = (state == QModbusDevice::ConnectedState);
            LOG_DEBUG(QString("调压装置连接状态变化: %1").arg(state == QModbusDevice::ConnectedState ? "已连接" : "未连接"), "主控设置");
            // 更新UI状态
            if (m_pressureRegulatorConnected) {
                ui->pressureRegulatorStatusLabel->setText("已连接");
                ui->pressureRegulatorStatusLabel->setStyleSheet("color: green; font-weight: bold;");
            } else {
                ui->pressureRegulatorStatusLabel->setText("未连接");
                ui->pressureRegulatorStatusLabel->setStyleSheet("color: red; font-weight: bold;");
            }
        });
    } else {
        // 客户端为空，设置为未连接状态
        m_pressureRegulatorConnected = false;
        ui->pressureRegulatorStatusLabel->setText("未连接");
        ui->pressureRegulatorStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
}

void MainControlSetting::setSlaveId(quint8 id)
{
    slaveId = id;
}

void MainControlSetting::setPressureRegulatorSlaveId(quint8 id)
{
    pressureRegulatorSlaveId = id;
}

// 设置主控板slaveId（新增）
void MainControlSetting::setMainBoardSlaveId(quint8 id)
{
    mainBoardSlaveId = id;
}

// 设置TCP通信管理器实例
void MainControlSetting::setTcpCommunicationManager(TcpCommunicationManager *tcpManager)
{
    m_tcpCommunicationManager = tcpManager;
}

// 设置RealTimeMonitor实例
void MainControlSetting::setRealTimeMonitor(RealTimeMonitor *monitor)
{
    m_realTimeMonitor = monitor;
}

// 获取当前程序号（返回数据库中保存的程序号，默认返回通道1的程序号）
int MainControlSetting::getProgramNumber()
{
    return m_programNumber1;
}

// 获取指定通道的程序号
int MainControlSetting::getProgramNumber(int channel)
{
    switch (channel) {
    case 1:
        return m_programNumber1;
    case 2:
        return m_programNumber2;
    case 3:
        return m_programNumber3;
    default:
        return 0;
    }
}

// 获取所有开启的通道列表
QList<int> MainControlSetting::getEnabledChannels()
{
    QList<int> enabledChannels;
    LOG_DEBUG(QString("【通道配置】检查通道状态: channel1=%1, channel2=%2, channel3=%3")
              .arg(m_testChannel1 ? "开启" : "关闭").arg(m_testChannel2 ? "开启" : "关闭").arg(m_testChannel3 ? "开启" : "关闭"), "主控设置");
    
    if (m_testChannel1) {
        LOG_DEBUG("【通道配置】通道1已开启，添加到列表", "主控设置");
        enabledChannels.append(1);
    } else {
        LOG_DEBUG("【通道配置】通道1未开启，跳过", "主控设置");
    }
    if (m_testChannel2) {
        LOG_DEBUG("【通道配置】通道2已开启，添加到列表", "主控设置");
        enabledChannels.append(2);
    } else {
        LOG_DEBUG("【通道配置】通道2未开启，跳过", "主控设置");
    }
    if (m_testChannel3) {
        LOG_DEBUG("【通道配置】通道3已开启，添加到列表", "主控设置");
        enabledChannels.append(3);
    } else {
        LOG_DEBUG("【通道配置】通道3未开启，跳过", "主控设置");
    }
    
    QString channelsStr;
    for (int i = 0; i < enabledChannels.size(); ++i) {
        if (i > 0) channelsStr += ", ";
        channelsStr += QString::number(enabledChannels[i]);
    }
    LOG_DEBUG(QString("【通道配置】获取开启通道列表: %1").arg(channelsStr), "主控设置");
    return enabledChannels;
}

// 检查指定通道是否开启
bool MainControlSetting::isChannelEnabled(int channel)
{
    switch (channel) {
    case 1:
        return m_testChannel1;
    case 2:
        return m_testChannel2;
    case 3:
        return m_testChannel3;
    default:
        return false;
    }
}

// 强制刷新主控板连接状态
void MainControlSetting::refreshMainBoardConnectionStatus()
{
    if (mainBoardModbusClient) {
        bool currentState = (mainBoardModbusClient->state() == QModbusDevice::ConnectedState);
        if (currentState != m_mainBoardConnected) {
            LOG_INFO(QString("刷新主控板连接状态: %1 -> %2").arg(
                m_mainBoardConnected ? "已连接" : "未连接",
                currentState ? "已连接" : "未连接"), "主控设置");
            m_mainBoardConnected = currentState;
            onMainBoardConnectionChanged(m_mainBoardConnected);
        }
    } else {
        if (m_mainBoardConnected) {
            LOG_INFO("刷新主控板连接状态: 客户端为空，设置为未连接", "主控设置");
            m_mainBoardConnected = false;
            onMainBoardConnectionChanged(false);
        }
    }
}

void MainControlSetting::onMainBoardConnectionChanged(bool connected)
{
    m_mainBoardConnected = connected;
    if (connected) {
        ui->mainBoardStatusLabel->setText("已连接");
        ui->mainBoardStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        ui->mainBoardStatusLabel->setText("未连接");
        ui->mainBoardStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
}

void MainControlSetting::onAirTightConnectionChanged(bool connected)
{
    m_airTightConnected = connected;
    if (connected) {
        ui->airTightStatusLabel->setText("已连接");
        ui->airTightStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        ui->airTightStatusLabel->setText("未连接");
        ui->airTightStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
}

void MainControlSetting::onPressureRegulatorConnectionChanged(bool connected)
{
    m_pressureRegulatorConnected = connected;
    if (connected) {
        ui->pressureRegulatorStatusLabel->setText("已连接");
        ui->pressureRegulatorStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        ui->pressureRegulatorStatusLabel->setText("未连接");
        ui->pressureRegulatorStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
}

void MainControlSetting::onTestChannel1OpenButtonClicked()
{
    m_testChannel1 = true;
    int programNumber = ui->programNumberComboBox1->currentIndex() + 1;
    qDebug() << "onTestChannel1OpenButtonClicked: programNumber=" << programNumber;
    bool result = DatabaseManager::getInstance()->updateTestChannelConfig(1, true, programNumber);
    qDebug() << "updateTestChannelConfig result:" << result;
    if (!result) {
        qDebug() << "Database error:" << DatabaseManager::getInstance()->getLastError();
    }
    LOG_DEBUG(QString("测试1通道已设置为开启状态，程序号：%1").arg(programNumber), "主控设置");
    updateChannelButtonStates();
}

void MainControlSetting::onTestChannel1CloseButtonClicked()
{
    m_testChannel1 = false;
    int programNumber = ui->programNumberComboBox1->currentIndex() + 1;
    DatabaseManager::getInstance()->updateTestChannelConfig(1, false, programNumber);
    LOG_DEBUG(QString("测试1通道已设置为关闭状态，程序号：%1").arg(programNumber), "主控设置");
    updateChannelButtonStates();
}

void MainControlSetting::onTestChannel2OpenButtonClicked()
{
    m_testChannel2 = true;
    int programNumber = ui->programNumberComboBox2->currentIndex() + 1;
    DatabaseManager::getInstance()->updateTestChannelConfig(2, true, programNumber);
    LOG_DEBUG(QString("测试2通道已设置为开启状态，程序号：%1").arg(programNumber), "主控设置");
    updateChannelButtonStates();
}

void MainControlSetting::onTestChannel2CloseButtonClicked()
{
    m_testChannel2 = false;
    int programNumber = ui->programNumberComboBox2->currentIndex() + 1;
    DatabaseManager::getInstance()->updateTestChannelConfig(2, false, programNumber);
    LOG_DEBUG(QString("测试2通道已设置为关闭状态，程序号：%1").arg(programNumber), "主控设置");
    updateChannelButtonStates();
}

void MainControlSetting::onTestChannel3OpenButtonClicked()
{
    m_testChannel3 = true;
    int programNumber = ui->programNumberComboBox3->currentIndex() + 1;
    DatabaseManager::getInstance()->updateTestChannelConfig(3, true, programNumber);
    LOG_DEBUG(QString("测试3通道已设置为开启状态，程序号：%1").arg(programNumber), "主控设置");
    updateChannelButtonStates();
}

void MainControlSetting::onTestChannel3CloseButtonClicked()
{
    m_testChannel3 = false;
    int programNumber = ui->programNumberComboBox3->currentIndex() + 1;
    DatabaseManager::getInstance()->updateTestChannelConfig(3, false, programNumber);
    LOG_DEBUG(QString("测试3通道已设置为关闭状态，程序号：%1").arg(programNumber), "主控设置");
    updateChannelButtonStates();
}

void MainControlSetting::updateChannelButtonStates()
{
    QString openActiveStyle = 
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y1:1, stop:0 #69f0ae, stop:0.5 #00e676, stop:1 #00c853);"
        "   color: white;"
        "   border: 3px solid #b9f6ca;"
        "   border-radius: 10px;"
        "   padding: 10px 20px;"
        "   font-size: 20px;"
        "   font-weight: bold;"
        "   min-width: 100px;"
        "   min-height: 50px;"
        "   box-shadow: 0 0 25px rgba(0, 230, 118, 0.8), inset 0 1px 0 rgba(255,255,255,0.3);"
        "   text-shadow: 0 1px 2px rgba(0,0,0,0.3);"
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y1:1, stop:0 #81c784, stop:1 #66bb6a);"
        "   border-color: #81c784;"
        "}"
        "QPushButton:pressed {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y1:1, stop:0 #00c853, stop:1 #00a844);"
        "}";

    QString openInactiveStyle = 
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y1:1, stop:0 #37474f, stop:1 #263238);"
        "   color: #607d8b;"
        "   border: 2px solid #455a64;"
        "   border-radius: 10px;"
        "   padding: 10px 20px;"
        "   font-size: 16px;"
        "   font-weight: normal;"
        "   min-width: 100px;"
        "   min-height: 50px;"
        "   box-shadow: inset 0 2px 4px rgba(0,0,0,0.4);"
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y1:1, stop:0 #455a64, stop:1 #37474f);"
        "}";

    QString closeActiveStyle = 
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y1:1, stop:0 #ff8a80, stop:0.5 #ff5252, stop:1 #d32f2f);"
        "   color: white;"
        "   border: 3px solid #ffcdd2;"
        "   border-radius: 10px;"
        "   padding: 10px 20px;"
        "   font-size: 20px;"
        "   font-weight: bold;"
        "   min-width: 100px;"
        "   min-height: 50px;"
        "   box-shadow: 0 0 25px rgba(255, 82, 82, 0.8), inset 0 1px 0 rgba(255,255,255,0.3);"
        "   text-shadow: 0 1px 2px rgba(0,0,0,0.3);"
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y1:1, stop:0 #ef9a9a, stop:1 #e57373);"
        "   border-color: #ef9a9a;"
        "}"
        "QPushButton:pressed {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y1:1, stop:0 #d32f2f, stop:1 #b71c1c);"
        "}";

    QString closeInactiveStyle = 
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y1:1, stop:0 #37474f, stop:1 #263238);"
        "   color: #607d8b;"
        "   border: 2px solid #455a64;"
        "   border-radius: 10px;"
        "   padding: 10px 20px;"
        "   font-size: 16px;"
        "   font-weight: normal;"
        "   min-width: 100px;"
        "   min-height: 50px;"
        "   box-shadow: inset 0 2px 4px rgba(0,0,0,0.4);"
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y1:1, stop:0 #455a64, stop:1 #37474f);"
        "}";

    ui->testChannel1OpenButton->setStyleSheet(m_testChannel1 ? openActiveStyle : openInactiveStyle);
    ui->testChannel1CloseButton->setStyleSheet(m_testChannel1 ? closeInactiveStyle : closeActiveStyle);
    ui->testChannel2OpenButton->setStyleSheet(m_testChannel2 ? openActiveStyle : openInactiveStyle);
    ui->testChannel2CloseButton->setStyleSheet(m_testChannel2 ? closeInactiveStyle : closeActiveStyle);
    ui->testChannel3OpenButton->setStyleSheet(m_testChannel3 ? openActiveStyle : openInactiveStyle);
    ui->testChannel3CloseButton->setStyleSheet(m_testChannel3 ? closeInactiveStyle : closeActiveStyle);
}

void MainControlSetting::onProgramNumberComboBox1Changed(int index)
{
    int programNumber = index + 1;
    m_programNumber1 = programNumber;
    DatabaseManager::getInstance()->updateTestChannelConfig(1, m_testChannel1, programNumber);
    LOG_DEBUG(QString("测试1通道程序号已更改为：%1").arg(programNumber), "主控设置");
}

void MainControlSetting::onProgramNumberComboBox2Changed(int index)
{
    int programNumber = index + 1;
    m_programNumber2 = programNumber;
    DatabaseManager::getInstance()->updateTestChannelConfig(2, m_testChannel2, programNumber);
    LOG_DEBUG(QString("测试2通道程序号已更改为：%1").arg(programNumber), "主控设置");
}

void MainControlSetting::onProgramNumberComboBox3Changed(int index)
{
    int programNumber = index + 1;
    m_programNumber3 = programNumber;
    DatabaseManager::getInstance()->updateTestChannelConfig(3, m_testChannel3, programNumber);
    LOG_DEBUG(QString("测试3通道程序号已更改为：%1").arg(programNumber), "主控设置");
}

bool MainControlSetting::sendModbusCommand(quint16 address, quint16 value, int timeoutMs)
{
    // 使用主控板专用Modbus客户端
    QModbusClient *client = mainBoardModbusClient;
    if (!client) {
        // 如果主控板客户端未设置，使用默认客户端
        client = modbusClient;
    }
    
    if (!client || client->state() != QModbusDevice::ConnectedState) {
        // 不显示警告，因为实时检测会频繁调用
        return false;
    }
    
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, 1);
    writeUnit.setValue(0, value);
    
    // 使用主控板从站ID
    if (auto *reply = client->sendWriteRequest(writeUnit, mainBoardSlaveId)) {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        
        connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        timer.start(timeoutMs);
        loop.exec();
        
        if (timer.isActive()) {
            timer.stop();
            
            if (reply->error() == QModbusDevice::NoError) {
                reply->deleteLater();
                return true;
            } else {
                reply->deleteLater();
                return false;
            }
        } else {
            reply->deleteLater();
            return false;
        }
    } else {
        return false;
    }
}

bool MainControlSetting::readModbusRegister(quint16 address, quint16 &value, int timeoutMs)
{
    // 使用主控板专用Modbus客户端
    QModbusClient *client = mainBoardModbusClient;
    if (!client) {
        // 如果主控板客户端未设置，使用默认客户端
        client = modbusClient;
    }
    
    if (!client || client->state() != QModbusDevice::ConnectedState) {
        // 不显示警告，因为实时检测会频繁调用
        return false;
    }
    
    QModbusDataUnit readUnit(QModbusDataUnit::InputRegisters, address, 1);
    
    // 使用主控板从站ID，而不是气密仪从站ID
    if (auto *reply = client->sendReadRequest(readUnit, mainBoardSlaveId)) {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        
        connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        timer.start(timeoutMs);
        loop.exec();
        
        if (timer.isActive()) {
            timer.stop();
            
            if (reply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit result = reply->result();
                if (result.valueCount() > 0) {
                    value = result.value(0);
                    reply->deleteLater();
                    return true;
                }
            }
            reply->deleteLater();
            return false;
        } else {
            reply->deleteLater();
            return false;
        }
    } else {
        return false;
    }
}



// 启动气密仪方法
bool MainControlSetting::startAirtightTest()
{
    // 检查是否需要扫码
    if (m_realTimeMonitor && !m_realTimeMonitor->canStartTest()) {
        LOG_WARNING("必须扫码但产品编号为空，弹出对话框提示用户输入", "主控设置");
        
        // 弹出输入对话框让用户输入产品编号
        bool ok;
        QString productId = QInputDialog::getText(
            this, 
            "请输入产品编号", 
            "必须扫描或输入产品编号才能启动测试：", 
            QLineEdit::Normal, 
            "", 
            &ok
        );
        
        // 如果用户点击确定并且输入了产品编号
        if (ok && !productId.isEmpty()) {
            // 更新产品编号
            m_realTimeMonitor->updateCurrentProductId(productId);
            LOG_INFO(QString("用户输入了产品编号: %1").arg(productId), "主控设置");
        } else {
            // 用户取消或未输入产品编号
            LOG_WARNING("用户未输入产品编号，取消启动气密仪", "主控设置");
            return false;
        }
    }
    
    // 检查airTightModbusClient是否为空或未连接
    // 使用m_airTightConnected变量来检查连接状态，该变量会通过stateChanged信号自动更新
    LOG_DEBUG(QString("气密仪连接状态：airtightModbusClient=%1, state=%2, m_airTightConnected=%3").arg(
        airtightModbusClient ? "有效" : "无效",
        airtightModbusClient ? QString::number(airtightModbusClient->state()) : "N/A",
        m_airTightConnected ? "已连接" : "未连接"), "主控设置");
    
    if (!airtightModbusClient || !m_airTightConnected) {
        LOG_ERROR("设备未连接，无法启动测试", "主控设置");
        return false;
    }

    // 发送启动命令到设备（向寄存器9472发送值4608）
    // 使用功能码16（Write Multiple Registers）发送启动命令
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 9472, 2);
    writeUnit.setValue(0, 4608);
    writeUnit.setValue(1, 0); // 第二个寄存器设为0，确保使用功能码16
    
    // 使用气密仪Modbus客户端
    QModbusClient *client = airtightModbusClient;
    
    if (auto *reply = client->sendWriteRequest(writeUnit, slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            LOG_INFO("成功发送启动命令到气密仪", "主控设置");
            return true;
        } else {
            delete reply;
            LOG_ERROR("发送启动命令失败", "主控设置");
            return false;
        }
    } else {
        LOG_ERROR("发送启动命令失败", "主控设置");
        return false;
    }
}

// 复位气密仪方法
void MainControlSetting::resetAirtightTest()
{
    // 只有当airtightModbusClient存在且已连接时，才发送复位命令到设备
    LOG_DEBUG(QString("气密仪连接状态：airtightModbusClient=%1, state=%2, m_airTightConnected=%3").arg(
        airtightModbusClient ? "有效" : "无效",
        airtightModbusClient ? QString::number(airtightModbusClient->state()) : "N/A",
        m_airTightConnected ? "已连接" : "未连接"), "主控设置");
    
    if (!airtightModbusClient || !m_airTightConnected) {
        LOG_WARNING("设备未连接，跳过发送复位命令", "主控设置");
        return;
    }

    // 发送复位命令到设备（向寄存器9472发送值4864）
    // 使用功能码16（Write Multiple Registers）发送复位命令
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 9472, 2);
    writeUnit.setValue(0, 4864);
    writeUnit.setValue(1, 0); // 第二个寄存器设为0，确保使用功能码16
    
    LOG_DEBUG(QString("发送复位命令到气密仪，寄存器地址9472，值4864，slaveId=%1").arg(slaveId), "主控设置");
    
    // 使用异步方式发送，与startAirtightTest保持一致
    if (auto *reply = airtightModbusClient->sendWriteRequest(writeUnit, slaveId)) {
        if (!reply->isFinished()) {
            // 连接完成信号，处理结果
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                if (reply->error() == QModbusDevice::NoError) {
                    LOG_INFO("成功发送复位命令到气密仪", "主控设置");
                } else {
                    LOG_WARNING(QString("发送复位命令返回错误: %1").arg(reply->errorString()), "主控设置");
                }
                reply->deleteLater();
            });
        } else {
            // 请求已完成（可能是错误）
            if (reply->error() == QModbusDevice::NoError) {
                LOG_INFO("成功发送复位命令到气密仪（同步完成）", "主控设置");
            } else {
                LOG_WARNING(QString("发送复位命令失败: %1").arg(reply->errorString()), "主控设置");
            }
            delete reply;
        }
    } else {
        LOG_ERROR("sendWriteRequest返回nullptr，无法发送复位命令", "主控设置");
    }
}

// 发送参数给气密仪
bool MainControlSetting::sendParamsToDevice(const QMap<QString, QVariant> params){
    // 使用气密仪专用Modbus客户端
    QModbusClient *client = airtightModbusClient;
    if (!client) {
        // 如果气密仪客户端未设置，使用默认客户端
        client = modbusClient;
    }
    
    if (!client || client->state() != QModbusDevice::ConnectedState) {
        LOG_ERROR("未连接到气密仪设备，无法发送参数", "主控设置");
        return false;
    }
    const int timeoutMs = 2000; // 增加超时时间到2秒
    const int maxRetries = 2; // 最大重试次数
    const quint16 startAddress = 8192; // 起始地址
    const quint16 endAddress = 8229;   // 结束地址
    const int numRegisters = endAddress - startAddress + 1; // 寄存器数量
    
    // 创建批量写入数据单元，从8192到8229共38个寄存器
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, startAddress, numRegisters);
    
    // 初始化所有寄存器值为0
    for (int i = 0; i < numRegisters; ++i) {
        writeUnit.setValue(i, 0);
    }
    
    writeUnit.setValue(8192 - startAddress, 26112);
    writeUnit.setValue(8193 - startAddress, 0);
    
     // 1. 时间参数：保持编码逻辑不变，但保留无符号编码值
    quint16 fillTime = AirtightParamSetting::encodeTimeParameter(params["fill_time"].toUInt());
    quint16 stabilizationTime = AirtightParamSetting::encodeTimeParameter(params["stabilization_time"].toUInt());
    quint16 testTime = AirtightParamSetting::encodeTimeParameter(params["test_time"].toUInt());
    quint16 dumpTime = AirtightParamSetting::encodeTimeParameter(params["dump_time"].toUInt());

    // 写入时间参数
    writeUnit.setValue(8196 - startAddress, fillTime);        // 填充时间
    writeUnit.setValue(8197 - startAddress, stabilizationTime); // 稳定时间
    writeUnit.setValue(8198 - startAddress, testTime);         // 测试时间
    writeUnit.setValue(8199 - startAddress, dumpTime);         // 排放时间

    // 压力单位
    writeUnit.setValue(8200 - startAddress, params["pressure_unit"].toUInt());   // 压力单位

    // 压力值参数
    quint16 minPressure = static_cast<quint16>(params["pressure_min"].toUInt());
    quint16 maxPressure = static_cast<quint16>(params["pressure_max"].toUInt());
    quint16 fillPressure = static_cast<quint16>(params["pressure_set_fill"].toUInt());

    QPair<quint16, quint16> minEnc = AirtightParamSetting::encodePressureParameter(minPressure);
    QPair<quint16, quint16> maxEnc = AirtightParamSetting::encodePressureParameter(maxPressure);
    QPair<quint16, quint16> fillEnc = AirtightParamSetting::encodePressureParameter(fillPressure);

    // 最小压力: 写入8201(低位交换) / 8202(高位交换)
    writeUnit.setValue(8201 - startAddress, minEnc.first);
    writeUnit.setValue(8202 - startAddress, minEnc.second);
    // 最大压力: 写入8203 / 8204
    writeUnit.setValue(8203 - startAddress, maxEnc.first);
    writeUnit.setValue(8204 - startAddress, maxEnc.second);
    // 填充压力: 写入8205 / 8206
    writeUnit.setValue(8205 - startAddress, fillEnc.first);
    writeUnit.setValue(8206 - startAddress, fillEnc.second);

    // 填充类型
    writeUnit.setValue(8211 - startAddress, static_cast<quint16>(params["fill_type"].toUInt()));          // 填充类型

    // 泄漏相关参数
    writeUnit.setValue(8212 - startAddress, static_cast<quint16>(params["leak_unit"].toUInt()));     // 泄漏单位
    writeUnit.setValue(8213 - startAddress, 256);  // 固定256
    writeUnit.setValue(8214 - startAddress, static_cast<quint16>(params["volume_unit"].toUInt()));       // 容积单位
    
    // 容积和其他参数
    writeUnit.setValue(8215 - startAddress, 0);        // 固定值
    
    // 将容积值映射为枚举编码（见VolumeEncoding.h）
    {
        quint16 volumeRaw = static_cast<quint16>(params["volume"].toUInt());
        quint16 volumeEncoded = volumeRaw;
        if (VolumeEncoding::tryEncode(static_cast<int>(volumeRaw), volumeEncoded)) {
            writeUnit.setValue(8216 - startAddress, volumeEncoded);
        } else {
            writeUnit.setValue(8216 - startAddress, volumeRaw);
        }
    }
    
    // 先用单位1编码还原单位，再取单位2编码
    LeakUnit leakUnit = LeakUnitHelper::fromRegister8212(static_cast<int>(params["leak_unit"].toUInt()));
    
    // 计算允许泄露值编码：根据泄露单位使用不同的编码方式
    quint16 allowLeakEncoded8219 = 0;
    quint16 allowLeakEncoded8220 = 0;
    double allowLeakValue = params["test_reject"].toDouble();
    
    if (leakUnit == LeakUnit::Pa) {
        quint32 allowLeakScaled = static_cast<quint32>(allowLeakValue * 2560);
        allowLeakEncoded8219 = static_cast<quint16>(allowLeakScaled % 65535);
        allowLeakEncoded8220 = 0;
    } else if (leakUnit == LeakUnit::MlPerMinute) {
        allowLeakEncoded8220 = static_cast<quint16>((static_cast<int>(allowLeakValue) / 64) * 256);
        quint32 scaled = static_cast<quint32>(allowLeakValue * 256000);
        allowLeakEncoded8219 = static_cast<quint16>((scaled % 65535) - allowLeakEncoded8220);
    } else {
        allowLeakEncoded8219 = static_cast<quint16>(allowLeakValue);
        allowLeakEncoded8220 = 0;
    }
    
    // 计算参考泄露值编码：使用与允许泄漏值相同的公式
    quint16 refLeakEncoded8217 = 0;
    quint16 refLeakEncoded8218 = 0;
    double refLeakValue = params["ref_reject"].toDouble();
    
    if (leakUnit == LeakUnit::Pa) {
        quint32 refLeakScaled = static_cast<quint32>(refLeakValue * 2560);
        refLeakEncoded8217 = static_cast<quint16>(refLeakScaled % 65535);
        refLeakEncoded8218 = 0;
    } else if (leakUnit == LeakUnit::MlPerMinute) {
        refLeakEncoded8218 = static_cast<quint16>((static_cast<int>(refLeakValue) / 64) * 256);
        quint32 scaled = static_cast<quint32>(refLeakValue * 256000);
        refLeakEncoded8217 = static_cast<quint16>((scaled % 65535) - refLeakEncoded8218);
    } else {
        refLeakEncoded8217 = static_cast<quint16>(refLeakValue);
        refLeakEncoded8218 = 0;
    }
    
    writeUnit.setValue(8217 - startAddress, refLeakEncoded8217);  // 参考泄露值低位
    writeUnit.setValue(8218 - startAddress, refLeakEncoded8218);  // 参考泄露值高位
    writeUnit.setValue(8219 - startAddress, allowLeakEncoded8219);  // 允许泄露值低位
    writeUnit.setValue(8220 - startAddress, allowLeakEncoded8220);  // 允许泄露值高位
    writeUnit.setValue(8224 - startAddress, 64);
    writeUnit.setValue(8225 - startAddress, 32068);          // 标准大气压
    writeUnit.setValue(8227 - startAddress, 0);         // 标准温度
    writeUnit.setValue(8229 - startAddress, static_cast<quint16>(params["offset"].toUInt()));          // 偏移量
    
    bool success = false;
    int retryCount = 0;
    
    // 尝试发送参数，最多重试maxRetries次
    while (retryCount <= maxRetries && !success) {
        // 等待设备准备
        QThread::msleep(500);
        
        LOG_DEBUG(QString("尝试发送批量参数，第%1次，从地址%2到%3").arg(retryCount+1).arg(startAddress).arg(endAddress), "主控设置");
        
        QModbusReply *reply = client->sendWriteRequest(writeUnit, slaveId);
        if (!reply) {
            LOG_ERROR("sendWriteRequest返回nullptr", "主控设置");
            retryCount++;
            continue;
        }

        // 等待回复或超时
        QEventLoop loop;
        connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);

        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);

        loop.exec();
        
        if (timer.isActive()) {
            // 正常收到回复
            timer.stop();

            if (reply->error() != QModbusDevice::NoError) {
                LOG_ERROR(QString("发送命令失败: %1").arg(reply->errorString()), "主控设置");
                retryCount++;
            } else {
                LOG_INFO(QString("批量参数发送成功 - 从地址 %1 到 %2").arg(startAddress).arg(endAddress), "主控设置");
                success = true;
            }
            reply->deleteLater();
        } else {
            // 超时处理
            LOG_WARNING(QString("发送命令超时 - 从地址 %1 到 %2，第%3次尝试").arg(startAddress).arg(endAddress).arg(retryCount+1), "主控设置");
            disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
            reply->deleteLater();
            retryCount++;
        }
    }
    
    // 发送参数给调压状态
    if (success) {
        if (!sendPressureToRegulator(params)) {
            LOG_ERROR("发送压力值到调压装置失败", "主控设置");
            success = false;
        }
    }
    
    if (success) {
        LOG_INFO("所有参数已成功发送到设备", "主控设置");
    } else {
        LOG_ERROR("发送参数完成命令失败", "主控设置");
    }
    return success;
}
// 发送压力值给调压装置
bool MainControlSetting::sendPressureToRegulator(const QMap<QString, QVariant> params){
    // 检查调压装置Modbus客户端是否设置
    if (!pressureRegulatorModbusClient) {
        LOG_ERROR("无法向调压装置发送压力值: 调压装置Modbus客户端未设置", "主控设置");
        return false;
    }
    
    // 检查调压装置连接状态
    if (!m_pressureRegulatorConnected) {
        LOG_ERROR("无法向调压装置发送压力值: 调压装置未连接", "主控设置");
        LOG_DEBUG(QString("当前调压装置状态: %1, m_pressureRegulatorConnected: %2").arg(
            pressureRegulatorModbusClient->state() == QModbusDevice::ConnectedState ? "已连接" : "未连接",
            m_pressureRegulatorConnected ? "已连接" : "未连接"), "主控设置");
        return false;
    }
    
    // 检查params中是否包含pressure_set_fill键
    if (!params.contains("pressure_set_fill")) {
        LOG_ERROR("参数中不包含pressure_set_fill键，无法向调压装置发送压力值", "主控设置");
        return false;
    }
    
    // 获取填充压力值
    QVariant pressureVariant = params["pressure_set_fill"];
    if (!pressureVariant.isValid() || pressureVariant.isNull()) {
        LOG_ERROR("pressure_set_fill参数无效或为空，无法向调压装置发送压力值", "主控设置");
        return false;
    }
    
    // 转换为double类型以支持精确的浮点数计算
    double pressureSetFill = pressureVariant.toDouble();
    LOG_DEBUG(QString("从参数中获取到填充压力值: %1").arg(pressureSetFill), "主控设置");
    
    // 准备压力值，将参数中的填充压力值写入到寄存器80
    quint16 pressureValue = static_cast<quint16>((pressureSetFill + 1) / 0.18);
    LOG_DEBUG(QString("计算后的压力值: %1，准备发送到调压装置").arg(pressureValue), "主控设置");
    
    // 使用功能码16（Write Multiple Registers）写入寄存器，确保写入成功
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 80, 2);
    writeUnit.setValue(0, pressureValue);
    writeUnit.setValue(1, 0); // 第二个寄存器设为0，确保使用功能码16
    
    const int timeoutMs = 2000; // 超时时间2秒
    const int maxRetries = 2; // 最大重试次数
    bool success = false;
    int retryCount = 0;
    
    // 尝试发送压力值，最多重试maxRetries次
    while (retryCount <= maxRetries && !success) {
        // 等待设备准备
        QThread::msleep(500);
        
        LOG_DEBUG(QString("尝试发送压力值到调压装置，第%1次，从站ID: %2, 寄存器地址: 80, 压力值: %3").arg(
            retryCount+1).arg(pressureRegulatorSlaveId).arg(pressureValue), "主控设置");
        
        QModbusReply *reply = pressureRegulatorModbusClient->sendWriteRequest(writeUnit, pressureRegulatorSlaveId);
        if (!reply) {
            LOG_ERROR(QString("sendWriteRequest返回nullptr，无法向调压装置发送压力值，从站ID: %1").arg(pressureRegulatorSlaveId), "主控设置");
            retryCount++;
            continue;
        }
        
        // 等待回复或超时
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        
        connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        timer.start(timeoutMs);
        loop.exec();
        
        // 处理回复
        if (timer.isActive()) {
            // 正常收到回复
            timer.stop();
            
            if (reply->error() == QModbusDevice::NoError) {
                LOG_INFO(QString("成功向调压装置发送压力值: %1").arg(pressureValue), "主控设置");
                success = true;
            } else {
                LOG_ERROR(QString("向调压装置发送压力值失败: %1, 从站ID: %2, 寄存器地址: 80, 压力值: %3").arg(
                    reply->errorString()).arg(pressureRegulatorSlaveId).arg(pressureValue), "主控设置");
                retryCount++;
            }
            
            reply->deleteLater();
        } else {
            // 超时
            LOG_WARNING(QString("向调压装置发送压力值超时，第%1次尝试，从站ID: %2, 寄存器地址: 80, 压力值: %3, 超时时间: %4ms").arg(
                retryCount+1).arg(pressureRegulatorSlaveId).arg(pressureValue).arg(timeoutMs), "主控设置");
            disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
            reply->deleteLater();
            retryCount++;
        }
    }
    
    LOG_DEBUG(QString("发送压力值到调压装置完成，结果: %1").arg(success ? "成功" : "失败"), "主控设置");
    return success;
}


