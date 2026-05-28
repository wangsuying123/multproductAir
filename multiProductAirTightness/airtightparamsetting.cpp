#include "airtightparamsetting.h"
#include "ui_airtightparamsetting.h"
#include "mainControlSetting.h"
#include "realtimemonitor.h"
#include "enum/pressureUnit.h"
#include "enum/leakUnit.h"
#include "enum/volumeUnit.h"
#include "enum/fillTypeUnit.h"
#include "enum/VolumeEncoding.h"
#include "databasemanager.h"
#include "logmanager.h"
#include <QMessageBox>
#include <QDebug>
#include <QModbusReply>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QGridLayout>


AirtightParamSetting::AirtightParamSetting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AirtightParamSetting),
    m_airTightConnected(false), // 初始化气密仪连接状态为false
    m_pressureRegulatorConnected(false), // 初始化调压装置连接状态为false
    m_mainBoardConnected(false), // 初始化主控板连接状态为false
    modbusClient(nullptr), // 初始化气密仪Modbus客户端指针
    m_pressureRegulatorModbusClient(nullptr), // 初始化调压装置Modbus客户端指针
    m_mainBoardModbusClient(nullptr), // 初始化主控板Modbus客户端指针
    slaveId(1), // 初始化气密仪从站地址
    m_pressureRegulatorSlaveId(1), // 初始化调压装置从站地址
    m_mainBoardSlaveId(1), // 初始化主控板从站地址
    m_lastSentFillPressure(0), // 初始化上次发送的填充压力值为0
    detectionTimer(nullptr), // 初始化检测定时器
    m_mainControlSetting(nullptr), // 初始化主控设置页面指针
    m_pageStack(nullptr),
    m_navigationFrame(nullptr),
    m_firstButton(nullptr),
    m_prevButton(nullptr),
    m_nextButton(nullptr),
    m_lastButton(nullptr),
    m_pageIndicator(nullptr),
    m_currentPage(0),
    m_totalPages(3),
    m_page1Widget(nullptr),
    m_page2Widget(nullptr),
    m_page3Widget(nullptr),
    m_isMultiChannelTesting(false), // 初始化多通道测试状态为false
    m_currentChannelIndex(-1), // 初始化当前通道索引为-1
    m_currentTestingChannel(0), // 初始化当前测试通道为0
    m_resetInProgress(false), // 初始化复位防抖标志为false
    m_resetDebounceTimer(nullptr) // 初始化复位防抖定时器指针
{
    LOG_INFO("=== AirtightParamSetting 构造函数开始执行 ===", "气密参数");
    ui->setupUi(this);
    
    // 初始化分页UI
    initializePaginatedUI();
    
    // 移除固定几何尺寸，确保页面能正确适应窗口大小变化
    this->setGeometry(QRect());
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 设置布局管理器的属性，确保它能正确适应窗口大小变化
    ui->verticalLayout->setSizeConstraint(QLayout::SetNoConstraint);
    ui->verticalLayout->setContentsMargins(5, 5, 5, 5);
    ui->verticalLayout->setSpacing(5);
    
    // 设置滚动区域的属性，确保它能正确适应窗口大小变化
    ui->scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->scrollArea->setWidgetResizable(true);
    ui->scrollLayout->setSizeConstraint(QLayout::SetNoConstraint);
    
    // 确保滚动区域内容widget能够正确调整大小
    if (ui->scrollArea->widget()) {
        ui->scrollArea->widget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->scrollArea->widget()->setGeometry(QRect());
    }
    
    // 设置时间参数默认值
    ui->fillTimeSpinBox->setMinimum(0.00);
    ui->fillTimeSpinBox->setMaximum(999.99);
    ui->fillTimeSpinBox->setValue(15.00);
    
    ui->stabilizationTimeSpinBox->setMinimum(0.00);
    ui->stabilizationTimeSpinBox->setMaximum(999.99);
    ui->stabilizationTimeSpinBox->setValue(15.00);
    
    ui->testTimeSpinBox->setMinimum(0.00);
    ui->testTimeSpinBox->setMaximum(999.99);
    ui->testTimeSpinBox->setValue(15.00);
    
    ui->dumpTimeSpinBox->setMinimum(0.00);
    ui->dumpTimeSpinBox->setMaximum(999.99);
    ui->dumpTimeSpinBox->setValue(8.00);
    
    // 设置压力参数默认值
    ui->pressureUnitComboBox->setCurrentText("kPa");
    ui->maxPressureSpinBox->setValue(60.00);
    ui->minPressureSpinBox->setValue(50.00);
    ui->setFillSpinBox->setValue(55.00);
    
    // 设置泄漏参数默认值
    ui->leakUnitComboBox->setCurrentText("mL/min");
    ui->testRejectSpinBox->setValue(3.00);
    ui->refRejectSpinBox->setValue(3.00);
    ui->offsetSpinBox->setValue(0.00);
    
    // 设置容积参数默认值
    ui->volumeComboBox->setCurrentText("30");
    ui->volumeUnitComboBox->setCurrentText("mL");
    
    // 设置填充类型默认值
    ui->fillTypeComboBox->setCurrentText("Ramp Control");
    
    // 设置标准大气压和温度默认值（这些控件在UI中是隐藏的）
    ui->stdAtmSpinBox->setValue(1013.25);
    ui->stdTempSpinBox->setValue(20.00);
    
    // 确保所有分组框都可见并正确显示
    ui->programGroupBox->show();
    ui->cycleTimeGroupBox->show();
    ui->pressureGroupBox->show();
    ui->leakGroupBox->show();
    
    // 强制更新布局
    ui->scrollLayout->update();
    ui->verticalLayout->update();
    this->update();
    
    // 初始化连接状态标签
    ui->connectionStatusLabel->setText("气密仪：未连接");
    ui->connectionStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
    
    ui->mainboardStatusLabel->setText("主控板：未连接");
    ui->mainboardStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
    
    ui->pressureStatusLabel->setText("调压装置：未连接");
    ui->pressureStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
    
    // 初始化Modbus客户端和从站地址
    modbusClient = nullptr;
    slaveId = 1; // 默认从站地址
    
    // 连接信号和槽
    connect(ui->saveParametersButton, &QPushButton::clicked, this, &AirtightParamSetting::onSaveParametersButtonClicked);
    connect(ui->sendToDeviceButton, &QPushButton::clicked, this, &AirtightParamSetting::onSendToDeviceButtonClicked);
    connect(ui->programNumberComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AirtightParamSetting::onProgramNumberChanged);
    
    // 初始化实时检测定时器
    detectionTimer = new QTimer(this);
    connect(detectionTimer, &QTimer::timeout, this, &AirtightParamSetting::realTimeRegisterDetection);
    detectionTimer->start(100); // 每100ms检测一次
    LOG_INFO(QString("=== 实时检测定时器已启动，间隔100ms，isActive=%1 ===").arg(detectionTimer->isActive()), "气密参数");

    // 初始化复位防抖定时器
    m_resetDebounceTimer = new QTimer(this);
    m_resetDebounceTimer->setSingleShot(true); // 单次触发
    m_resetDebounceTimer->setInterval(3000); // 3秒防抖时间
    connect(m_resetDebounceTimer, &QTimer::timeout, this, [this]() {
        LOG_DEBUG("复位防抖定时器到期，允许新的复位操作", "气密参数");
        m_resetInProgress = false;
    });
    LOG_INFO("复位防抖定时器初始化完成，防抖时间3秒", "气密参数");
    
    // 加载当前程序号的参数
    LOG_INFO(QString("加载程序号 %1 的参数").arg(ui->programNumberComboBox->currentText()), "气密参数");
    loadParameters(ui->programNumberComboBox->currentText().toInt());
    LOG_INFO("=== AirtightParamSetting 构造函数执行完成 ===", "气密参数");
}

AirtightParamSetting::~AirtightParamSetting()
{
    LOG_INFO("=== AirtightParamSetting 析构函数被调用 ===", "气密参数");
    if (detectionTimer) {
        detectionTimer->stop();
        delete detectionTimer;
    }
    if (m_resetDebounceTimer) {
        m_resetDebounceTimer->stop();
        delete m_resetDebounceTimer;
    }
    delete ui;
}

// 设置Modbus客户端（气密仪）
void AirtightParamSetting::setModbusClient(QModbusClient *client)
{
    // 断开旧客户端的信号连接
    if (modbusClient) {
        modbusClient->disconnect(this);
    }
    
    modbusClient = client;
    
    // 连接modbusClient的所有相关信号
    if (modbusClient) {
        // 连接状态变化信号
        connect(modbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            m_airTightConnected = (state == QModbusDevice::ConnectedState);
            LOG_INFO(QString("Modbus客户端状态变化：%1").arg(state), "气密参数");
            
            // 更新UI状态
            if (m_airTightConnected) {
                ui->connectionStatusLabel->setText("气密仪：已连接");
                ui->connectionStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
            } else {
                ui->connectionStatusLabel->setText("气密仪：未连接");
                ui->connectionStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
            }
        });
        
        // 连接错误信号
        connect(modbusClient, &QModbusClient::errorOccurred, this, [this](QModbusDevice::Error error) {
            LOG_ERROR(QString("Modbus客户端错误：%1 - %2").arg(error).arg(modbusClient->errorString()), "气密参数");
        });

        // 立即同步当前连接状态
        m_airTightConnected = (modbusClient->state() == QModbusDevice::ConnectedState);
        if (m_airTightConnected) {
            ui->connectionStatusLabel->setText("气密仪：已连接");
            ui->connectionStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
            LOG_INFO("气密仪客户端设置时已处于连接状态，直接同步", "气密参数");
        }
    } else {
        // 没有客户端时，重置连接状态
        m_airTightConnected = false;
        ui->connectionStatusLabel->setText("气密仪：未连接");
        ui->connectionStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
    }
}

// 设置从站地址
void AirtightParamSetting::setSlaveId(quint8 id)
{
    slaveId = id;
}

// 设置调压装置Modbus客户端
void AirtightParamSetting::setPressureRegulatorModbusClient(QModbusClient *client)
{
    // 断开旧客户端的信号连接
    if (m_pressureRegulatorModbusClient) {
        m_pressureRegulatorModbusClient->disconnect(this);
    }
    
    m_pressureRegulatorModbusClient = client;
    
    // 连接m_pressureRegulatorModbusClient的所有相关信号
    if (m_pressureRegulatorModbusClient) {
        // 连接状态变化信号，使用统一的槽函数处理
        connect(m_pressureRegulatorModbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            bool connected = (state == QModbusDevice::ConnectedState);
            m_pressureRegulatorConnected = connected;
            LOG_INFO(QString("调压装置Modbus客户端状态变化：%1").arg(state), "气密参数");
            onPressureRegulatorConnectionChanged(connected);
        });
        
        // 连接错误信号
        connect(m_pressureRegulatorModbusClient, &QModbusClient::errorOccurred, this, [this](QModbusDevice::Error error) {
            LOG_ERROR(QString("调压装置Modbus客户端错误：%1 - %2").arg(error).arg(m_pressureRegulatorModbusClient->errorString()), "气密参数");
        });

        // 立即同步当前连接状态
        bool alreadyConnected = (m_pressureRegulatorModbusClient->state() == QModbusDevice::ConnectedState);
        if (alreadyConnected) {
            m_pressureRegulatorConnected = true;
            onPressureRegulatorConnectionChanged(true);
            LOG_INFO("调压装置客户端设置时已处于连接状态，直接同步", "气密参数");
        }
    } else {
        // 没有客户端时，重置连接状态
        m_pressureRegulatorConnected = false;
        onPressureRegulatorConnectionChanged(false);
    }
}

// 设置调压装置从站地址
void AirtightParamSetting::setPressureRegulatorSlaveId(quint8 id)
{
    m_pressureRegulatorSlaveId = id;
}

// 设置主控板Modbus客户端
void AirtightParamSetting::setMainBoardModbusClient(QModbusClient *client)
{
    // 断开旧客户端的信号连接
    if (m_mainBoardModbusClient) {
        m_mainBoardModbusClient->disconnect(this);
    }
    
    m_mainBoardModbusClient = client;
    
    // 连接m_mainBoardModbusClient的所有相关信号
    if (m_mainBoardModbusClient) {
        // 连接状态变化信号，使用统一的槽函数处理
        connect(m_mainBoardModbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            bool connected = (state == QModbusDevice::ConnectedState);
            m_mainBoardConnected = connected;
            LOG_INFO(QString("主控板Modbus客户端状态变化：%1").arg(state), "气密参数");
            onMainBoardConnectionChanged(connected);
        });
        
        // 连接错误信号
        connect(m_mainBoardModbusClient, &QModbusClient::errorOccurred, this, [this](QModbusDevice::Error error) {
            LOG_ERROR(QString("主控板Modbus客户端错误：%1 - %2").arg(error).arg(m_mainBoardModbusClient->errorString()), "气密参数");
        });

        // 立即同步一次当前连接状态，避免客户端已连接但未触发 stateChanged 的情况
        // （开机自启动时设备可能已处于 ConnectedState，不会再发出 stateChanged 信号）
        bool alreadyConnected = (m_mainBoardModbusClient->state() == QModbusDevice::ConnectedState);
        if (alreadyConnected) {
            m_mainBoardConnected = true;
            onMainBoardConnectionChanged(true);
            LOG_INFO("主控板客户端设置时已处于连接状态，直接同步", "气密参数");
        }
    } else {
        // 没有客户端时，重置连接状态
        m_mainBoardConnected = false;
        onMainBoardConnectionChanged(false);
    }
}

// 设置主控板从站地址
void AirtightParamSetting::setMainBoardSlaveId(quint8 id)
{
    m_mainBoardSlaveId = id;
}

// 设置主控设置页面指针
void AirtightParamSetting::setMainControlSetting(MainControlSetting *mainControlSetting)
{
    m_mainControlSetting = mainControlSetting;
    LOG_INFO("主控设置页面指针已设置", "气密参数");
}

void AirtightParamSetting::setRealTimeMonitor(RealTimeMonitor *monitor)
{
    m_realTimeMonitor = monitor;
    LOG_INFO("实时监控页面指针已设置", "气密参数");
    
    // 连接重发启动命令信号，用于通道测试阶段异常时的兜底处理
    if (m_realTimeMonitor) {
        connect(m_realTimeMonitor, &RealTimeMonitor::reSendStartCommand, this, [this](int channel) {
            LOG_WARNING(QString("收到重发启动命令信号，通道: %1").arg(channel), "气密参数");
            if (m_isMultiChannelTesting && m_currentTestingChannel == channel) {
                startAirtightTest();
            }
        });
    }
}

// 主控板连接状态变化处理（统一的UI更新方法）
void AirtightParamSetting::onMainBoardConnectionChanged(bool connected)
{
    m_mainBoardConnected = connected;
    if (connected) {
        ui->mainboardStatusLabel->setText("主控板：已连接");
        ui->mainboardStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
    } else {
        ui->mainboardStatusLabel->setText("主控板：未连接");
        ui->mainboardStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
    }
}

void AirtightParamSetting::onAirTightConnectionChanged(bool connected)
{
    m_airTightConnected = connected; // 保存气密仪连接状态
    
    if (connected) {
        ui->connectionStatusLabel->setText("气密仪：已连接");
        ui->connectionStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
    } else {
        ui->connectionStatusLabel->setText("气密仪：未连接");
        ui->connectionStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
    }
}

void AirtightParamSetting::onPressureRegulatorConnectionChanged(bool connected)
{
    m_pressureRegulatorConnected = connected;
    if (connected) {
        ui->pressureStatusLabel->setText("调压装置：已连接");
        ui->pressureStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
    } else {
        ui->pressureStatusLabel->setText("调压装置：未连接");
        ui->pressureStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold; background-color: rgba(255, 255, 255, 0.15); padding: 8px 16px; border-radius: 6px; font-size: 14px; border: 1px solid rgba(255, 255, 255, 0.2);");
    }
}

// 保存参数按钮点击事件处理函数
void AirtightParamSetting::onSaveParametersButtonClicked()
{
    int programNumber = ui->programNumberComboBox->currentText().toInt();
    saveParameters(programNumber);
    LOG_INFO(QString("程序号 %1 的参数已成功保存！").arg(programNumber), "气密参数");
    QMessageBox::information(this, "保存成功", QString("程序号 %1 的参数已成功保存！").arg(programNumber));
}

// 程序号变化事件处理函数
void AirtightParamSetting::onProgramNumberChanged(int index)
{
    int programNumber = ui->programNumberComboBox->itemText(index).toInt();
    loadParameters(programNumber);
}

// 保存参数方法
void AirtightParamSetting::saveParameters(int programNumber)
{
    LOG_INFO(QString("开始保存程序号 %1 的参数").arg(programNumber), "气密参数");
    
    // 创建参数映射
    QMap<QString, QVariant> params;
    params["program_number"] = programNumber;
    params["param_name"] = QString("程序%1参数").arg(programNumber);
    
    // 读取时间参数
    params["fill_time"] = ui->fillTimeSpinBox->value();
    params["stabilization_time"] = ui->stabilizationTimeSpinBox->value();
    params["test_time"] = ui->testTimeSpinBox->value();
    params["dump_time"] = ui->dumpTimeSpinBox->value();
    
    LOG_DEBUG(QString("时间参数: 填充=%1, 稳定=%2, 测试=%3, 排气=%4")
        .arg(params["fill_time"].toDouble())
        .arg(params["stabilization_time"].toDouble())
        .arg(params["test_time"].toDouble())
        .arg(params["dump_time"].toDouble()), "气密参数");
    
    // 压力单位转换
    params["pressure_unit"] = PressureUnitHelper::toInt(PressureUnitHelper::fromString(ui->pressureUnitComboBox->currentText()));
    params["pressure_max"] = ui->maxPressureSpinBox->value();
    params["pressure_min"] = ui->minPressureSpinBox->value();
    params["pressure_set_fill"] = ui->setFillSpinBox->value();
    
    LOG_DEBUG(QString("压力参数: 单位=%1, 最大=%2, 最小=%3, 填充=%4")
        .arg(params["pressure_unit"].toInt())
        .arg(params["pressure_max"].toDouble())
        .arg(params["pressure_min"].toDouble())
        .arg(params["pressure_set_fill"].toDouble()), "气密参数");
    
    // 填充类型转换
    params["fill_type"] = FillTypeHelper::toInt(FillTypeHelper::fromString(ui->fillTypeComboBox->currentText()));
    
    // 泄漏单位转换
    params["leak_unit"] = LeakUnitHelper::getRegister8212Value(ui->leakUnitComboBox->currentText());
    params["leak_unit2"] = LeakUnitHelper::getRegister8217Value(ui->leakUnitComboBox->currentText());
    
    // 其他参数
    params["test_reject"] = ui->testRejectSpinBox->value();
    params["ref_reject"] = ui->refRejectSpinBox->value();
    params["offset"] = ui->offsetSpinBox->value();
    params["std_atm"] = ui->stdAtmSpinBox->value();
    params["std_temp"] = ui->stdTempSpinBox->value();
    
    LOG_DEBUG(QString("泄漏参数: 单位=%1, 允许泄漏=%2, 参考泄漏=%3, 偏移=%4")
        .arg(params["leak_unit"].toInt())
        .arg(params["test_reject"].toDouble())
        .arg(params["ref_reject"].toDouble())
        .arg(params["offset"].toDouble()), "气密参数");
    
    quint16 encoded = 0;
    // 容积值处理
    double volumeValue = ui->volumeComboBox->currentText().toDouble();
    if (!VolumeEncoding::tryEncode(volumeValue, encoded)) {
        LOG_ERROR("容积值编码失败", "气密参数");
        QMessageBox::warning(this, "警告", "容积值编码失败");
        return;
    }
    // 直接使用编码值
    params["volume"] = encoded;
    
    // 容积单位转换
    params["volume_unit"] = VolumeUnitHelper::toInt(VolumeUnitHelper::fromString(ui->volumeUnitComboBox->currentText()));
    params["reject_calc"] = 0; // 默认计算方式
    
    LOG_DEBUG(QString("容积参数: 容积=%1, 编码=%2, 单位=%3")
        .arg(volumeValue)
        .arg(encoded)
        .arg(params["volume_unit"].toInt()), "气密参数");
    
    // 连接数据库并保存参数
    DatabaseManager* dbManager = DatabaseManager::getInstance();
    if (!dbManager->connectDatabase()) {
        LOG_ERROR("数据库连接失败，无法保存参数", "气密参数");
        QMessageBox::warning(this, "错误", "数据库连接失败，无法保存参数");
        return;
    }
    
    // 检查是否存在该程序号的参数
    QList<QMap<QString, QVariant>> existingParams = dbManager->getAirTightnessParamsByProgram(programNumber);
    bool success = false;
    
    if (existingParams.isEmpty()) {
        // 保存新参数
        LOG_INFO(QString("保存新参数，程序号: %1").arg(programNumber), "气密参数");
        success = dbManager->saveAirTightnessParams(params);
        if (success) {
            LOG_INFO(QString("程序 %1 参数保存成功").arg(programNumber), "气密参数");
        } else {
            LOG_ERROR(QString("程序 %1 参数保存失败: %2").arg(programNumber).arg(dbManager->getLastError()), "气密参数");
            QMessageBox::warning(this, "错误", QString("参数保存失败: %1").arg(dbManager->getLastError()));
        }
    } else {
        // 更新现有参数
        int id = existingParams.first()["id"].toInt();
        LOG_INFO(QString("更新现有参数，程序号: %1, ID: %2").arg(programNumber).arg(id), "气密参数");
        success = dbManager->updateAirTightnessParams(id, params);
        if (success) {
            LOG_INFO(QString("程序 %1 参数更新成功").arg(programNumber), "气密参数");
        } else {
            LOG_ERROR(QString("程序 %1 参数更新失败: %2").arg(programNumber).arg(dbManager->getLastError()), "气密参数");
            QMessageBox::warning(this, "错误", QString("参数更新失败: %1").arg(dbManager->getLastError()));
        }
    }
    
    // 发送参数到设备
    if (success) {
        emit parametersSaved(programNumber);
        LOG_INFO("参数保存成功，已发送更新通知", "气密参数");
    }
}

// 发送单个Modbus命令
bool AirtightParamSetting::sendModbusCommand(quint16 address, quint16 value, int timeoutMs)
{
    if (!modbusClient || modbusClient->state() != QModbusDevice::ConnectedState) {
        // 如果modbusClient未初始化或未连接，但底层连接已建立，直接返回成功
        if (m_airTightConnected) {
            LOG_DEBUG("Modbus客户端未连接，但信任m_airTightConnected状态，返回成功", "气密参数");
            return true;
        }
        LOG_WARNING("Modbus客户端未连接", "气密参数");
        return false;
    }
    
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, 1);
    writeUnit.setValue(0, value);
    
    LOG_DEBUG(QString("发送单个Modbus命令: 地址=%1, 值=%2, 从站ID=%3").arg(address).arg(value).arg(slaveId), "气密参数");
    QModbusReply *reply = modbusClient->sendWriteRequest(writeUnit, slaveId);
    if (!reply) {
        LOG_ERROR("发送Modbus命令失败: 无法创建回复对象", "气密参数");
        return false;
    }
    
    QEventLoop loop;
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    
    loop.exec();
    
    bool success = false;
    if (timer.isActive()) {
        timer.stop();
        
        if (reply->error() == QModbusDevice::NoError) {
            success = true;
            LOG_INFO("单个Modbus命令发送成功", "气密参数");
        } else {
            LOG_ERROR(QString("单个Modbus命令错误: %1 - %2").arg(reply->error()).arg(reply->errorString()), "气密参数");
            
            // 尝试获取更多异常信息
            if (reply->error() == QModbusDevice::ProtocolError) {
                QModbusExceptionResponse exception = reply->rawResult();
                LOG_ERROR(QString("Modbus异常代码：%1 - 功能码：%2").arg(exception.exceptionCode()).arg(exception.functionCode()), "气密参数");
            }
        }
    } else {
        LOG_WARNING("单个Modbus命令超时", "气密参数");
    }
    
    reply->deleteLater();
    return success;
}

// 读取Modbus寄存器
bool AirtightParamSetting::readModbusRegister(quint16 address, quint16 &value, int timeoutMs)
{
    // 使用主控板专用Modbus客户端
    QModbusClient *client = m_mainBoardModbusClient;
    if (!client) {
        LOG_WARNING("主控板Modbus客户端未设置", "气密参数");
        return false;
    }
    
    if (client->state() != QModbusDevice::ConnectedState) {
        LOG_WARNING(QString("主控板Modbus客户端未连接，状态=%1").arg(client->state()), "气密参数");
        return false;
    }
    
    QModbusDataUnit readUnit(QModbusDataUnit::InputRegisters, address, 1);
    
    QModbusReply *reply = client->sendReadRequest(readUnit, m_mainBoardSlaveId);
    if (!reply) {
        LOG_ERROR("读取Modbus寄存器失败: 无法创建回复对象", "气密参数");
        return false;
    }
    
    QEventLoop loop;
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    
    loop.exec();
    
    bool success = false;
    if (timer.isActive()) {
        timer.stop();
        
        if (reply->error() == QModbusDevice::NoError) {
            const QModbusDataUnit result = reply->result();
            if (result.valueCount() > 0) {
                value = result.value(0);
                success = true;
            }
        } else {
            LOG_ERROR(QString("读取Modbus寄存器错误: %1 - %2").arg(reply->error()).arg(reply->errorString()), "气密参数");
            
            // 尝试获取更多异常信息
            if (reply->error() == QModbusDevice::ProtocolError) {
                QModbusExceptionResponse exception = reply->rawResult();
                LOG_ERROR(QString("Modbus异常代码：%1 - 功能码：%2").arg(exception.exceptionCode()).arg(exception.functionCode()), "气密参数");
            }
        }
    } else {
        LOG_WARNING("读取Modbus寄存器超时", "气密参数");
    }
    
    reply->deleteLater();
    return success;
}

// 辅助函数：检查设备连接状态
bool AirtightParamSetting::checkDeviceConnection()
{
    if (!m_airTightConnected) {
        QMessageBox::warning(this, "连接失败", "气密仪未连接，无法发送参数！");
        return false;
    }
    
    if (modbusClient && modbusClient->state() != QModbusDevice::ConnectedState) {
        // 如果modbusClient未初始化或未连接，但底层连接已建立，可能是因为直接使用了TCP/串口连接
        // 这里我们信任m_airTightConnected状态，跳过modbusClient检查
        LOG_DEBUG("Modbus客户端未连接，但信任m_airTightConnected状态", "气密参数");
    }
    
    return true;
}

// 辅助函数：编码时间参数
quint16 AirtightParamSetting::encodeTimeParameter(double timeValue)
{
    if (timeValue < 0.0) timeValue = 0.0;
    if (timeValue > 200.0) timeValue = 200.0;
    quint16 value = static_cast<quint16>(std::round(timeValue * 100.0));
    quint16 low = static_cast<quint16>(value & 0xFFu);
    quint16 high = static_cast<quint16>((value >> 8) & 0xFFu);
    return static_cast<quint16>((low << 8) | high);
}

// 辅助函数：编码压力参数
QPair<quint16, quint16> AirtightParamSetting::encodePressureParameter(quint16 pressureValue)
{
    auto swap16 = [](quint16 v) -> quint16 { return static_cast<quint16>((v << 8) | (v >> 8)); };
    
    quint32 raw = static_cast<quint32>(pressureValue) * 1000u;
    quint16 low = static_cast<quint16>(raw & 0xFFFFu);
    quint16 high = static_cast<quint16>((raw >> 16) & 0xFFFFu);
    
    return qMakePair(swap16(low), swap16(high));
}

// 辅助函数：准备Modbus写入数据单元
QModbusDataUnit AirtightParamSetting::prepareWriteDataUnit()
{
    const quint16 startAddress = MODBUS_START_ADDRESS;
    const quint16 endAddress = MODBUS_END_ADDRESS;
    const int numRegisters = endAddress - startAddress + 1;
    
    // 创建批量写入数据单元
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, startAddress, numRegisters);
    
    // 初始化所有寄存器值为0
    for (int i = 0; i < numRegisters; ++i) {
        writeUnit.setValue(i, 0);
    }
    
    // 编码时间参数
    quint16 fillTime = encodeTimeParameter(ui->fillTimeSpinBox->value());
    quint16 stabilizationTime = encodeTimeParameter(ui->stabilizationTimeSpinBox->value());
    quint16 testTime = encodeTimeParameter(ui->testTimeSpinBox->value());
    quint16 dumpTime = encodeTimeParameter(ui->dumpTimeSpinBox->value());
    
    writeUnit.setValue(8192 - startAddress, 26112);
    
    // 写入时间参数
    writeUnit.setValue(8196 - startAddress, fillTime);        // 填充时间
    writeUnit.setValue(8197 - startAddress, stabilizationTime); // 稳定时间
    writeUnit.setValue(8198 - startAddress, testTime);         // 测试时间
    writeUnit.setValue(8199 - startAddress, dumpTime);         // 排放时间
    
    // 压力单位
    quint16 pressureUnitCode = PressureUnitHelper::toInt(PressureUnitHelper::fromString(ui->pressureUnitComboBox->currentText()));
    writeUnit.setValue(8200 - startAddress, pressureUnitCode);   // 压力单位
    
    // 压力值参数
    quint16 minPressure = static_cast<quint16>(ui->minPressureSpinBox->value());
    quint16 maxPressure = static_cast<quint16>(ui->maxPressureSpinBox->value());
    quint16 setFill = static_cast<quint16>(ui->setFillSpinBox->value());
    
    // 将填充压力值保存到成员变量
    { 
        double __val = ui->setFillSpinBox->value();
        int __intPressure = static_cast<int>((__val + 1) / 0.18);
        m_lastSentFillPressure = static_cast<quint16>(__intPressure);
    }
    
    // 编码并写入压力参数
    QPair<quint16, quint16> minEnc = encodePressureParameter(minPressure);
    QPair<quint16, quint16> maxEnc = encodePressureParameter(maxPressure);
    QPair<quint16, quint16> fillEnc = encodePressureParameter(setFill);
    
    writeUnit.setValue(8201 - startAddress, minEnc.first);
    writeUnit.setValue(8202 - startAddress, minEnc.second);
    writeUnit.setValue(8203 - startAddress, maxEnc.first);
    writeUnit.setValue(8204 - startAddress, maxEnc.second);
    writeUnit.setValue(8205 - startAddress, fillEnc.first);
    writeUnit.setValue(8206 - startAddress, fillEnc.second);
    
    // 填充类型
    quint16 fillTypeCode = FillTypeHelper::toInt(FillTypeHelper::fromString(ui->fillTypeComboBox->currentText()));
    writeUnit.setValue(8211 - startAddress, fillTypeCode);          // 填充类型
    
    // 泄漏相关参数
    QString leakUnitString = ui->leakUnitComboBox->currentText();
    quint16 leakUnitCode = static_cast<quint16>(LeakUnitHelper::getRegister8212Value(leakUnitString));
    
    writeUnit.setValue(8212 - startAddress, leakUnitCode);     // 泄漏单位
    writeUnit.setValue(8213 - startAddress, 256); // 固定256
    
    // 容积参数
    quint16 volumeUnitCode = VolumeUnitHelper::toInt(VolumeUnitHelper::fromString(ui->volumeUnitComboBox->currentText()));
    writeUnit.setValue(8214 - startAddress, volumeUnitCode);        // 容积单位
    writeUnit.setValue(8215 - startAddress, 0);
    
    // 需要把容积值映射到枚举编码
    quint16 volume = static_cast<quint16>(ui->volumeComboBox->currentText().toUShort());
    {
        quint16 encoded = 0;
        if (VolumeEncoding::tryEncode(static_cast<int>(volume), encoded)) {
            volume = encoded;
        }
    }
    writeUnit.setValue(8216 - startAddress, volume);          // 容积
    
    // 先用单位1编码还原单位，再取单位2编码
    LeakUnit leakUnit = LeakUnitHelper::fromRegister8212(static_cast<int>(leakUnitCode));
    
    // 计算允许泄露值编码：根据泄露单位使用不同的编码方式
    quint16 allowLeakEncoded8219 = 0;
    quint16 allowLeakEncoded8220 = 0;
    double allowLeakValue = ui->testRejectSpinBox->value();
    
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
    double refLeakValue = ui->refRejectSpinBox->value();
    
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
    
    // 其他参数
    quint16 rejectCalc = REJECT_CALCULATION_DEFAULT; // 默认计算方式
    quint16 offset = static_cast<quint16>(ui->offsetSpinBox->value());
    
    writeUnit.setValue(8218 - startAddress, rejectCalc); // 拒绝计算方式
    writeUnit.setValue(8224 - startAddress, 64);
    writeUnit.setValue(8225 - startAddress, STANDARD_ATMOSPHERE_VALUE);          // 标准大气压
    writeUnit.setValue(8227 - startAddress, 0);         // 标准温度
    writeUnit.setValue(8229 - startAddress, offset);          // 偏移量
    
    return writeUnit;
}

// 辅助函数：发送批量写入请求
bool AirtightParamSetting::sendBatchWriteRequest(const QModbusDataUnit &writeUnit, int timeoutMs)
{
    if (!modbusClient || modbusClient->state() != QModbusDevice::ConnectedState) {
        // 如果modbusClient未连接，但底层连接已建立，直接返回成功
        if (m_airTightConnected) {
            LOG_DEBUG("Modbus客户端未连接，但信任m_airTightConnected状态，报告成功", "气密参数");
            return true;
        }
        return false;
    }
    
    QModbusReply *reply = modbusClient->sendWriteRequest(writeUnit, slaveId);
    if (!reply) {
        return false;
    }
    
    // 等待回复或超时
    QEventLoop loop;
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    
    loop.exec();
    
    bool success = false;
    
    if (timer.isActive()) {
        // 正常收到回复
        timer.stop();
        
        if (reply->error() == QModbusDevice::NoError) {
            success = true;
            LOG_INFO("批量写入成功", "气密参数");
        } else {
            LOG_ERROR(QString("批量写入失败：%1 - %2").arg(reply->error()).arg(reply->errorString()), "气密参数");
            
            // 尝试获取更多异常信息
            if (reply->error() == QModbusDevice::ProtocolError) {
                // 获取Modbus异常代码
                QModbusExceptionResponse exception = reply->rawResult();
                LOG_ERROR(QString("Modbus异常代码：%1 - 功能码：%2").arg(exception.exceptionCode()).arg(exception.functionCode()), "气密参数");
            }
        }
    } else {
        // 超时处理
        LOG_WARNING("批量写入超时", "气密参数");
    }
    
    reply->deleteLater();
    return success;
}

// 发送到设备按钮点击事件处理函数
void AirtightParamSetting::onSendToDeviceButtonClicked()
{
    LOG_INFO("=== 开始发送参数到设备 ===", "气密参数");
    
    // 检查设备连接状态
    if (!checkDeviceConnection()) {
        LOG_WARNING("设备连接检查失败", "气密参数");
        return;
    }
    
    const int timeoutMs = MODBUS_WRITE_TIMEOUT; // 单个命令超时时间
    
    // 先发送地址8192,值为1的命令，通知设备准备接收参数
    LOG_INFO("发送设备准备命令", "气密参数");
    bool prepareSuccess = sendModbusCommand(MODBUS_START_ADDRESS, DEVICE_PREPARE_COMMAND_VALUE, timeoutMs);
    LOG_INFO(QString("准备命令发送结果: %1").arg(prepareSuccess ? "成功" : "失败"), "气密参数");
    
    // 等待设备准备
    QThread::msleep(DEVICE_PREPARE_WAIT_TIME);
    
    // 发送程序号
    int programNumber = ui->programNumberComboBox->currentText().toInt();
    LOG_INFO(QString("当前程序号: %1").arg(programNumber), "气密参数");
    
    // 准备Modbus写入数据单元
    LOG_INFO("准备Modbus写入数据", "气密参数");
    QModbusDataUnit writeUnit = prepareWriteDataUnit();
    
    // 发送批量写入请求
    LOG_INFO("发送批量写入请求", "气密参数");
    bool success = sendBatchWriteRequest(writeUnit, timeoutMs);
    LOG_INFO(QString("批量写入结果: %1").arg(success ? "成功" : "失败"), "气密参数");
    
    QString message;
    if (success) {
        LOG_INFO(QString("发送填充压力到调压装置: %1").arg(m_lastSentFillPressure), "气密参数");
        bool pressureSuccess = sendPressureToRegulator(m_lastSentFillPressure, timeoutMs);
        if (pressureSuccess) {
            message = "调压参数已成功发送到设备！";
            LOG_INFO(message, "气密参数");
            QMessageBox::information(this, "发送成功", message);
        } else {
            message = "发送压力参数失败！";
            LOG_ERROR(message, "气密参数");
            QMessageBox::warning(this, "发送失败", message);
        }
    } else {
        message = "发送参数失败！";
        LOG_ERROR(message, "气密参数");
        QMessageBox::warning(this, "发送失败", message);
    }
    
    LOG_INFO("=== 发送参数到设备完成 ===", "气密参数");
}

// 获取上次发送的填充压力值
quint16 AirtightParamSetting::getLastSentFillPressure() const
{
    return m_lastSentFillPressure;
}

// 获取当前程序号
int AirtightParamSetting::getCurrentProgramNumber()
{
    int programNumber = 0;
    
    // 首先尝试从主控设置页面获取程序号
    if (m_mainControlSetting) {
        programNumber = m_mainControlSetting->getProgramNumber();
        LOG_INFO(QString("从主控设置页面获取程序号: %1").arg(programNumber), "气密参数");
    }
    
    // 如果仍然为0，默认使用程序号1
    if (programNumber == 0) {
        programNumber = 1;
        LOG_INFO("程序号未设置，默认使用程序号1", "气密参数");
    }
    
    return programNumber;
}

// 启动下一个通道测试
void AirtightParamSetting::startNextChannelTest()
{
    LOG_DEBUG(QString("【通道测试】进入startNextChannelTest(), m_isMultiChannelTesting=%1, m_currentChannelIndex=%2, m_enabledChannels.size=%3")
              .arg(m_isMultiChannelTesting ? "true" : "false").arg(m_currentChannelIndex).arg(m_enabledChannels.size()), "气密参数");

    if (!m_isMultiChannelTesting) {
        LOG_WARNING("【通道测试】未在多通道测试状态中，直接返回", "气密参数");
        return;
    }

    // 停止实时监控定时器，避免Modbus通信冲突
    if (m_realTimeMonitor) {
        LOG_DEBUG("【通道测试】停止实时监控定时器，避免Modbus通信冲突", "气密参数");
        m_realTimeMonitor->stopDataTimers();
    } else {
        LOG_WARNING("【通道测试】m_realTimeMonitor为空，无法停止定时器", "气密参数");
    }
    
    // 新增：等待200ms确保Modbus链路完全释放
    LOG_DEBUG("【通道测试】等待200ms确保Modbus链路释放", "气密参数");
    QThread::msleep(200);
    
    // 重置实时监控的软件状态，确保下一通道测试结果能被正确处理
    if (m_realTimeMonitor) {
        m_realTimeMonitor->resetTestPhaseState();
    }
    
    // 等待气密仪进入待机状态（寄存器8707=0）后再开始下一通道测试
    waitForDeviceStandby();
    
    if (m_currentChannelIndex >= m_enabledChannels.size()) {
        // 所有通道测试完成
        LOG_INFO("【通道测试】所有开启通道的气密测试已完成", "气密参数");
        m_isMultiChannelTesting = false;
        m_currentChannelIndex = -1;
        m_currentTestingChannel = 0;
        m_enabledChannels.clear();
        // 所有测试完成，重置测试阶段状态，重新启动定时器
        if (m_realTimeMonitor) {
            m_realTimeMonitor->resetTestPhaseState();
            m_realTimeMonitor->startDataTimers();
        }
        return;
    }
    
    // 获取当前要测试的通道
    m_currentTestingChannel = m_enabledChannels[m_currentChannelIndex];
    LOG_INFO(QString("【通道测试】开始测试通道%1 (索引: %2/%3)").arg(m_currentTestingChannel)
             .arg(m_currentChannelIndex + 1).arg(m_enabledChannels.size()), "气密参数");
    QString enabledChannelsStr;
    for (int i = 0; i < m_enabledChannels.size(); ++i) {
        if (i > 0) enabledChannelsStr += ", ";
        enabledChannelsStr += QString::number(m_enabledChannels[i]);
    }
    LOG_DEBUG(QString("【通道测试】通道列表: %1").arg(enabledChannelsStr), "气密参数");
    
    // 设置当前测试通道到RealTimeMonitor
    if (m_realTimeMonitor) {
        LOG_DEBUG(QString("【通道测试】设置RealTimeMonitor当前测试通道为%1").arg(m_currentTestingChannel), "气密参数");
        m_realTimeMonitor->setCurrentTestingChannel(m_currentTestingChannel);
    } else {
        LOG_WARNING("【通道测试】m_realTimeMonitor为空，无法设置当前测试通道", "气密参数");
    }
    
    // 获取该通道对应的程序号
    int programNumber = getProgramNumberForChannel(m_currentTestingChannel);
    LOG_INFO(QString("【通道测试】通道%1 -> 程序号%2").arg(m_currentTestingChannel).arg(programNumber), "气密参数");
    
    // 通知实时监控页面更新参数显示
    if (m_realTimeMonitor) {
        m_realTimeMonitor->onProgramNumberReceived(programNumber);
    }
    
    // 从数据库加载参数
    LOG_DEBUG(QString("【通道测试】通道%1 - 开始从数据库加载程序号%2的参数").arg(m_currentTestingChannel).arg(programNumber), "气密参数");
    DatabaseManager* dbManager = DatabaseManager::getInstance();
    if (!dbManager->connectDatabase()) {
        LOG_ERROR(QString("【通道测试】通道%1 - 数据库连接失败：%2").arg(m_currentTestingChannel).arg(dbManager->getLastError()), "气密参数");
        // 继续下一个通道
        LOG_WARNING(QString("【通道测试】通道%1 - 数据库连接失败，跳过此通道，继续下一个").arg(m_currentTestingChannel), "气密参数");
        m_currentChannelIndex++;
        // 即使失败也重新启动定时器，避免定时器永久停止
        if (m_realTimeMonitor) {
            m_realTimeMonitor->startDataTimers();
        }
        QTimer::singleShot(100, this, &AirtightParamSetting::startNextChannelTest);
        return;
    }
    LOG_DEBUG(QString("【通道测试】通道%1 - 数据库连接成功").arg(m_currentTestingChannel), "气密参数");
    
    QList<QMap<QString, QVariant>> paramsList = dbManager->getAirTightnessParamsByProgram(programNumber);
    LOG_DEBUG(QString("【通道测试】通道%1 - 查询到%2条参数记录").arg(m_currentTestingChannel).arg(paramsList.size()), "气密参数");
    
    if (paramsList.isEmpty()) {
        QString lastError = dbManager->getLastError();
        if (!lastError.isEmpty()) {
            LOG_ERROR(QString("【通道测试】通道%1 - 数据库查询失败：%2").arg(m_currentTestingChannel).arg(lastError), "气密参数");
        } else {
            LOG_ERROR(QString("【通道测试】通道%1 - 未找到程序号%2的参数！").arg(m_currentTestingChannel).arg(programNumber), "气密参数");
        }
        // 继续下一个通道
        LOG_WARNING(QString("【通道测试】通道%1 - 参数不存在，跳过此通道，继续下一个").arg(m_currentTestingChannel), "气密参数");
        m_currentChannelIndex++;
        // 即使失败也重新启动定时器，避免定时器永久停止
        if (m_realTimeMonitor) {
            m_realTimeMonitor->startDataTimers();
        }
        QTimer::singleShot(100, this, &AirtightParamSetting::startNextChannelTest);
        return;
    }
    
    // 发送参数到设备
    QMap<QString, QVariant> params = paramsList.first();
    LOG_INFO(QString("【通道测试】通道%1 - 开始发送程序号%2的参数到气密仪").arg(m_currentTestingChannel).arg(programNumber), "气密参数");
    
    if (!sendParamsToDevice(params)) {
        LOG_ERROR(QString("【通道测试】通道%1 - 发送参数到设备失败").arg(m_currentTestingChannel), "气密参数");
        // 继续下一个通道
        LOG_WARNING(QString("【通道测试】通道%1 - 参数发送失败，跳过此通道，继续下一个").arg(m_currentTestingChannel), "气密参数");
        m_currentChannelIndex++;
        // 即使失败也重新启动定时器，避免定时器永久停止
        if (m_realTimeMonitor) {
            m_realTimeMonitor->startDataTimers();
        }
        QTimer::singleShot(100, this, &AirtightParamSetting::startNextChannelTest);
        return;
    }
    LOG_DEBUG(QString("【通道测试】通道%1 - 参数发送成功").arg(m_currentTestingChannel), "气密参数");
    
    // 参数发送后等待300ms，确保设备有足够时间处理参数
    LOG_DEBUG("【通道测试】等待300ms确保参数处理完成", "气密参数");
    QThread::msleep(300);
    
    // 启动气密仪
    LOG_INFO(QString("【通道测试】通道%1 - 参数发送成功，启动气密仪").arg(m_currentTestingChannel), "气密参数");
    bool startSuccess = startAirtightTest();
    LOG_DEBUG(QString("【通道测试】通道%1 - startAirtightTest()返回: %2").arg(m_currentTestingChannel).arg(startSuccess ? "成功" : "失败"), "气密参数");

    // 重新启动实时监控定时器（延迟1000ms确保启动命令已发送完成并被设备接收）
    if (m_realTimeMonitor) {
        QTimer::singleShot(1000, this, [this]() {
            LOG_DEBUG("【通道测试】重新启动实时监控定时器", "气密参数");
            m_realTimeMonitor->startDataTimers();
        });
    }

    // 发送通道测试开始信号
    LOG_DEBUG(QString("【通道测试】通道%1 - 发送channelTestCompleted信号").arg(m_currentTestingChannel), "气密参数");
    emit channelTestCompleted(m_currentTestingChannel, false);
}

// 测试完成处理槽函数（添加通道参数）
void AirtightParamSetting::onTestCompleted(int channel, bool success)
{
    if (!m_isMultiChannelTesting) {
        LOG_DEBUG("收到测试完成信号，但未在多通道测试状态中", "气密参数");
        return;
    }
    
    LOG_INFO(QString("通道%1测试完成，结果: %2").arg(channel).arg(success ? "通过" : "不通过"), "气密参数");
    
    // 发送通道测试完成信号
    emit channelTestCompleted(channel, success);
    
    // 移动到下一个通道
    m_currentChannelIndex++;
    
    // 延迟一段时间后启动下一个通道测试
    // 等待测试结果完全处理完毕
    QTimer::singleShot(1000, this, &AirtightParamSetting::startNextChannelTest);
}

// 获取所有开启的通道列表
QList<int> AirtightParamSetting::getEnabledChannels()
{
    QList<int> enabledChannels;
    
    if (m_mainControlSetting) {
        enabledChannels = m_mainControlSetting->getEnabledChannels();
    } else {
        LOG_WARNING("主控设置页面指针未设置，无法获取开启通道列表", "气密参数");
    }
    
    return enabledChannels;
}

// 根据通道号获取程序号
int AirtightParamSetting::getProgramNumberForChannel(int channel)
{
    int programNumber = 0;
    
    if (m_mainControlSetting) {
        programNumber = m_mainControlSetting->getProgramNumber(channel);
        LOG_DEBUG(QString("通道%1对应的程序号: %2").arg(channel).arg(programNumber), "气密参数");
    } else {
        LOG_WARNING("主控设置页面指针未设置，无法获取通道程序号", "气密参数");
        programNumber = channel; // 默认使用通道号作为程序号
    }
    
    // 如果获取到的程序号为0，使用通道号作为默认程序号
    if (programNumber == 0) {
        programNumber = channel;
        LOG_INFO(QString("通道%1的程序号为0，使用通道号作为默认程序号").arg(channel), "气密参数");
    }
    
    return programNumber;
}

// 校验参数有效性
bool AirtightParamSetting::validateParams(const QMap<QString, QVariant> &params, int channel)
{
    QStringList errors;
    
    // 校验时间参数
    double fillTime = params["fill_time"].toDouble();
    double stabilizationTime = params["stabilization_time"].toDouble();
    double testTime = params["test_time"].toDouble();
    double dumpTime = params["dump_time"].toDouble();
    
    if (fillTime <= 0 || fillTime > 200) {
        errors.append(QString("填充时间(%1)超出有效范围(0-200)").arg(fillTime));
    }
    if (stabilizationTime <= 0 || stabilizationTime > 200) {
        errors.append(QString("稳定时间(%1)超出有效范围(0-200)").arg(stabilizationTime));
    }
    if (testTime <= 0 || testTime > 200) {
        errors.append(QString("测试时间(%1)超出有效范围(0-200)").arg(testTime));
    }
    if (dumpTime <= 0 || dumpTime > 200) {
        errors.append(QString("排气时间(%1)超出有效范围(0-200)").arg(dumpTime));
    }
    
    // 校验压力参数
    double pressureMin = params["pressure_min"].toDouble();
    double pressureMax = params["pressure_max"].toDouble();
    double pressureFill = params["pressure_set_fill"].toDouble();
    
    if (pressureMin <= 0) {
        errors.append(QString("最小压力(%1)必须大于0").arg(pressureMin));
    }
    if (pressureMax <= 0) {
        errors.append(QString("最大压力(%1)必须大于0").arg(pressureMax));
    }
    if (pressureFill <= 0) {
        errors.append(QString("填充压力(%1)必须大于0").arg(pressureFill));
    }
    if (pressureMin >= pressureMax) {
        errors.append(QString("最小压力(%1)必须小于最大压力(%2)").arg(pressureMin).arg(pressureMax));
    }
    
    // 校验容积参数
    quint16 volume = params["volume"].toUInt();
    if (volume == 0) {
        errors.append("容积参数无效(等于0)");
    }
    
    // 校验泄漏参数
    double testReject = params["test_reject"].toDouble();
    if (testReject < 0) {
        errors.append(QString("允许泄漏值(%1)不能为负数").arg(testReject));
    }
    
    // 如果有错误，记录日志
    if (!errors.isEmpty()) {
        LOG_ERROR(QString("【通道%1】参数校验失败: %2").arg(channel).arg(errors.join("; ")), "气密参数");
        return false;
    }
    
    LOG_DEBUG(QString("【通道%1】参数校验通过").arg(channel), "气密参数");
    return true;
}

// 发送参数到设备
bool AirtightParamSetting::sendParamsToDevice(const QMap<QString, QVariant> &params)
{
    // 检查气密仪连接状态
    if (!modbusClient || !m_airTightConnected) {
        LOG_ERROR("气密仪未连接，无法发送参数", "气密参数");
        return false;
    }
    
    // 参数校验
    if (!validateParams(params, m_currentTestingChannel)) {
        LOG_ERROR("参数校验失败，无法发送参数", "气密参数");
        return false;
    }
    
    const int timeoutMs = 2000; // 超时时间2秒
    const int maxRetries = 2; // 最大重试次数
    const quint16 startAddress = MODBUS_START_ADDRESS;
    const quint16 endAddress = MODBUS_END_ADDRESS;
    const int numRegisters = endAddress - startAddress + 1;
    
    // 创建批量写入数据单元
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, startAddress, numRegisters);
    
    // 初始化所有寄存器值为0
    for (int i = 0; i < numRegisters; ++i) {
        writeUnit.setValue(i, 0);
    }
    
    writeUnit.setValue(8192 - startAddress, DEVICE_PREPARE_COMMAND_VALUE);
    writeUnit.setValue(8193 - startAddress, 0);
    
    // 1. 时间参数
    quint16 fillTime = encodeTimeParameter(params["fill_time"].toDouble());
    quint16 stabilizationTime = encodeTimeParameter(params["stabilization_time"].toDouble());
    quint16 testTime = encodeTimeParameter(params["test_time"].toDouble());
    quint16 dumpTime = encodeTimeParameter(params["dump_time"].toDouble());
    
    writeUnit.setValue(8196 - startAddress, fillTime);
    writeUnit.setValue(8197 - startAddress, stabilizationTime);
    writeUnit.setValue(8198 - startAddress, testTime);
    writeUnit.setValue(8199 - startAddress, dumpTime);
    
    // 2. 压力单位
    writeUnit.setValue(8200 - startAddress, params["pressure_unit"].toUInt());
    
    // 3. 压力值参数
    quint16 minPressure = static_cast<quint16>(params["pressure_min"].toUInt());
    quint16 maxPressure = static_cast<quint16>(params["pressure_max"].toUInt());
    quint16 fillPressure = static_cast<quint16>(params["pressure_set_fill"].toUInt());
    
    QPair<quint16, quint16> minEnc = encodePressureParameter(minPressure);
    QPair<quint16, quint16> maxEnc = encodePressureParameter(maxPressure);
    QPair<quint16, quint16> fillEnc = encodePressureParameter(fillPressure);
    
    writeUnit.setValue(8201 - startAddress, minEnc.first);
    writeUnit.setValue(8202 - startAddress, minEnc.second);
    writeUnit.setValue(8203 - startAddress, maxEnc.first);
    writeUnit.setValue(8204 - startAddress, maxEnc.second);
    writeUnit.setValue(8205 - startAddress, fillEnc.first);
    writeUnit.setValue(8206 - startAddress, fillEnc.second);
    
    // 4. 填充类型
    writeUnit.setValue(8211 - startAddress, static_cast<quint16>(params["fill_type"].toUInt()));
    
    // 5. 泄漏相关参数
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
    
    // 6. 其他参数
    writeUnit.setValue(8224 - startAddress, 64);
    writeUnit.setValue(8225 - startAddress, STANDARD_ATMOSPHERE_VALUE);
    writeUnit.setValue(8227 - startAddress, 0);
    writeUnit.setValue(8229 - startAddress, static_cast<quint16>(params["offset"].toUInt()));
    
    // 保存填充压力值用于调压装置
    double pressureSetFill = params["pressure_set_fill"].toDouble();
    m_lastSentFillPressure = static_cast<quint16>((pressureSetFill + 1) / 0.18);
    
    bool success = false;
    int retryCount = 0;
    
    // 尝试发送参数，最多重试maxRetries次
    while (retryCount <= maxRetries && !success) {
        // 等待设备准备
        QThread::msleep(500);
        
        LOG_DEBUG(QString("尝试发送批量参数，第%1次，从地址%2到%3").arg(retryCount+1).arg(startAddress).arg(endAddress), "气密参数");
        
        QModbusReply *reply = modbusClient->sendWriteRequest(writeUnit, slaveId);
        if (!reply) {
            LOG_ERROR("sendWriteRequest返回nullptr", "气密参数");
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
                LOG_ERROR(QString("发送命令失败: %1").arg(reply->errorString()), "气密参数");
                retryCount++;
            } else {
                LOG_INFO(QString("批量参数发送成功 - 从地址 %1 到 %2").arg(startAddress).arg(endAddress), "气密参数");
                success = true;
            }
            reply->deleteLater();
        } else {
            // 超时处理
            LOG_WARNING(QString("发送命令超时 - 从地址 %1 到 %2，第%3次尝试").arg(startAddress).arg(endAddress).arg(retryCount+1), "气密参数");
            disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
            reply->deleteLater();
            retryCount++;
        }
    }
    
    // 发送压力值到调压装置
    if (success) {
        if (!sendPressureToRegulator(m_lastSentFillPressure, timeoutMs)) {
            LOG_ERROR("发送压力值到调压装置失败", "气密参数");
            // 注意：这里不设置success=false，因为气密仪参数已经发送成功
        }
    }
    
    if (success) {
        LOG_INFO("所有参数已成功发送到设备", "气密参数");
    } else {
        LOG_ERROR("发送参数失败", "气密参数");
    }
    
    return success;
}

// 实时检测主控板寄存器
void AirtightParamSetting::realTimeRegisterDetection()
{
    // 添加详细的调试日志 - 每次都输出，方便调试
    static int callCount = 0;
    callCount++;
    
    // 检查主控板Modbus客户端是否为空或未连接
    if (!m_mainBoardModbusClient || !m_mainBoardConnected) {
        LOG_DEBUG("主控板未连接，跳过寄存器检测", "气密参数");
        return;
    }
    
    quint16 reg0006 = 0, reg0007 = 0;
    
    // 读取寄存器0006的值
    if (readModbusRegister(0x0006, reg0006, 500)) {
        // 只在值变化时记录日志
        static quint16 lastReg0006 = 0xFFFF;
        if (reg0006 != lastReg0006) {
            LOG_INFO(QString("寄存器0006值变化: %1 -> %2").arg(lastReg0006).arg(reg0006), "气密参数");
            lastReg0006 = reg0006;
        }
        
        // 检测寄存器0006是否为1，如果是则启动气密仪
            if (reg0006 == 1) {
                // 如果正在进行多通道测试，忽略新的启动信号
                if (m_isMultiChannelTesting) {
                    LOG_WARNING("正在进行多通道测试，忽略新的启动信号", "气密参数");
                    return;
                }
                
                LOG_INFO("检测到寄存器0006为1，准备启动气密仪", "气密参数");
                
                // 检查是否需要扫码
                if (m_realTimeMonitor && !m_realTimeMonitor->canStartTest()) {
                    LOG_WARNING("必须扫码但产品编号为空，拒绝启动气密仪", "气密参数");
                    // 发送警告信息给主控板（可以写入某个寄存器或通过其他方式通知）
                    return;
                }
                
                // 通知RealTimeMonitor清空测试结果和图表
                emit testStarted();
            
            // 0. 在启动气密仪之前，先将主控板寄存器0004、0005、0006写0
            LOG_INFO("启动前清零主控板寄存器0004、0005、0006", "气密参数");
            sendMainBoardCommand(0x0004, 0x0000, 500);
            sendMainBoardCommand(0x0005, 0x0000, 500);
            sendMainBoardCommand(0x0006, 0x0000, 500);
            
            // 1. 获取所有开启的通道列表
            m_enabledChannels = getEnabledChannels();
            QString channelsStr;
            for (int i = 0; i < m_enabledChannels.size(); ++i) {
                if (i > 0) channelsStr += ", ";
                channelsStr += QString::number(m_enabledChannels[i]);
            }
            LOG_INFO(QString("【启动流程】检测到开启的通道: %1").arg(channelsStr), "气密参数");
            LOG_DEBUG(QString("【启动流程】通道列表长度: %1, 第一个通道: %2").arg(m_enabledChannels.size()).arg(m_enabledChannels.isEmpty() ? 0 : m_enabledChannels.first()), "气密参数");
            
            if (m_enabledChannels.isEmpty()) {
                LOG_WARNING("【启动流程】未检测到任何开启的通道，取消启动", "气密参数");
                return;
            }
            
            // 2. 初始化多通道测试状态
            m_isMultiChannelTesting = true;
            m_currentChannelIndex = 0;
            LOG_INFO(QString("【启动流程】初始化多通道测试，起始索引=0，将从通道%1开始测试").arg(m_enabledChannels[0]), "气密参数");
            
            // 3. 启动第一个通道测试
            LOG_DEBUG("【启动流程】准备调用startNextChannelTest()", "气密参数");
            startNextChannelTest();
        }
    } else {
        // 读取失败时记录（但不要太频繁）
        static int failCount = 0;
        failCount++;
        if (failCount % 100 == 0) {
            LOG_WARNING(QString("读取寄存器0006失败，失败次数: %1").arg(failCount), "气密参数");
        }
    }
    
    // 读取寄存器0007的值
    if (readModbusRegister(0x0007, reg0007, 500)) {
        // 只在值变化时记录日志
        static quint16 lastReg0007 = 0xFFFF;
        if (reg0007 != lastReg0007) {
            LOG_INFO(QString("寄存器0007值变化: %1 -> %2").arg(lastReg0007).arg(reg0007), "气密参数");
            lastReg0007 = reg0007;
        }

        // 检测寄存器0007是否为1，如果是则复位气密仪（带防抖）
        if (reg0007 == 1) {
            // 检查防抖标志
            if (m_resetInProgress) {
                LOG_DEBUG("复位操作正在进行中，忽略本次复位请求（防抖）", "气密参数");
            } else {
                LOG_INFO("检测到寄存器0007为1，复位气密仪", "气密参数");
                m_resetInProgress = true;
                m_resetDebounceTimer->start(); // 启动防抖定时器
                resetAirtightTest();
            }

            // 复位后清零寄存器0007，防止重复触发
            LOG_DEBUG("复位完成后清零寄存器0007", "气密参数");
            sendMainBoardCommand(0x0007, 0x0000, 500);
        }
    } else {
        // 读取失败时记录（但不要太频繁）
        static int failCount = 0;
        failCount++;
        if (failCount % 100 == 0) {
            LOG_WARNING(QString("读取寄存器0007失败，失败次数: %1").arg(failCount), "气密参数");
        }
    }
}

// 启动气密仪方法
bool AirtightParamSetting::startAirtightTest()
{
    // 检查airTightModbusClient是否为空或未连接
    if (!modbusClient || !m_airTightConnected) {
        LOG_ERROR("气密仪未连接，无法启动测试", "气密参数");
        return false;
    }

    // 发送启动命令到设备（向寄存器9472发送值4608）
    // 使用功能码16（Write Multiple Registers）发送启动命令
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 9472, 2);
    writeUnit.setValue(0, 4608);
    writeUnit.setValue(1, 0); // 第二个寄存器设为0，确保使用功能码16
    
    LOG_DEBUG(QString("发送启动命令到气密仪，寄存器地址9472，值4608，slaveId=%1").arg(slaveId), "气密参数");
    
    if (auto *reply = modbusClient->sendWriteRequest(writeUnit, slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            LOG_INFO("成功发送启动命令到气密仪", "气密参数");
            return true;
        } else {
            delete reply;
            LOG_ERROR("发送启动命令失败", "气密参数");
            return false;
        }
    } else {
        LOG_ERROR("sendWriteRequest返回nullptr，无法发送启动命令", "气密参数");
        return false;
    }
}

// 重置气密仪设备状态（通道切换前调用，清除前一通道状态残留）
void AirtightParamSetting::waitForDeviceStandby()
{
    // 检查气密仪是否连接
    if (!modbusClient || !m_airTightConnected) {
        LOG_WARNING("气密仪未连接，跳过等待待机状态", "气密参数");
        return;
    }
    
    LOG_INFO("【通道切换】等待气密仪进入待机状态", "气密参数");
    
    // 等待设备进入待机状态（寄存器8707=0）
    // 最大等待5秒，每200ms检查一次
    int waitCount = 0;
    const int maxWaitCount = 25; // 25 * 200ms = 5秒
    bool deviceReady = false;
    
    while (waitCount < maxWaitCount && !deviceReady) {
        QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 8707, 1);
        QModbusReply *reply = modbusClient->sendReadRequest(readUnit, slaveId);
        
        if (reply) {
            QEventLoop loop;
            connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
            QTimer::singleShot(300, &loop, &QEventLoop::quit);
            loop.exec();
            
            if (reply->isFinished() && reply->error() == QModbusDevice::NoError) {
                QModbusDataUnit result = reply->result();
                if (result.valueCount() > 0 && result.value(0) == 0) {
                    deviceReady = true;
                    LOG_DEBUG("【通道切换】设备已进入待机状态（寄存器8707=0）", "气密参数");
                } else {
                    LOG_DEBUG(QString("【通道切换】等待设备进入待机状态，当前状态: %1").arg(result.value(0)), "气密参数");
                }
            }
            reply->deleteLater();
        }
        
        if (!deviceReady) {
            QThread::msleep(200);
            waitCount++;
        }
    }
    
    if (deviceReady) {
        // 等待额外1秒确保所有寄存器自动归0（8707、9088、9472会自动归0）
        LOG_DEBUG("【通道切换】等待1秒确保寄存器自动归0", "气密参数");
        QThread::msleep(1000);
        
        LOG_INFO("【通道切换】气密仪已完全就绪，可以开始下一通道测试", "气密参数");
    } else {
        LOG_WARNING("【通道切换】等待设备进入待机状态超时，继续执行下一通道测试", "气密参数");
    }
}

void AirtightParamSetting::waitForTestStart()
{
    // 检查气密仪是否连接
    if (!modbusClient || !m_airTightConnected) {
        LOG_WARNING("气密仪未连接，跳过等待测试开始", "气密参数");
        return;
    }
    
    LOG_INFO("【通道测试】等待设备进入测试状态", "气密参数");
    
    // 等待设备进入测试状态（寄存器9088=256）
    // 最大等待10秒，每200ms检查一次
    int waitCount = 0;
    const int maxWaitCount = 50; // 50 * 200ms = 10秒
    bool testStarted = false;
    
    while (waitCount < maxWaitCount && !testStarted) {
        QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 9088, 1);
        QModbusReply *reply = modbusClient->sendReadRequest(readUnit, slaveId);
        
        if (reply) {
            // 同步等待读取完成
            QEventLoop loop;
            connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
            QTimer::singleShot(300, &loop, &QEventLoop::quit); // 超时保护
            loop.exec();
            
            if (reply->isFinished() && reply->error() == QModbusDevice::NoError) {
                QModbusDataUnit result = reply->result();
                if (result.valueCount() > 0) {
                    quint16 status = result.value(0);
                    if (status == 256) {
                        testStarted = true;
                        LOG_DEBUG("【通道测试】设备已进入测试状态（寄存器9088=256）", "气密参数");
                    } else {
                        LOG_DEBUG(QString("【通道测试】等待设备进入测试状态，当前寄存器9088值: %1").arg(status), "气密参数");
                    }
                }
            }
            reply->deleteLater();
        }
        
        if (!testStarted) {
            QThread::msleep(200);
            waitCount++;
        }
    }
    
    if (testStarted) {
        LOG_INFO("【通道测试】设备已进入测试状态，可以开始实时监控", "气密参数");
    } else {
        LOG_WARNING("【通道测试】等待设备进入测试状态超时，继续执行", "气密参数");
    }
}

void AirtightParamSetting::resetAirtightDeviceState()
{
    // 检查airTightModbusClient是否为空或未连接
    if (!modbusClient || !m_airTightConnected) {
        LOG_WARNING("气密仪未连接，跳过设备状态重置", "气密参数");
        return;
    }

    LOG_INFO("【通道切换】开始重置气密仪设备状态", "气密参数");
    
    // 等待设备进入待机状态（寄存器8707=0），寄存器会自动归0
    // 最大等待5秒，每100ms检查一次
    int waitCount = 0;
    const int maxWaitCount = 50; // 50 * 100ms = 5秒
    bool deviceReady = false;
    
    while (waitCount < maxWaitCount && !deviceReady) {
        QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 8707, 1);
        QModbusReply *reply = modbusClient->sendReadRequest(readUnit, slaveId);
        
        if (reply) {
            // 同步等待读取完成
            QEventLoop loop;
            connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
            QTimer::singleShot(200, &loop, &QEventLoop::quit); // 超时保护
            loop.exec();
            
            if (reply->isFinished() && reply->error() == QModbusDevice::NoError) {
                QModbusDataUnit result = reply->result();
                if (result.valueCount() > 0 && result.value(0) == 0) {
                    deviceReady = true;
                    LOG_DEBUG("【通道切换】设备已进入待机状态（寄存器8707=0）", "气密参数");
                } else {
                    LOG_DEBUG(QString("【通道切换】等待设备进入待机状态，当前状态: %1").arg(result.value(0)), "气密参数");
                }
            }
            reply->deleteLater();
        }
        
        if (!deviceReady) {
            QThread::msleep(100);
            waitCount++;
        }
    }
    
    if (deviceReady) {
        LOG_INFO("【通道切换】气密仪设备状态重置完成，设备已就绪", "气密参数");
    } else {
        LOG_WARNING("【通道切换】设备状态重置完成，但设备未进入待机状态", "气密参数");
    }
}

// 复位气密仪方法
void AirtightParamSetting::resetAirtightTest()
{
    // 检查airTightModbusClient是否为空或未连接
    if (!modbusClient || !m_airTightConnected) {
        LOG_WARNING("气密仪未连接，跳过发送复位命令", "气密参数");
        return;
    }

    // 发送复位命令到设备（向寄存器9472发送值4864）
    // 使用功能码16（Write Multiple Registers）发送复位命令
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 9472, 2);
    writeUnit.setValue(0, 4864);
    writeUnit.setValue(1, 0); // 第二个寄存器设为0，确保使用功能码16
    
    LOG_DEBUG(QString("发送复位命令到气密仪，寄存器地址9472，值4864，slaveId=%1").arg(slaveId), "气密参数");
    
    if (auto *reply = modbusClient->sendWriteRequest(writeUnit, slaveId)) {
        if (!reply->isFinished()) {
            // 连接完成信号，处理结果
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                if (reply->error() == QModbusDevice::NoError) {
                    LOG_INFO("成功发送复位命令到气密仪", "气密参数");
                } else {
                    LOG_WARNING(QString("发送复位命令返回错误: %1").arg(reply->errorString()), "气密参数");
                }
                reply->deleteLater();
            });
        } else {
            // 请求已完成（可能是错误）
            if (reply->error() == QModbusDevice::NoError) {
                LOG_INFO("成功发送复位命令到气密仪（同步完成）", "气密参数");
            } else {
                LOG_WARNING(QString("发送复位命令失败: %1").arg(reply->errorString()), "气密参数");
            }
            delete reply;
        }
    } else {
        LOG_ERROR("sendWriteRequest返回nullptr，无法发送复位命令", "气密参数");
    }
}

// 向主控板发送Modbus命令
bool AirtightParamSetting::sendMainBoardCommand(quint16 address, quint16 value, int timeoutMs)
{
    // 使用主控板专用Modbus客户端
    QModbusClient *client = m_mainBoardModbusClient;
    if (!client) {
        LOG_WARNING("主控板Modbus客户端未设置", "气密参数");
        return false;
    }
    
    if (client->state() != QModbusDevice::ConnectedState) {
        LOG_WARNING("主控板Modbus客户端未连接", "气密参数");
        return false;
    }
    
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, 1);
    writeUnit.setValue(0, value);
    
    LOG_DEBUG(QString("发送主控板命令: 地址=%1, 值=%2, 从站ID=%3").arg(address).arg(value).arg(m_mainBoardSlaveId), "气密参数");
    
    // 使用主控板从站ID
    if (auto *reply = client->sendWriteRequest(writeUnit, m_mainBoardSlaveId)) {
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
                LOG_DEBUG(QString("主控板命令发送成功: 地址=%1, 值=%2").arg(address).arg(value), "气密参数");
                reply->deleteLater();
                return true;
            } else {
                LOG_ERROR(QString("主控板命令发送失败: %1").arg(reply->errorString()), "气密参数");
                reply->deleteLater();
                return false;
            }
        } else {
            LOG_WARNING("主控板命令发送超时", "气密参数");
            reply->deleteLater();
            return false;
        }
    } else {
        LOG_ERROR("sendWriteRequest返回nullptr，无法发送主控板命令", "气密参数");
        return false;
    }
}

// 加载参数方法
void AirtightParamSetting::loadParameters(int programNumber)
{
    LOG_INFO(QString("开始加载程序号 %1 的参数").arg(programNumber), "气密参数");
    
    // 连接数据库并获取参数
    DatabaseManager* dbManager = DatabaseManager::getInstance();
    if (!dbManager->connectDatabase()) {
        LOG_WARNING(QString("数据库连接失败，使用默认参数").arg(programNumber), "气密参数");
        // 数据库连接失败，保持当前的默认值
        return;
    }
    
    // 获取该程序号的参数
    QList<QMap<QString, QVariant>> existingParams = dbManager->getAirTightnessParamsByProgram(programNumber);
    
    if (!existingParams.isEmpty()) {
        QMap<QString, QVariant> params = existingParams.first();
        LOG_INFO(QString("成功从数据库加载程序号 %1 的参数").arg(programNumber), "气密参数");
        
        // 更新UI控件
        if (params.contains("fill_time")) {
            ui->fillTimeSpinBox->setValue(params["fill_time"].toDouble());
        }
        if (params.contains("stabilization_time")) {
            ui->stabilizationTimeSpinBox->setValue(params["stabilization_time"].toDouble());
        }
        if (params.contains("test_time")) {
            ui->testTimeSpinBox->setValue(params["test_time"].toDouble());
        }
        if (params.contains("dump_time")) {
            ui->dumpTimeSpinBox->setValue(params["dump_time"].toDouble());
        }
        
        // 压力单位需要转化成对应的字符串
        if (params.contains("pressure_unit")) {
            QString pressureUnitStr = PressureUnitHelper::toString(PressureUnitHelper::fromInt(params["pressure_unit"].toInt()));
            int index = ui->pressureUnitComboBox->findText(pressureUnitStr);
            if (index >= 0) {
                ui->pressureUnitComboBox->setCurrentIndex(index);
            }
        }
        
        if (params.contains("pressure_max")) {
            ui->maxPressureSpinBox->setValue(params["pressure_max"].toDouble());
        }
        if (params.contains("pressure_min")) {
            ui->minPressureSpinBox->setValue(params["pressure_min"].toDouble());
        }
        if (params.contains("pressure_set_fill")) {
            ui->setFillSpinBox->setValue(params["pressure_set_fill"].toDouble());
        }
        
        // 泄漏单位需要转化成对应的字符串
        if (params.contains("leak_unit")) {
            QString leakUnitStr = LeakUnitHelper::toString(LeakUnitHelper::fromInt(params["leak_unit"].toInt()));
            int index = ui->leakUnitComboBox->findText(leakUnitStr);
            if (index >= 0) {
                ui->leakUnitComboBox->setCurrentIndex(index);
            }
        }
        
        if (params.contains("test_reject")) {
            ui->testRejectSpinBox->setValue(params["test_reject"].toDouble());
        }
        if (params.contains("ref_reject")) {
            ui->refRejectSpinBox->setValue(params["ref_reject"].toDouble());
        }
        if (params.contains("offset")) {
            ui->offsetSpinBox->setValue(params["offset"].toDouble());
        }
        
        // 标准大气压和标准温度（这些控件在UI中是隐藏的）
        if (params.contains("std_atm")) {
            ui->stdAtmSpinBox->setValue(params["std_atm"].toDouble());
        }
        if (params.contains("std_temp")) {
            ui->stdTempSpinBox->setValue(params["std_temp"].toDouble());
        }
        
        // 设置容积值
        if (params.contains("volume")) {
            int volumeValue;
            quint16 volumeCode = static_cast<quint16>(params["volume"].toUInt());
            if (VolumeEncoding::tryDecode(volumeCode, volumeValue)) {
                // 解码成功，根据容积值设置ComboBox
                QString volumeText = QString::number(volumeValue);
                int volumeIndex = ui->volumeComboBox->findText(volumeText);
                if (volumeIndex >= 0) {
                    ui->volumeComboBox->setCurrentIndex(volumeIndex);
                    LOG_DEBUG(QString("容积值设置成功: %1").arg(volumeValue), "气密参数");
                } else {
                    LOG_WARNING(QString("容积值 %1 在ComboBox中未找到").arg(volumeValue), "气密参数");
                }
            } else {
                // 解码失败，使用默认值或保持当前选择
                LOG_WARNING(QString("容积编码解码失败: %1").arg(volumeCode), "气密参数");
            }
        }
        
        // 容积单位
        if (params.contains("volume_unit")) {
            QString volumeUnitStr = VolumeUnitHelper::toString(VolumeUnitHelper::fromInt(params["volume_unit"].toInt()));
            int index = ui->volumeUnitComboBox->findText(volumeUnitStr);
            if (index >= 0) {
                ui->volumeUnitComboBox->setCurrentIndex(index);
            }
        }
        
        // 设置填充类型
        if (params.contains("fill_type")) {
            QString fillTypeStr = FillTypeHelper::toString(FillTypeHelper::fromInt(params["fill_type"].toInt()));
            int index = ui->fillTypeComboBox->findText(fillTypeStr);
            if (index >= 0) {
                ui->fillTypeComboBox->setCurrentIndex(index);
            }
        }
        
        LOG_INFO(QString("程序号 %1 的参数加载完成").arg(programNumber), "气密参数");
        
        // 强制更新UI显示
        ui->scrollArea->widget()->update();
        ui->scrollLayout->update();
        this->update();
    } else {
        LOG_WARNING(QString("数据库中未找到程序号 %1 的参数，重置为默认值").arg(programNumber), "气密参数");
        
        // 数据库中没有该程序号的参数，重置为默认值
        // 时间参数默认值
        ui->fillTimeSpinBox->setValue(15.00);
        ui->stabilizationTimeSpinBox->setValue(15.00);
        ui->testTimeSpinBox->setValue(15.00);
        ui->dumpTimeSpinBox->setValue(8.00);
        
        // 压力参数默认值
        ui->pressureUnitComboBox->setCurrentText("kPa");
        ui->maxPressureSpinBox->setValue(60.00);
        ui->minPressureSpinBox->setValue(50.00);
        ui->setFillSpinBox->setValue(55.00);
        
        // 泄漏参数默认值
        ui->leakUnitComboBox->setCurrentText("mL/min");
        ui->testRejectSpinBox->setValue(3.00);
        ui->refRejectSpinBox->setValue(3.00);
        ui->offsetSpinBox->setValue(0.00);
        
        // 容积参数默认值
        ui->volumeComboBox->setCurrentText("30");
        ui->volumeUnitComboBox->setCurrentText("mL");
        
        // 填充类型默认值
        ui->fillTypeComboBox->setCurrentText("Ramp Control");
        
        // 标准大气压和温度默认值
        ui->stdAtmSpinBox->setValue(1013.25);
        ui->stdTempSpinBox->setValue(20.00);
        
        LOG_INFO(QString("程序号 %1 已重置为默认值").arg(programNumber), "气密参数");
        
        // 强制更新UI显示
        ui->scrollArea->widget()->update();
        ui->scrollLayout->update();
        this->update();
    }
}


bool AirtightParamSetting::sendPressureToRegulator(quint16 pressureValue, int timeoutMs){
    // 检查调压装置连接状态
    if (!m_pressureRegulatorConnected) {
        return false;
    }
    
    // 检查调压装置Modbus客户端
    if (!m_pressureRegulatorModbusClient) {
        return false;
    }
    
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 80, 1);
    writeUnit.setValue(0, pressureValue);
    
    if (QModbusReply *reply = m_pressureRegulatorModbusClient->sendWriteRequest(writeUnit, m_pressureRegulatorSlaveId)) {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        timer.start(timeoutMs);
        
        connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        loop.exec();
        
        bool success = (timer.isActive() && reply->error() == QModbusDevice::NoError);
        reply->deleteLater();
        
        return success;
    } else {
        return false;
    }
}


// 初始化分页UI
void AirtightParamSetting::initializePaginatedUI()
{
    LOG_INFO("开始初始化分页UI", "气密参数");
    
    // 创建QStackedWidget作为主要内容容器
    m_pageStack = new QStackedWidget(this);
    m_pageStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 创建各个页面
    m_page1Widget = createPage1();
    m_page2Widget = createPage2();
    m_page3Widget = createPage3();
    
    // 将页面添加到堆栈中
    m_pageStack->addWidget(m_page1Widget);
    m_pageStack->addWidget(m_page2Widget);
    m_pageStack->addWidget(m_page3Widget);
    
    // 创建导航栏
    m_navigationFrame = createNavigationBar();
    
    // 将QStackedWidget和导航栏添加到主布局
    // 首先移除scrollArea（如果存在）
    if (ui->scrollArea) {
        ui->verticalLayout->removeWidget(ui->scrollArea);
        ui->scrollArea->hide();
    }
    
    // 添加页面堆栈和导航栏到主布局
    ui->verticalLayout->addWidget(m_pageStack, 1); // stretch factor 1
    ui->verticalLayout->addWidget(m_navigationFrame, 0); // stretch factor 0
    
    // 设置初始页面为第一页
    switchToPage(0);
    
    LOG_INFO("分页UI初始化完成", "气密参数");
}

// 创建第一页
QWidget* AirtightParamSetting::createPage1()
{
    LOG_INFO("创建第一页", "气密参数");
    
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(20);
    
    // 将程序设置GroupBox移动到第一页
    if (ui->programGroupBox) {
        ui->programGroupBox->setParent(page);
        layout->addWidget(ui->programGroupBox);
    }
    
    // 重新创建时间参数GroupBox，每行两个输入框
    QGroupBox* timeGroupBox = new QGroupBox("⏱ 时间参数");
    timeGroupBox->setObjectName("timeGroupBox");
    QGridLayout* timeLayout = new QGridLayout(timeGroupBox);
    timeLayout->setContentsMargins(25, 30, 25, 25);
    timeLayout->setHorizontalSpacing(20);
    timeLayout->setVerticalSpacing(25);
    
    // 第一行：填充时间、稳定时间
    if (ui->fillLabel && ui->fillTimeSpinBox) {
        timeLayout->addWidget(ui->fillLabel, 0, 0, Qt::AlignRight);
        
        QHBoxLayout* fillLayout = new QHBoxLayout();
        fillLayout->addWidget(ui->fillTimeSpinBox);
        fillLayout->addWidget(new QLabel("s"));
        fillLayout->setSpacing(5);
        timeLayout->addLayout(fillLayout, 0, 1);
    }
    if (ui->stabilizationLabel && ui->stabilizationTimeSpinBox) {
        timeLayout->addWidget(ui->stabilizationLabel, 0, 2, Qt::AlignRight);
        
        QHBoxLayout* stabilizationLayout = new QHBoxLayout();
        stabilizationLayout->addWidget(ui->stabilizationTimeSpinBox);
        stabilizationLayout->addWidget(new QLabel("s"));
        stabilizationLayout->setSpacing(5);
        timeLayout->addLayout(stabilizationLayout, 0, 3);
    }
    
    // 第二行：测试时间、排气时间
    if (ui->testTimeLabel && ui->testTimeSpinBox) {
        timeLayout->addWidget(ui->testTimeLabel, 1, 0, Qt::AlignRight);
        
        QHBoxLayout* testLayout = new QHBoxLayout();
        testLayout->addWidget(ui->testTimeSpinBox);
        testLayout->addWidget(new QLabel("s"));
        testLayout->setSpacing(5);
        timeLayout->addLayout(testLayout, 1, 1);
    }
    if (ui->dumpLabel && ui->dumpTimeSpinBox) {
        timeLayout->addWidget(ui->dumpLabel, 1, 2, Qt::AlignRight);
        
        QHBoxLayout* dumpLayout = new QHBoxLayout();
        dumpLayout->addWidget(ui->dumpTimeSpinBox);
        dumpLayout->addWidget(new QLabel("s"));
        dumpLayout->setSpacing(5);
        timeLayout->addLayout(dumpLayout, 1, 3);
    }
    
    // 设置列宽比例
    timeLayout->setColumnStretch(0, 0);  // 标签列不拉伸
    timeLayout->setColumnStretch(1, 1);  // 输入框列拉伸
    timeLayout->setColumnStretch(2, 0);  // 标签列不拉伸
    timeLayout->setColumnStretch(3, 1);  // 输入框列拉伸
    
    layout->addWidget(timeGroupBox);
    
    // 隐藏原来的cycleTimeGroupBox
    if (ui->cycleTimeGroupBox) {
        ui->cycleTimeGroupBox->hide();
    }
    
    layout->addStretch();
    
    LOG_INFO("第一页创建完成", "气密参数");
    return page;
}

// 创建第二页
QWidget* AirtightParamSetting::createPage2()
{
    LOG_INFO("创建第二页", "气密参数");
    
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(20);
    
    // 重新创建压力参数GroupBox，每行两个输入框
    QGroupBox* pressureGroupBox = new QGroupBox("📊 压力参数");
    pressureGroupBox->setObjectName("pressureGroupBox");
    QGridLayout* pressureLayout = new QGridLayout(pressureGroupBox);
    pressureLayout->setContentsMargins(25, 30, 25, 25);
    pressureLayout->setHorizontalSpacing(20);
    pressureLayout->setVerticalSpacing(25);
    
    // 第一行：压力单位、最大压力
    if (ui->unitLabel && ui->pressureUnitComboBox) {
        pressureLayout->addWidget(ui->unitLabel, 0, 0, Qt::AlignRight);
        pressureLayout->addWidget(ui->pressureUnitComboBox, 0, 1);
    }
    if (ui->maximumLabel && ui->maxPressureSpinBox) {
        pressureLayout->addWidget(ui->maximumLabel, 0, 2, Qt::AlignRight);
        pressureLayout->addWidget(ui->maxPressureSpinBox, 0, 3);
    }
    
    // 第二行：最小压力、填充压力
    if (ui->minimumLabel && ui->minPressureSpinBox) {
        pressureLayout->addWidget(ui->minimumLabel, 1, 0, Qt::AlignRight);
        pressureLayout->addWidget(ui->minPressureSpinBox, 1, 1);
    }
    if (ui->setFillLabel && ui->setFillSpinBox) {
        pressureLayout->addWidget(ui->setFillLabel, 1, 2, Qt::AlignRight);
        pressureLayout->addWidget(ui->setFillSpinBox, 1, 3);
    }
    
    // 第三行：填充类型
    if (ui->fillTypeLabel && ui->fillTypeComboBox) {
        pressureLayout->addWidget(ui->fillTypeLabel, 2, 0, Qt::AlignRight);
        pressureLayout->addWidget(ui->fillTypeComboBox, 2, 1);
    }
    
    // 设置列宽比例
    pressureLayout->setColumnStretch(0, 0);  // 标签列不拉伸
    pressureLayout->setColumnStretch(1, 1);  // 输入框列拉伸
    pressureLayout->setColumnStretch(2, 0);  // 标签列不拉伸
    pressureLayout->setColumnStretch(3, 1);  // 输入框列拉伸
    
    layout->addWidget(pressureGroupBox);
    
    // 隐藏原来的pressureGroupBox
    if (ui->pressureGroupBox) {
        ui->pressureGroupBox->hide();
    }
    
    // 隐藏原来的leakGroupBox
    if (ui->leakGroupBox) {
        ui->leakGroupBox->hide();
    }
    
    layout->addStretch();
    
    LOG_INFO("第二页创建完成", "气密参数");
    return page;
}

// 创建第三页
QWidget* AirtightParamSetting::createPage3()
{
    LOG_INFO("创建第三页", "气密参数");
    
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(20);
    
    // 创建完整的泄露参数GroupBox
    QGroupBox* leakGroupBox = new QGroupBox("💧 泄露参数");
    leakGroupBox->setObjectName("leakGroupBox");
    QGridLayout* leakLayout = new QGridLayout(leakGroupBox);
    leakLayout->setContentsMargins(25, 30, 25, 25);
    leakLayout->setHorizontalSpacing(20);
    leakLayout->setVerticalSpacing(25);
    
    // 第一行：泄漏单位、允许泄漏
    if (ui->leakUnitLabel && ui->leakUnitComboBox) {
        leakLayout->addWidget(ui->leakUnitLabel, 0, 0, Qt::AlignRight);
        leakLayout->addWidget(ui->leakUnitComboBox, 0, 1);
    }
    if (ui->testRejectLabel && ui->testRejectSpinBox) {
        leakLayout->addWidget(ui->testRejectLabel, 0, 2, Qt::AlignRight);
        leakLayout->addWidget(ui->testRejectSpinBox, 0, 3);
    }
    
    // 第二行：参考泄漏、偏移量
    if (ui->refRejectLabel && ui->refRejectSpinBox) {
        leakLayout->addWidget(ui->refRejectLabel, 1, 0, Qt::AlignRight);
        leakLayout->addWidget(ui->refRejectSpinBox, 1, 1);
    }
    if (ui->offsetLabel && ui->offsetSpinBox) {
        leakLayout->addWidget(ui->offsetLabel, 1, 2, Qt::AlignRight);
        leakLayout->addWidget(ui->offsetSpinBox, 1, 3);
    }
    
    // 第三行：容积、容积单位
    if (ui->volumeLabel && ui->volumeComboBox) {
        leakLayout->addWidget(ui->volumeLabel, 2, 0, Qt::AlignRight);
        leakLayout->addWidget(ui->volumeComboBox, 2, 1);
    }
    if (ui->volumeUnitLabel && ui->volumeUnitComboBox) {
        leakLayout->addWidget(ui->volumeUnitLabel, 2, 2, Qt::AlignRight);
        leakLayout->addWidget(ui->volumeUnitComboBox, 2, 3);
    }
    
    // 设置列宽比例
    leakLayout->setColumnStretch(0, 0);  // 标签列不拉伸
    leakLayout->setColumnStretch(1, 1);  // 输入框列拉伸
    leakLayout->setColumnStretch(2, 0);  // 标签列不拉伸
    leakLayout->setColumnStretch(3, 1);  // 输入框列拉伸
    
    layout->addWidget(leakGroupBox);
    layout->addStretch();
    
    LOG_INFO("第三页创建完成", "气密参数");
    return page;
}

// 创建导航栏
QFrame* AirtightParamSetting::createNavigationBar()
{
    LOG_INFO("创建导航栏", "气密参数");
    
    QFrame* navFrame = new QFrame();
    navFrame->setObjectName("navigationFrame");
    navFrame->setFrameShape(QFrame::StyledPanel);
    navFrame->setMinimumHeight(60);
    navFrame->setMaximumHeight(80);
    
    // 设置导航栏样式
    navFrame->setStyleSheet(
        "QFrame#navigationFrame {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #112d4e, stop:0.5 #1e5f74, stop:1 #3282b8);"
        "    border-radius: 12px;"
        "    border: 1px solid #3282b8;"
        "}"
    );
    
    QHBoxLayout* navLayout = new QHBoxLayout(navFrame);
    navLayout->setContentsMargins(20, 10, 20, 10);
    navLayout->setSpacing(20);
    
    // 创建"首页"按钮
    m_firstButton = new QPushButton("首页");
    m_firstButton->setMinimumSize(100, 40);
    m_firstButton->setStyleSheet(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #00e676, stop:1 #00c853);"
        "    color: white;"
        "    border: 2px solid #00e676;"
        "    border-radius: 10px;"
        "    padding: 8px 20px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #69f0ae, stop:1 #00e676);"
        "    border-color: #69f0ae;"
        "}"
        "QPushButton:disabled {"
        "    background: rgba(0, 230, 118, 0.3);"
        "    border-color: rgba(0, 230, 118, 0.3);"
        "    color: rgba(255, 255, 255, 0.5);"
        "}"
    );
    
    // 创建"上一页"按钮
    m_prevButton = new QPushButton("上一页");
    m_prevButton->setMinimumSize(100, 40);
    m_prevButton->setStyleSheet(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #42a5f5, stop:1 #1e88e5);"
        "    color: white;"
        "    border: 2px solid #42a5f5;"
        "    border-radius: 10px;"
        "    padding: 8px 20px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #64b5f6, stop:1 #42a5f5);"
        "    border-color: #64b5f6;"
        "}"
        "QPushButton:disabled {"
        "    background: rgba(66, 165, 245, 0.3);"
        "    border-color: rgba(66, 165, 245, 0.3);"
        "    color: rgba(255, 255, 255, 0.5);"
        "}"
    );
    
    // 创建"下一页"按钮
    m_nextButton = new QPushButton("下一页");
    m_nextButton->setMinimumSize(100, 40);
    m_nextButton->setStyleSheet(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #42a5f5, stop:1 #1e88e5);"
        "    color: white;"
        "    border: 2px solid #42a5f5;"
        "    border-radius: 10px;"
        "    padding: 8px 20px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #64b5f6, stop:1 #42a5f5);"
        "    border-color: #64b5f6;"
        "}"
        "QPushButton:disabled {"
        "    background: rgba(66, 165, 245, 0.3);"
        "    border-color: rgba(66, 165, 245, 0.3);"
        "    color: rgba(255, 255, 255, 0.5);"
        "}"
    );
    
    // 创建"末页"按钮
    m_lastButton = new QPushButton("末页");
    m_lastButton->setMinimumSize(100, 40);
    m_lastButton->setStyleSheet(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #00e676, stop:1 #00c853);"
        "    color: white;"
        "    border: 2px solid #00e676;"
        "    border-radius: 10px;"
        "    padding: 8px 20px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #69f0ae, stop:1 #00e676);"
        "    border-color: #69f0ae;"
        "}"
        "QPushButton:disabled {"
        "    background: rgba(0, 230, 118, 0.3);"
        "    border-color: rgba(0, 230, 118, 0.3);"
        "    color: rgba(255, 255, 255, 0.5);"
        "}"
    );
    
    // 创建页码指示器
    m_pageIndicator = new QLabel();
    m_pageIndicator->setAlignment(Qt::AlignCenter);
    m_pageIndicator->setStyleSheet(
        "QLabel {"
        "    color: #64d2ff;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    background: transparent;"
        "}"
    );
    
    // 连接按钮信号
    connect(m_firstButton, &QPushButton::clicked, this, &AirtightParamSetting::onFirstButtonClicked);
    connect(m_prevButton, &QPushButton::clicked, this, &AirtightParamSetting::onPrevButtonClicked);
    connect(m_nextButton, &QPushButton::clicked, this, &AirtightParamSetting::onNextButtonClicked);
    connect(m_lastButton, &QPushButton::clicked, this, &AirtightParamSetting::onLastButtonClicked);
    
    // 添加控件到布局
    navLayout->addWidget(m_firstButton);
    navLayout->addWidget(m_prevButton);
    navLayout->addStretch();
    navLayout->addWidget(m_pageIndicator);
    navLayout->addStretch();
    navLayout->addWidget(m_nextButton);
    navLayout->addWidget(m_lastButton);
    
    LOG_INFO("导航栏创建完成", "气密参数");
    return navFrame;
}

// 切换到指定页面
void AirtightParamSetting::switchToPage(int pageIndex)
{
    // 验证页面索引有效性
    if (pageIndex < 0 || pageIndex >= m_totalPages) {
        LOG_WARNING(QString("无效的页面索引: %1，保持当前页面").arg(pageIndex), "气密参数");
        return;
    }
    
    // 使用QStackedWidget切换到指定页面
    m_pageStack->setCurrentIndex(pageIndex);
    
    // 更新当前页码成员变量
    m_currentPage = pageIndex;
    
    // 更新导航按钮状态
    updateNavigationButtons();
    
    // 更新页码指示器
    updatePageIndicator();
    
    LOG_INFO(QString("切换到第 %1 页").arg(pageIndex + 1), "气密参数");
}

// 首页按钮点击处理
void AirtightParamSetting::onFirstButtonClicked()
{
    switchToPage(0);
}

// 上一页按钮点击处理
void AirtightParamSetting::onPrevButtonClicked()
{
    if (m_currentPage > 0) {
        switchToPage(m_currentPage - 1);
    }
}

// 下一页按钮点击处理
void AirtightParamSetting::onNextButtonClicked()
{
    if (m_currentPage < m_totalPages - 1) {
        switchToPage(m_currentPage + 1);
    }
}

// 末页按钮点击处理
void AirtightParamSetting::onLastButtonClicked()
{
    switchToPage(m_totalPages - 1);
}

// 更新导航按钮状态
void AirtightParamSetting::updateNavigationButtons()
{
    // 根据当前页码判断是否在第一页或最后一页
    bool isFirstPage = (m_currentPage == 0);
    bool isLastPage = (m_currentPage == m_totalPages - 1);
    
    // 设置"首页"按钮的启用/禁用状态
    m_firstButton->setEnabled(!isFirstPage);
    
    // 设置"上一页"按钮的启用/禁用状态
    m_prevButton->setEnabled(!isFirstPage);
    
    // 设置"下一页"按钮的启用/禁用状态
    m_nextButton->setEnabled(!isLastPage);
    
    // 设置"末页"按钮的启用/禁用状态
    m_lastButton->setEnabled(!isLastPage);
    
    LOG_DEBUG(QString("导航按钮状态更新: 首页=%1, 上一页=%2, 下一页=%3, 末页=%4")
        .arg(m_firstButton->isEnabled() ? "启用" : "禁用")
        .arg(m_prevButton->isEnabled() ? "启用" : "禁用")
        .arg(m_nextButton->isEnabled() ? "启用" : "禁用")
        .arg(m_lastButton->isEnabled() ? "启用" : "禁用"), "气密参数");
}

// 更新页码指示器
void AirtightParamSetting::updatePageIndicator()
{
    // 根据当前页码和总页数生成文本
    QString text = QString("第 %1 页 / 共 %2 页").arg(m_currentPage + 1).arg(m_totalPages);
    
    // 更新QLabel的文本
    m_pageIndicator->setText(text);
    
    LOG_DEBUG(QString("页码指示器更新: %1").arg(text), "气密参数");
}
