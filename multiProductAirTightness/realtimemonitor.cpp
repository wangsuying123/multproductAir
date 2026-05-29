#include "realtimemonitor.h"
#include "ui_realtimemonitor.h"
#include <QDateTime>
#include <QRandomGenerator>
#include <QPainter>
#include <QMessageBox>
#include <QEventLoop>
#include <QApplication>
#include <QTimer>
#include "databasemanager.h"
#include "enum/pressureUnit.h"
#include "logmanager.h"
#include "mainControlSetting.h"

RealTimeMonitor::RealTimeMonitor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RealTimeMonitor),
    timeTimer(new QTimer(this)),
    dataTimer(new QTimer(this)),
    testResultTimer(new QTimer(this)),
    m_testTimeoutTimer(new QTimer(this)),
    m_readDataTimeoutTimer(new QTimer(this)),
    dataCount(0),
    airTightModbusClient(nullptr),
    mainBoardModbusClient(nullptr),
    pressureRegulatorModbusClient(nullptr),
    airTightSlaveId(1),
    mainBoardSlaveId(1),
    pressureRegulatorSlaveId(1),
    programNumber(1),
    m_currentTestingChannel(0),
    m_isCommunicationError(false),
    m_isReadingData(false),
    m_mainControlSetting(nullptr),
    m_airTightnessParamsDao(new AirTightnessParamsDao())
{
    ui->setupUi(this);
    
    // 初始化实时数据显示的默认值
    if (ui->pressureValueLabel) {
        ui->pressureValueLabel->setText("0.00 ---");
    }
    if (ui->leakValueLabel) {
        ui->leakValueLabel->setText("0.00 ---");
    }
    // 移除固定几何尺寸，确保页面能正确适应窗口大小变化
    this->setGeometry(QRect());
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 设置布局管理器的属性，确保它能正确适应窗口大小变化
    if (this->layout()) {
        this->layout()->setSizeConstraint(QLayout::SetNoConstraint);
        this->layout()->setContentsMargins(5, 5, 5, 5);
        this->layout()->setSpacing(5);
    }

    // 初始化定时器
    connect(timeTimer, &QTimer::timeout, this, &RealTimeMonitor::updateTime);
    connect(dataTimer, &QTimer::timeout, this, &RealTimeMonitor::updateData);
    connect(testResultTimer, &QTimer::timeout, this, &RealTimeMonitor::readTestResultData);
    
    // 初始化读取数据超时保护定时器（5秒后自动重置m_isReadingData）
    m_readDataTimeoutTimer->setSingleShot(true);
    connect(m_readDataTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_isReadingData) {
            LOG_WARNING("读取数据超时保护触发，重置m_isReadingData标志", "实时监控");
            m_isReadingData = false;
        }
    });

    // 启动定时器，1秒更新一次时间，2000毫秒更新一次数据（降低通信频率，避免冲突）
    timeTimer->start(1000);
    dataTimer->start(2000);  // 修改为2秒更新一次实时数据，减少Modbus通信压力
    testResultTimer->start(1000); // 1秒读取一次测试结果

    // 初始化时间显示
    updateTime();
    
    // 初始化折线图
    chart = new QChart();
    leakSeries = new QLineSeries();
    pressureSeries = new QLineSeries();
    
    // 设置折线图标题和颜色
    leakSeries->setName("泄露值");
    leakSeries->setColor(QColor(255, 70, 131));
    pressureSeries->setName("压力值");
    pressureSeries->setColor(QColor(50, 150, 255));
    
    // 将折线添加到图表
    chart->addSeries(leakSeries);
    chart->addSeries(pressureSeries);
    
    // 创建坐标轴
    axisX = new QValueAxis();
    axisYLeft = new QValueAxis(); // 左边Y轴：压力值
    axisYRight = new QValueAxis(); // 右边Y轴：泄漏值
    
    // 设置坐标轴范围和刻度
    axisX->setRange(0, 50);
    axisX->setLabelFormat("%d");
    axisX->setTitleText("时间 (s)");
    
    // 左边Y轴：压力值，最大值200
    axisYLeft->setRange(0, 200);
    axisYLeft->setLabelFormat("%.1f");
    axisYLeft->setTitleText("压力值");
    axisYLeft->setLinePenColor(pressureSeries->color());
    axisYLeft->setLabelsColor(pressureSeries->color());
    axisYLeft->setTitleBrush(pressureSeries->color());
    
    // 右边Y轴：泄漏值，设置合适的最大值
    axisYRight->setRange(0, 10);
    axisYRight->setLabelFormat("%.2f");
    axisYRight->setTitleText("泄漏值");
    axisYRight->setLinePenColor(leakSeries->color());
    axisYRight->setLabelsColor(leakSeries->color());
    axisYRight->setTitleBrush(leakSeries->color());
    
    // 添加坐标轴到图表
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisYLeft, Qt::AlignLeft);
    chart->addAxis(axisYRight, Qt::AlignRight);
    
    // 将序列关联到坐标轴
    pressureSeries->attachAxis(axisX);
    pressureSeries->attachAxis(axisYLeft);
    leakSeries->attachAxis(axisX);
    leakSeries->attachAxis(axisYRight);
    
    // 设置图表标题
    chart->setTitle("实时数据监测");
    
    // 隐藏图例
    chart->legend()->hide();
    
    // 初始化实时运行参数显示为 "--"
    ui->paramValue1->setText("--");  // 填充时间
    ui->paramValue2->setText("--");  // 稳定时间
    ui->paramValue3->setText("--");  // 测试时间
    ui->paramValue4->setText("--");  // 排气时间
    ui->paramValue5->setText("--");  // 填充压力
    ui->paramValue7->setText("--");  // 程序号
    
    // 初始化本次测试结果显示为 "--"
    ui->resultValue5->setText("--");  // 压力值
    ui->resultValue6->setText("--");  // 泄漏值
    ui->resultValue8->setText("--");  // 测试2压力值
    
    // 初始化时从数据库加载测试结果汇总统计
    updateTestResultSummary();
    
    // 初始化 ChartDialog
    chartDialog = nullptr;
    
    // 初始化产品编号输入框和扫码复选框
    productId = "";
    scanRequired = false;
    if (ui->productIdLineEdit) {
        ui->productIdLineEdit->setText("");
        // 设置输入框为只读模式，只能通过扫码输入
        ui->productIdLineEdit->setReadOnly(false);
        // 安装事件过滤器来处理焦点事件
        ui->productIdLineEdit->installEventFilter(this);
        connect(ui->productIdLineEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
            this->productId = text;
            // 更新当前产品编号显示
            updateCurrentProductId(text);
        });
        // 监听回车键，扫码枪通常会在输入完成后发送回车键
        connect(ui->productIdLineEdit, &QLineEdit::returnPressed, this, [this]() {
            // 扫码完成后自动清空输入框，准备下一次输入
            ui->productIdLineEdit->clear();
            ui->productIdLineEdit->selectAll();
            LOG_DEBUG("产品编号输入框收到回车键，已清空内容准备下次输入", "实时监控");
        });
    }
    if (ui->scanRequiredCheckBox) {
        ui->scanRequiredCheckBox->setChecked(false);
        connect(ui->scanRequiredCheckBox, &QCheckBox::stateChanged, this, [this](int state) {
            this->scanRequired = (state == Qt::Checked);
        });
    }
    
    // 初始化当前产品编号显示
    if (ui->currentProductIdValue) {
        ui->currentProductIdValue->setText("---");
    }
    
    // 连接UI中的全屏查看按钮
    connect(ui->fullScreenButton, &QPushButton::clicked, this, &RealTimeMonitor::onExpandChartButtonClicked);
}

RealTimeMonitor::~RealTimeMonitor()
{
    delete ui;
    delete timeTimer;
    delete dataTimer;
    delete testResultTimer;
    delete m_testTimeoutTimer;
    
    // 释放折线图资源
    delete leakSeries;
    delete pressureSeries;
    delete axisX;
    delete axisYLeft;
    delete axisYRight;
    delete chart;
    
    // 释放数据库相关资源
    delete m_airTightnessParamsDao;
    
    // 安全删除 ChartDialog
    if (chartDialog) {
        delete chartDialog;
        chartDialog = nullptr;
    }
}

void RealTimeMonitor::clearTestResult()
{
    // 清空通道测试结果跟踪
    channelTestResults.clear();
    // 重置测试结果命令发送标志
    m_resultSent = false;
    
    // 使用主线程更新UI
    QMetaObject::invokeMethod(this, [this]() {
        // 1. 清空图表数据
        leakSeries->clear();
        pressureSeries->clear();
        dataCount = 0;
        
        // 2. 重置坐标轴范围
        axisX->setRange(0, 50);
        
        // 4. 清空运行参数
        ui->paramValue1->setText("--");  // 填充时间
        ui->paramValue2->setText("--");  // 稳定时间
        ui->paramValue3->setText("--");  // 测试时间
        ui->paramValue4->setText("--");  // 排气时间
        ui->paramValue5->setText("--");  // 填充压力
        ui->paramValue7->setText("--");  // 程序号
        
        // 5. 清空测试1结果面板值
        if (ui->resultValue5) ui->resultValue5->setText("--"); // 测试1压力值
        if (ui->resultValue6) ui->resultValue6->setText("--"); // 测试1泄漏值
        if (ui->resultValue7) {
            ui->resultValue7->setText("--"); // 测试1结果
            ui->resultValue7->setStyleSheet("font-size: 13px; font-weight: 600; color: #9e9e9e; padding: 2px 6px; border-radius: 3px; background: rgba(158, 158, 158, 0.1); border: 1px solid #9e9e9e;");
        }
        
        // 清空测试2结果面板值
        if (ui->resultValue8) ui->resultValue8->setText("--"); // 测试2压力值
        if (ui->resultValue9) ui->resultValue9->setText("--"); // 测试2泄漏值
        if (ui->resultValue10) {
            ui->resultValue10->setText("--"); // 测试2结果
            ui->resultValue10->setStyleSheet("font-size: 13px; font-weight: 600; color: #9e9e9e; padding: 2px 6px; border-radius: 3px; background: rgba(158, 158, 158, 0.1); border: 1px solid #9e9e9e;");
        }
        
        // 清空测试3结果面板值
        if (ui->resultValue11) ui->resultValue11->setText("--"); // 测试3压力值
        if (ui->resultValue12) ui->resultValue12->setText("--"); // 测试3泄漏值
        if (ui->resultValue13) {
            ui->resultValue13->setText("--"); // 测试3结果
            ui->resultValue13->setStyleSheet("font-size: 13px; font-weight: 600; color: #9e9e9e; padding: 2px 6px; border-radius: 3px; background: rgba(158, 158, 158, 0.1); border: 1px solid #9e9e9e;");
        }
        
        // 重置总测试结果
        if (ui->resultValue14) {
            ui->resultValue14->setText("--");
            ui->resultValue14->setStyleSheet("font-size: 16px; font-weight: 700; color: #9e9e9e; padding: 3px 10px; border-radius: 4px; background: rgba(158, 158, 158, 0.15); border: 2px solid #9e9e9e;");
        }
        
        // 6. 重置进度条
        if (ui->testProgressBar) {
            ui->testProgressBar->setValue(0);
            ui->testProgressBar->setStyleSheet("QProgressBar { background-color: #e0e0e0; border: 1px solid #bdbdbd; border-radius: 4px; } QProgressBar::chunk { background-color: #9e9e9e; }");
        }
        
        // 8. 重置测试进程标题（rightPanelTitle控件已移除）
        
        // 注意：产品编号不应在此处清空，保留用于记录测试数据
        
    }, Qt::QueuedConnection);
}

void RealTimeMonitor::updateTime()
{
    // 更新当前时间
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timeStr = currentTime.toString("yyyy-MM-dd hh:mm:ss");
    // ui->timeInfo->setText(QString("当前时间：%1").arg(timeStr)); // 已移除timeInfo控件
}

void RealTimeMonitor::updateData()
{
    // 如果通信异常，显示错误状态
    if (m_isCommunicationError) {
        updateCommunicationErrorUI();
        return;
    }
    // 调用实际的实时数据读取方法，替换模拟数据
    readRealTimeData();
}

void RealTimeMonitor::updateCommunicationErrorUI()
{
    // 更新UI显示通信异常状态
    QMetaObject::invokeMethod(this, [this]() {
        if (ui->pressureValueLabel) {
            ui->pressureValueLabel->setText("--- ---");
        }
        if (ui->leakValueLabel) {
            ui->leakValueLabel->setText("--- ---");
        }
        
        // 更新测试状态显示
        if (ui->testProgressBar) {
            ui->testProgressBar->setValue(0);
            ui->testProgressBar->setStyleSheet("QProgressBar { background-color: #ffebee; border: 1px solid #ef5350; border-radius: 4px; } QProgressBar::chunk { background-color: #ef5350; }");
        }
    }, Qt::QueuedConnection);
}



void RealTimeMonitor::updateOperatorInfo(const QString& username, const QString& role)
{
    // 保存当前操作员信息
    currentOperatorName = username;
    currentOperatorRole = role;
    
    LOG_INFO(QString("更新操作员信息：%1 (%2)").arg(username).arg(role), "实时监控");
}

void RealTimeMonitor::updateCurrentProductId(const QString& productId)
{
    // 更新成员变量
    this->productId = productId;
    
    // 更新显示
    if (ui->currentProductIdValue) {
        if (productId.isEmpty()) {
            ui->currentProductIdValue->setText("---");
        } else {
            ui->currentProductIdValue->setText(productId);
        }
    }
}

void RealTimeMonitor::onProgramNumberReceived(int programNumber)
{
    // 更新成员变量
    this->programNumber = programNumber;
    
    // 更新实时运行参数面板中的程序号显示
    ui->paramValue7->setText(QString("%1").arg(programNumber));
    
    // 通过程序号去数据库查询气密仪参数
    QList<QMap<QString, QVariant>> paramsList = m_airTightnessParamsDao->getParamsByProgram(programNumber);
    
    if (!paramsList.isEmpty()) {
        QMap<QString, QVariant> params = paramsList.first();
        
        // 填充时间（数据库字段：fill_time）
        if (params.contains("fill_time")) {
            ui->paramValue1->setText(QString("%1").arg(params["fill_time"].toInt()));
        }
        // 稳定时间（数据库字段：stabilization_time）
        if (params.contains("stabilization_time")) {
            ui->paramValue2->setText(QString("%1").arg(params["stabilization_time"].toInt()));
        }
        
        // 测试时间（数据库字段：test_time）
        if (params.contains("test_time")) {
            ui->paramValue3->setText(QString("%1").arg(params["test_time"].toInt()));
        }
        
        // 排气时间（数据库字段：dump_time）
        if (params.contains("dump_time")) {
            ui->paramValue4->setText(QString("%1").arg(params["dump_time"].toInt()));
        }
        
        // 填充压力（值+单位）（数据库字段：pressure_set_fill, pressure_unit）
        if (params.contains("pressure_set_fill") && params.contains("pressure_unit")) {
            // 需要把单位代码转成相应的枚举值
            PressureUnit unit = static_cast<PressureUnit>(params["pressure_unit"].toInt());
            ui->paramValue5->setText(QString("%1 %2").arg(params["pressure_set_fill"].toDouble()).arg(PressureUnitHelper::toString(unit)));
        }
    }
}

void RealTimeMonitor::setAirTightModbusClient(QModbusClient *client)
{
    airTightModbusClient = client;
}

void RealTimeMonitor::setMainBoardModbusClient(QModbusClient *client)
{
    mainBoardModbusClient = client;
}

void RealTimeMonitor::setPressureRegulatorModbusClient(QModbusClient *client)
{
    pressureRegulatorModbusClient = client;
}

void RealTimeMonitor::setAirTightSlaveId(int slaveId)
{
    airTightSlaveId = slaveId;
}

void RealTimeMonitor::setMainBoardSlaveId(int slaveId)
{
    mainBoardSlaveId = slaveId;
}

void RealTimeMonitor::setPressureRegulatorSlaveId(int slaveId)
{
    pressureRegulatorSlaveId = slaveId;
}

bool RealTimeMonitor::writeDeviceData(quint16 address, quint16 value, QModbusDataUnit::RegisterType type) {
    // 使用try-catch块捕获所有异常，防止程序崩溃
    try {
        Q_UNUSED(type);
        
        // 默认使用气密仪Modbus客户端
        QModbusClient *modbusClient = airTightModbusClient;
        int slaveId = airTightSlaveId;
        
        // 检查Modbus客户端是否为空
        if (!modbusClient) {
            return false;
        }

        if (modbusClient->state() != QModbusDevice::ConnectedState) {
            return false;
        }

        // 强制使用功能码16（Write Multiple Registers）
        QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, 2);
        writeUnit.setValue(0, value);
        writeUnit.setValue(1, 0); // 第二个寄存器设为0，确保使用功能码16

        if (auto *reply = modbusClient->sendWriteRequest(writeUnit, slaveId)) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                return true;
            } else {
                delete reply;
                return false;
            }
        } else {
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("写入设备数据时发生异常: %1").arg(e.what()), "实时监控");
        return false;
    } catch (...) {
        LOG_ERROR("写入设备数据时发生未知异常", "实时监控");
        return false;
    }
}

void RealTimeMonitor::readRealTimeData()
{
    // 检查是否正在读取数据，避免Modbus通信冲突
    if (m_isReadingData) {
        LOG_DEBUG("正在读取数据中，跳过本次实时数据读取", "实时监控");
        return;
    }

    // 检查airTightModbusClient是否为空，启动时可能还未初始化，跳过本次读取
    if (!airTightModbusClient) {
        // 静默处理，避免启动时弹出大量错误提示
        LOG_DEBUG("气密仪Modbus客户端未设置，跳过数据读取", "实时监控");
        return;
    }

    if (airTightModbusClient->state() != QModbusDevice::ConnectedState) {
        // 设备未连接时静默处理，避免频繁弹窗
        LOG_DEBUG("气密仪未连接，跳过数据读取", "实时监控");
        return;
    }

    LOG_DEBUG("开始读取气密仪实时数据...", "实时监控");

    m_isReadingData = true;
    
    // 启动超时保护定时器（5秒后自动重置m_isReadingData）
    m_readDataTimeoutTimer->start(5000);

    // 异步读取主要数据（寄存器8705-8717）
    readDeviceDataAsync(8705, 13, QModbusDataUnit::HoldingRegisters,
                        [this](bool success, const QModbusDataUnit &data) {
                            // 停止超时保护定时器
                            if (m_readDataTimeoutTimer->isActive()) {
                                m_readDataTimeoutTimer->stop();
                            }
                            
                            m_isReadingData = false;
                            if (success) {
                                //LOG_DEBUG("成功读取气密仪寄存器数据", "实时监控");
                                // 处理主要寄存器数据
                                processMainRegisterData(data);
                            } else {
                                LOG_ERROR(QString("读取主要寄存器数据失败：%1").arg(airTightModbusClient->errorString()), "实时监控");
                            }
                        });
}

// 实时读取测试结果数据
void RealTimeMonitor::readTestResultData()
{
    // 检查是否正在读取数据，避免Modbus通信冲突
    if (m_isReadingData) {
        LOG_DEBUG("正在读取数据中，跳过本次测试结果读取", "实时监控");
        return;
    }

    // 检查airTightModbusClient是否为空
    if (!airTightModbusClient) {
        return;
    }

    if (airTightModbusClient->state() != QModbusDevice::ConnectedState) {
        return;
    }

    m_isReadingData = true;
    
    // 启动超时保护定时器（5秒后自动重置m_isReadingData）
    m_readDataTimeoutTimer->start(5000);

    // 读取寄存器9088-9100的测试结果数据
    readDeviceDataAsync(9088, 13, QModbusDataUnit::HoldingRegisters,
        [this](bool readSuccess, const QModbusDataUnit &data) {
            // 停止超时保护定时器
            if (m_readDataTimeoutTimer->isActive()) {
                m_readDataTimeoutTimer->stop();
            }
            
            m_isReadingData = false;
            if (readSuccess) {
                quint16 reg9088Value = data.value(0);
                
                // 寄存器9088只有两个状态：
                // 0：未测试或测试未完成
                // 256：测试完成，可以读取结果
                
                // 只有当从0变为256时才处理测试结果
                // 避免从256变回0时重复触发
                if (reg9088Value == 256 && m_lastReg9088Value == 0) {
                    LOG_INFO("寄存器9088从0变为256，测试完成，开始处理测试结果", "实时监控");
                    processTestResultData(data);
                    m_lastReg9088Value = reg9088Value;
                } else if (reg9088Value != m_lastReg9088Value) {
                    // 记录状态变化但不处理
                    LOG_INFO(QString("测试阶段变化：%1 -> %2").arg(m_lastReg9088Value).arg(reg9088Value), "实时监控");
                    m_lastReg9088Value = reg9088Value;
                }
            } else {
                LOG_DEBUG("读取测试结果寄存器9088-9100失败", "实时监控");
            }
        });
}

void RealTimeMonitor::processMainRegisterData(const QModbusDataUnit &data){
    // 使用try-catch块捕获所有异常，防止程序崩溃
    try {
        QMetaObject::invokeMethod(this, [this, data]() {
        if (data.valueCount() < 13) {
            return;
        }

        // 从寄存器读取数据
        quint16 deviceStatus = 0;
        quint16 processValue = 0;
        quint16 pressureValue = 0;
        quint16 pressureHighValue = 0; // 高16位（寄存器8709）
        quint16 pressureUnit = 0;
        quint16 leakValue = 0;
        quint16 leakValue2 = 0; // 8712 第二寄存器
        quint16 leakUnit = 0;
        quint16 atmPressureValue = 0;
        quint16 atmPressureUnit = 0;
        quint16 temperatureValue = 0;
        quint16 temperatureUnit = 0;

        // 记录每个参数的读取状态
        bool deviceStatusRead = false;
        bool processValueRead = false;
        bool pressureValueRead = false;
        bool pressureHighValueRead = false; // 新增：压力高位读取状态
        bool pressureUnitRead = false;
        bool leakValueRead = false;
        bool leakValue2Read = false; // 8712 第二寄存器读取状态
        bool leakUnitRead = false;
        bool atmPressureValueRead = false;
        bool atmPressureUnitRead = false;
        bool temperatureValueRead = false;
        bool temperatureUnitRead = false;

        // 提取各个寄存器的值
        try {
            if (data.valueCount() > 1) { deviceStatus = data.value(1); deviceStatusRead = true; } // 设备状态 - 地址8706
            if (data.valueCount() > 2) { processValue = data.value(2); this->register8707Value = processValue; processValueRead = true; } // 测试进程 - 地址8707，同时保存到全局变量
            if (data.valueCount() > 3) { pressureValue = data.value(3); pressureValueRead = true; } // 压力低位编码 - 地址8708
            if (data.valueCount() > 4) { pressureHighValue = data.value(4); pressureHighValueRead = true; } // 压力高位编码 - 地址8709
            if (data.valueCount() > 5) { pressureUnit = data.value(5); pressureUnitRead = true; } // Pressure单位 - 地址8710
            if (data.valueCount() > 6) { leakValue = data.value(6); leakValueRead = true; } // 泄露值 - 地址8711
            if (data.valueCount() > 7) { leakValue2 = data.value(7); leakValue2Read = true; } // 泄露值第二寄存器 - 地址8712
            if (data.valueCount() > 8) { leakUnit = data.value(8); leakUnitRead = true; } // Leak单位 - 地址8713
            if (data.valueCount() > 9) { atmPressureValue = data.value(9); atmPressureValueRead = true; } // 实时标准大气压 - 地址8714
            if (data.valueCount() > 10) { atmPressureUnit = data.value(10); atmPressureUnitRead = true; } // 单位 - 地址8715
            if (data.valueCount() > 11) { temperatureValue = data.value(11); temperatureValueRead = true; } // 实时温度 - 地址8716
            if (data.valueCount() > 12) { temperatureUnit = data.value(12); temperatureUnitRead = true; } // 单位 - 地址8717
        } catch (const std::exception& e) {
            LOG_ERROR(QString("解析寄存器值时发生异常: %1").arg(e.what()), "实时监控");
        }

        // 压力值计算：合并高低16位，转换为实际值
        double pressure = 0.0;
        QString pressureUnitStr = ""; // 压力单位字符串
        // 泄露值计算
        double leak = 0.0;
        QString leakUnitStr = ""; // 泄漏单位字符串
        
        // 只要压力相关参数读取成功就更新压力值显示
        if (pressureValueRead && pressureHighValueRead) {
            // 1. 压力值参数解码：低位(8708)和高位(8709)分别字节交换，根据高位是否为0选择缩放因子
            auto swap16 = [](quint16 v) -> quint16 { return static_cast<quint16>((v << 8) | (v >> 8)); };
            quint16 lowOrig = swap16(pressureValue);      // 8708 低位原始值
            quint16 highOrig = swap16(pressureHighValue); // 8709 高位原始值
            
            // 合并32位值并正确处理有符号数
            qint32 raw = 0;
            quint32 combined = (static_cast<quint32>(highOrig) << 16) | static_cast<quint32>(lowOrig);
            
            // 正确转换为有符号32位整数
            if (combined & 0x80000000) {
                raw = static_cast<qint32>(combined - 0x100000000);
            } else {
                raw = static_cast<qint32>(combined);
            }
            
            pressure = static_cast<double>(raw) / 1000.0;
            
            // 添加数据有效性验证：压力值应该在合理范围内（-1000 到 1000 KPa 之间）
            bool isValidPressure = (pressure >= -1000.0) && (pressure <= 1000.0);
            
            if (isValidPressure) {
                // 更新当前测试进程面板中的实时压力值显示
                if (ui->pressureValueLabel) {
                    ui->pressureValueLabel->setText(QString::number(pressure, 'f', 2) + " " + pressureUnitStr);
                }
            } else {
                // 无效数据，显示错误提示或保持上次有效值
                LOG_WARNING(QString("压力值超出合理范围: %1 KPa，原始数据: low=0x%2, high=0x%3")
                    .arg(pressure)
                    .arg(lowOrig, 4, 16, QChar('0'))
                    .arg(highOrig, 4, 16, QChar('0')), "实时监控");
                // 可以选择不更新UI，保持上次的有效值
            }
        }
        
        // 根据pressureUnit获取对应的字符串表示
        if (pressureUnitRead) {
            PressureUnit pressureUnitEnum = PressureUnitHelper::fromInt(static_cast<int>(pressureUnit));
            pressureUnitStr = PressureUnitHelper::toString(pressureUnitEnum);
        }
            
        // 根据leakUnit获取对应的字符串表示
        if (leakUnitRead) {
            LeakUnit leakUnitEnum = LeakUnitHelper::fromInt(static_cast<int>(leakUnit));
            leakUnitStr = LeakUnitHelper::toString(leakUnitEnum);
        }

        // 只要泄漏相关参数读取成功就更新泄漏值显示
        if (leakValueRead && leakValue2Read && leakUnitRead) {
            // 2. 泄漏值处理 - 使用两个寄存器进行字节交换后相减并按1000缩放
            auto swap16 = [](quint16 v) -> quint16 { return static_cast<quint16>((v << 8) | (v >> 8)); };
            quint16 r1 = leakValue;   // 寄存器8711
            quint16 r2 = leakValue2;  // 寄存器8712
            quint16 r1Swapped = swap16(r1);
            quint16 r2Swapped = swap16(r2);
            // 正确处理有符号数：先转int16_t，再扩展到int32_t
            int16_t r1Signed = static_cast<int16_t>(r1Swapped);
            int16_t r2Signed = static_cast<int16_t>(r2Swapped);
            // 如果泄露单位是Pa，就除以10，否则就直接除1000
            LeakUnit leakUnitEnum = LeakUnitHelper::fromInt(static_cast<int>(leakUnit));
            if (leakUnitEnum == LeakUnit::Pa) {
                leak = static_cast<double>(static_cast<int32_t>(r1Signed) - static_cast<int32_t>(r2Signed)) / 10.0;
            } else {
                leak = static_cast<double>(static_cast<int32_t>(r1Signed) - static_cast<int32_t>(r2Signed)) / 1000.0;
            }
           
            // 更新当前测试进程面板中的实时泄漏值显示
            if (ui->leakValueLabel) {
                ui->leakValueLabel->setText(QString::number(leak, 'f', 2) + " " + leakUnitStr);
            }
        }

        // 更新测试进程状态
        if (processValueRead) {
            updateTestStatus(processValue);
        }

        // 记录读取失败的参数
        QStringList failedParams;
        if (!deviceStatusRead) failedParams.append("设备状态 (寄存器8706)");
        if (!processValueRead) failedParams.append("测试进程 (寄存器8707)");
        if (!pressureValueRead) failedParams.append("压力值 (寄存器8708)");
        if (!pressureHighValueRead) failedParams.append("压力高位 (寄存器8709)");
        if (!pressureUnitRead) failedParams.append("压力单位 (寄存器8710)");
        if (!leakValueRead) failedParams.append("泄露值 (寄存器8711)");
        if (!leakValue2Read) failedParams.append("泄露值2 (寄存器8712)");
        if (!leakUnitRead) failedParams.append("泄漏单位 (寄存器8713)");
        if (!atmPressureValueRead) failedParams.append("标准大气压 (寄存器8714)");
        if (!atmPressureUnitRead) failedParams.append("大气压单位 (寄存器8715)");
        if (!temperatureValueRead) failedParams.append("温度值 (寄存器8716)");
        if (!temperatureUnitRead) failedParams.append("温度单位 (寄存器8717)");

        if (!failedParams.isEmpty()) {
            LOG_WARNING(QString("以下参数读取失败，保持上次读取的值: %1").arg(failedParams.join(", ")), "实时监控");
        }

        QString processName;
        // 使用正确的枚举限定符
        switch (processValue) {
        case ProcessStatus::STANDBY: processName = "待机"; break;
        case ProcessStatus::FILL: processName = "充气"; break;
        case ProcessStatus::STB: processName = "保压"; break;
        case ProcessStatus::TEST: processName = "测试"; break;
        case ProcessStatus::DUMP: processName = "排气"; break;
        default: processName = QString("未知(%1)").arg(processValue); break;
        }

        // 更新测试进程UI
        updateTestStatus(processValue);

        // 更新折线图数据，添加空指针检查
        if (leakSeries && pressureSeries) {
            leakSeries->append(dataCount, leak);
            pressureSeries->append(dataCount, pressure);
            
            // 移动坐标轴范围，只显示最近50个数据点
            if (axisX && dataCount > 50) {
                axisX->setRange(dataCount - 50, dataCount);
            }

            // 如果折线图对话框已打开，同步追加同一个数据点
            if (chartDialog) {
                chartDialog->appendData(dataCount, pressure, leak);
            }

            dataCount++;
        }

        // 发出实时数据更新信号，供图表等其他组件使用
        emit realtimeDataUpdated(pressure, leak, pressureUnitStr, leakUnitStr, processName);
    });
    } catch (const std::exception& e) {
        LOG_ERROR(QString("处理实时数据时发生异常: %1").arg(e.what()), "实时监控");
    } catch (...) {
        LOG_ERROR("处理实时数据时发生未知异常", "实时监控");
    }
}

// 常量定义：通信重试配置
const int MAX_RETRY_COUNT = 3;       // 最大重试次数
const int READ_TIMEOUT_MS = 3000;    // 读取超时时间（毫秒）
const int RETRY_DELAY_MS = 300;      // 重试前等待时间（毫秒）

void RealTimeMonitor::readDeviceDataAsync(quint16 address, quint16 count, QModbusDataUnit::RegisterType type, const std::function<void(bool, const QModbusDataUnit&)>& callback, int retryCount) {
    // 使用try-catch块捕获所有异常，防止程序崩溃
    try {
        // 参数有效性检查
        if (address == 0 || count == 0) {
            if (callback) {
                callback(false, QModbusDataUnit());
            }
            return;
        }

        // 检查Modbus客户端是否可用
        if (!airTightModbusClient) {
            if (callback) {
                callback(false, QModbusDataUnit());
            }
            return;
        }

        if (airTightModbusClient->state() != QModbusDevice::ConnectedState) {
            if (callback) {
                callback(false, QModbusDataUnit());
            }
            return;
        }

        // 创建读取请求
        QModbusDataUnit readUnit(type, address, count);

        // 发送异步读取请求
        QModbusReply *reply = airTightModbusClient->sendReadRequest(readUnit, static_cast<quint8>(airTightSlaveId));
        if (!reply) {
            // 发送请求失败，尝试重试
            if (retryCount < MAX_RETRY_COUNT) {
                LOG_WARNING(QString("发送读取请求失败，尝试重试 (%d/%d): %1, 地址: %2").arg(retryCount + 1).arg(MAX_RETRY_COUNT).arg(airTightModbusClient->errorString()).arg(address), "实时监控");
                QTimer::singleShot(RETRY_DELAY_MS, this, [this, address, count, type, callback, retryCount]() {
                    readDeviceDataAsync(address, count, type, callback, retryCount + 1);
                });
            } else {
                LOG_ERROR(QString("发送读取请求失败，已达到最大重试次数(%d): %1, 地址: %2, 从站: %3").arg(MAX_RETRY_COUNT).arg(airTightModbusClient->errorString()).arg(address).arg(airTightSlaveId), "实时监控");
                // 标记通信异常
                m_isCommunicationError = true;
                emit registerReadError(address);
                if (callback) {
                    callback(false, QModbusDataUnit());
                }
            }
            return;
        }

        // 设置超时计时器
        QTimer *timeoutTimer = new QTimer(this);
        if (!timeoutTimer) {
            LOG_ERROR(QString("创建超时计时器失败, 地址: %1").arg(address), "实时监控");
            reply->deleteLater();
            if (callback) {
                callback(false, QModbusDataUnit());
            }
            return;
        }
        
        timeoutTimer->setSingleShot(true);
        timeoutTimer->setInterval(READ_TIMEOUT_MS); // 延长超时时间到3秒

        // 连接超时信号
        connect(timeoutTimer, &QTimer::timeout, this, [this, reply, address, count, type, callback, timeoutTimer, retryCount]() {
            try {
                LOG_WARNING(QString("读取寄存器超时: 地址 %1").arg(address), "实时监控");
                if (timeoutTimer) {
                    timeoutTimer->deleteLater();
                }
                if (reply) {
                    reply->deleteLater();
                }
                
                // 尝试重试
                if (retryCount < MAX_RETRY_COUNT) {
                    LOG_INFO(QString("读取寄存器超时，尝试重试 (%d/%d): 地址 %1").arg(retryCount + 1).arg(MAX_RETRY_COUNT).arg(address), "实时监控");
                    QTimer::singleShot(RETRY_DELAY_MS, this, [this, address, count, type, callback, retryCount]() {
                        readDeviceDataAsync(address, count, type, callback, retryCount + 1);
                    });
                } else {
                    LOG_ERROR(QString("读取寄存器超时，已达到最大重试次数(%d): 地址 %1").arg(MAX_RETRY_COUNT).arg(address), "实时监控");
                    // 标记通信异常
                    m_isCommunicationError = true;
                    emit registerReadError(address);
                    if (callback) {
                        callback(false, QModbusDataUnit());
                    }
                }
            } catch (const std::exception& e) {
                LOG_ERROR(QString("处理读取超时事件时发生异常: %1, 地址: %2").arg(e.what()).arg(address), "实时监控");
                if (timeoutTimer) {
                    timeoutTimer->deleteLater();
                }
                if (reply) {
                    reply->deleteLater();
                }
                if (callback) {
                    callback(false, QModbusDataUnit());
                }
            } catch (...) {
                LOG_ERROR(QString("处理读取超时事件时发生未知异常, 地址: %1").arg(address), "实时监控");
                if (timeoutTimer) {
                    timeoutTimer->deleteLater();
                }
                if (reply) {
                    reply->deleteLater();
                }
                if (callback) {
                    callback(false, QModbusDataUnit());
                }
            }
        });

        // 连接完成信号
        connect(reply, &QModbusReply::finished, this, [this, reply, address, count, type, callback, timeoutTimer, retryCount]() {
            try {
                if (timeoutTimer) {
                    timeoutTimer->stop();
                    timeoutTimer->deleteLater();
                }

                if (reply->error() == QModbusDevice::NoError) {
                    // 重置通信错误状态
                    m_isCommunicationError = false;
                    if (callback) {
                        callback(true, reply->result());
                    }
                } else {
                    LOG_ERROR(QString("读取寄存器失败: %1, 地址: %2").arg(reply->errorString()).arg(address), "实时监控");
                    
                    // 尝试重试
                    if (reply) {
                        reply->deleteLater();
                    }
                    
                    if (retryCount < MAX_RETRY_COUNT) {
                        LOG_INFO(QString("读取寄存器失败，尝试重试 (%d/%d): 地址 %1").arg(retryCount + 1).arg(MAX_RETRY_COUNT).arg(address), "实时监控");
                        QTimer::singleShot(RETRY_DELAY_MS, this, [this, address, count, type, callback, retryCount]() {
                            readDeviceDataAsync(address, count, type, callback, retryCount + 1);
                        });
                    } else {
                        LOG_ERROR(QString("读取寄存器失败，已达到最大重试次数(%d): 地址 %1").arg(MAX_RETRY_COUNT).arg(address), "实时监控");
                        // 标记通信异常
                        m_isCommunicationError = true;
                        emit registerReadError(address);
                        if (callback) {
                            callback(false, QModbusDataUnit());
                        }
                    }
                    return;
                }

                if (reply) {
                    reply->deleteLater();
                }
            } catch (const std::exception& e) {
                LOG_ERROR(QString("处理读取完成事件时发生异常: %1, 地址: %2").arg(e.what()).arg(address), "实时监控");
                if (timeoutTimer) {
                    timeoutTimer->deleteLater();
                }
                if (reply) {
                    reply->deleteLater();
                }
                if (callback) {
                    callback(false, QModbusDataUnit());
                }
            } catch (...) {
                LOG_ERROR(QString("处理读取完成事件时发生未知异常, 地址: %1").arg(address), "实时监控");
                if (timeoutTimer) {
                    timeoutTimer->deleteLater();
                }
                if (reply) {
                    reply->deleteLater();
                }
            }
        });

        // 启动超时计时器
        timeoutTimer->start();
    } catch (const std::exception& e) {
        LOG_ERROR(QString("异步读取设备数据时发生异常: %1, 地址: %2").arg(e.what()).arg(address), "实时监控");
        if (callback) {
            callback(false, QModbusDataUnit());
        }
    } catch (...) {
        LOG_ERROR(QString("异步读取设备数据时发生未知异常, 地址: %1").arg(address), "实时监控");
        if (callback) {
            callback(false, QModbusDataUnit());
        }
    }
}

void RealTimeMonitor::processTestResultData(const QModbusDataUnit &data){
// 使用try-catch块捕获所有异常，防止程序崩溃
    try {
        // 使用主线程更新UI
        QMetaObject::invokeMethod(this, [this, data]() {
        if (data.valueCount() >= 13) {
            // 索引2对应地址9090:报警信息
            quint16 register9090Value = data.value(2);
            QString alarmCode = QString::number(register9090Value);
            
            LOG_INFO(QString("处理测试结果数据，寄存器9090值=%1").arg(register9090Value), "实时监控");
            
            // 压力值和压力单位
            // 9091/9092：Pressure值（低位9091，高位9092）- 16位字节交换；根据高位是否为0选择缩放因子
            auto swap16 = [](quint16 v) -> quint16 { return ((v & 0x00FF) << 8) | ((v & 0xFF00) >> 8); }; 
            quint16 lowCode = data.value(3);
            quint16 highCode = data.value(4);
            quint16 lowRaw = swap16(lowCode);
            quint16 highRaw = swap16(highCode);
            
            // 合并32位值并正确处理有符号数
            qint32 rawValue = 0;
            quint32 combined = (static_cast<quint32>(highRaw) << 16) | static_cast<quint32>(lowRaw);
            
            // 正确转换为有符号32位整数
            if (combined & 0x80000000) {
                rawValue = static_cast<qint32>(combined - 0x100000000);
            } else {
                rawValue = static_cast<qint32>(combined);
            }
            
            double pressureValue = static_cast<double>(rawValue) / 1000.0;
            
            // 添加数据有效性验证：压力值应该在合理范围内（-1000 到 1000 KPa 之间）
            bool isValidPressure = (pressureValue >= -1000.0) && (pressureValue <= 1000.0);
            if (!isValidPressure) {
                LOG_WARNING(QString("测试结果压力值超出合理范围: %1 KPa，原始数据: low=0x%2, high=0x%3")
                    .arg(pressureValue)
                    .arg(lowRaw, 4, 16, QChar('0'))
                    .arg(highRaw, 4, 16, QChar('0')), "实时监控");
            }
            // 9093：Pressure单位
            quint16 pressureUnitCode = data.value(5);
            PressureUnit pressureUnitEnum = PressureUnitHelper::fromInt(static_cast<int>(pressureUnitCode));
            QString pressureUnit = PressureUnitHelper::toString(pressureUnitEnum);
            
            LOG_INFO(QString("测试结果压力值=%1 %2").arg(pressureValue).arg(pressureUnit), "实时监控");
             // 9096：Leak单位
            quint16 leakUnitCode = data.value(8); 
            LeakUnit leakUnitEnum = LeakUnitHelper::fromInt(static_cast<int>(leakUnitCode));
            QString leakUnit = LeakUnitHelper::toString(leakUnitEnum);
            
            // 泄露值和泄露单位
            quint16 r1Code = data.value(6); // 9094
            quint16 r2Code = data.value(7); // 9095
            quint16 r1Swapped = swap16(r1Code);
            quint16 r2Swapped = swap16(r2Code);
            // 正确处理有符号数：先转int16_t，再扩展到int32_t
            int16_t r1Signed = static_cast<int16_t>(r1Swapped);
            int16_t r2Signed = static_cast<int16_t>(r2Swapped);
            double leakValue = 0.0;
            // 如果泄露单位是Pa，就除以10，否则就直接除1000
            if (leakUnitEnum == LeakUnit::Pa) {
                leakValue = static_cast<double>(static_cast<int32_t>(r1Signed) - static_cast<int32_t>(r2Signed)) / 10.0;
            } else {
                leakValue = static_cast<double>(static_cast<int32_t>(r1Signed) - static_cast<int32_t>(r2Signed)) / 1000.0;
            }
           
     
            // 9097:标准大气压
            quint16 atmPressureCode = data.value(9);
            double atmPressure = 1006.0746484375 + atmPressureCode / 25600.0;

            QString atmPressureUnit = "hPa";
            // 9099:温度
            quint16 temperatureCode = data.value(11);
            double temperature = 31.4853515625 + temperatureCode / 25600.0;
            QString temperatureUnit = "°C";
            
            // 更新本次测试结果面板中的压力值和泄漏值，添加空指针检查
            QString pressureText = QString("%1 %2").arg(QString::number(pressureValue, 'f', 2)).arg(pressureUnit);
            QString leakText = QString("%1 %2").arg(QString::number(leakValue, 'f', 2)).arg(leakUnit);
            
            // 测试结果
            QString testResult;
            QString resultStyle;
            bool success = false;
            
            if (register9090Value == 0) {
                // 值为0，表示测试通过
                testResult = "通过";
                resultStyle = "color: #00e676; border: 1px solid #00e676;";
                success = true;
            } else{
                // 值不为0，均测试不通过
                testResult = "不通过";
                resultStyle = "color: #ff5252; border: 1px solid #ff5252;";
                success = false;
            } 
            
            // 发送测试结果信号给mainControlSetting.cpp（包含当前测试通道）
            LOG_INFO(QString("准备发送测试结果信号，channel=%1, success=%2").arg(m_currentTestingChannel).arg(success ? "true" : "false"), "实时监控");
            emit updateTestResult(m_currentTestingChannel, success);
            LOG_INFO("测试结果信号已发送", "实时监控");
            
            // 根据当前测试通道更新三次测试结果中的对应位置
            if (m_currentTestingChannel > 0) {
                updateChannelTestResult(m_currentTestingChannel, pressureText, leakText, testResult);
            } else {
                // 如果没有设置当前测试通道，使用默认显示
                if (ui->resultValue5) {
                    ui->resultValue5->setText(pressureText);
                    LOG_INFO(QString("更新测试结果压力值: %1").arg(pressureText), "实时监控");
                }
                if (ui->resultValue6) {
                    ui->resultValue6->setText(leakText);
                    LOG_INFO(QString("更新测试结果泄漏值: %1").arg(leakText), "实时监控");
                }
            }
            
            
            
            // 将测试结果保存到数据库
            QMap<QString, QVariant> testResultData;
            // 从UI获取产品编号
            QString productId = this->productId;
            if (ui->productIdLineEdit) {
                productId = ui->productIdLineEdit->text();
            }
            // 使用当前登录的操作员信息，如果为空则使用"未知"
            QString operatorName = currentOperatorName.isEmpty() ? "未知" : currentOperatorName;
            // 已移除operatorInfo控件
            // if (ui->operatorInfo) {
            //     QString operatorText = ui->operatorInfo->text();
            //     if (operatorText.contains("当前操作人员：")) {
            //         // 解析操作人员信息，格式："当前操作人员：张三 角色：管理员"
            //         int startPos = operatorText.indexOf("当前操作人员：") + QString("当前操作人员：").length();
            //         int endPos = operatorText.indexOf(" 角色：");
            //         if (endPos > startPos) {
            //             operatorName = operatorText.mid(startPos, endPos - startPos);
            //         } else {
            //             // 如果没有找到" 角色："，则取从startPos到末尾的所有内容
            //             operatorName = operatorText.mid(startPos);
            //         }
            //     }
            // }
            testResultData["serial_number"] = this->programNumber;
            testResultData["channel"] = m_currentTestingChannel; // 添加通道号
            testResultData["product_id"] = productId;
            testResultData["operator_name"] = operatorName;
            testResultData["test_result"] = testResult;
            testResultData["alarm_code"] = alarmCode;
            testResultData["pressure_value"] = pressureValue;
            testResultData["pressure_unit"] = pressureUnit;
            testResultData["leak_value"] = leakValue;
            testResultData["leak_unit"] = leakUnit;
            testResultData["standard_atm"] = atmPressure;
            testResultData["standard_atm_unit"] = atmPressureUnit;
            testResultData["temperature"] = temperature;
            testResultData["temperature_unit"] = temperatureUnit;
            testResultData["test_time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
            
            // 调用DatabaseManager保存测试结果，添加异常处理
            try {
                DatabaseManager* dbManager = DatabaseManager::getInstance();
                if (dbManager && !dbManager->addTestResult(testResultData)) {
                    LOG_ERROR(QString("保存测试结果到数据库失败：%1").arg(dbManager->getLastError()), "实时监控");
                } else if (dbManager) {
                    LOG_INFO("测试结果已成功保存到数据库", "实时监控");
                    // 保存成功后更新测试结果汇总统计
                    updateTestResultSummary();
                    // 发出信号通知TestResultshow自动刷新
                    emit testResultSaved(testResultData);
                }
            } catch (const std::exception& e) {
                LOG_ERROR(QString("保存测试结果到数据库时发生异常：%1").arg(e.what()), "实时监控");
            } catch (...) {
                LOG_ERROR("保存测试结果到数据库时发生未知异常", "实时监控");
            }
        } else {
            LOG_ERROR(QString("读取测试结果数据失败：返回值数量不足，只收到 %1 个值").arg(data.valueCount()), "实时监控");
        }
    }, Qt::QueuedConnection);
    } catch (const std::exception& e) {
        LOG_ERROR(QString("处理测试结果数据时发生异常: %1").arg(e.what()), "实时监控");
    } catch (...) {
        LOG_ERROR("处理测试结果数据时发生未知异常", "实时监控");
    }
}

void RealTimeMonitor::updateTestStatus(int processValue)
{
    // 使用try-catch块捕获所有异常，防止程序崩溃
    try {
        QString processName;
        QString frameStyle;
        QString labelStyle;
        QString valueStyle;
        int progress = 0; // 进度条值，范围0-100

        // 测试阶段时间参数（从UI获取，这些值是从数据库读取的预设值）
        // 添加空指针检查，防止UI控件未初始化导致崩溃
        int fillTime = 0;
        int stabilizationTime = 0;
        int testTime = 0;
        int dumpTime = 0;
        
        if (ui->paramValue1) {
            fillTime = ui->paramValue1->text().toInt(); // 填充时间
        }
        if (ui->paramValue2) {
            stabilizationTime = ui->paramValue2->text().toInt(); // 稳定时间
        }
        if (ui->paramValue3) {
            testTime = ui->paramValue3->text().toInt(); // 测试时间
        }
        if (ui->paramValue4) {
            dumpTime = ui->paramValue4->text().toInt(); // 排气时间
        }
    
    // 记录当前阶段开始时间的静态变量
    static QDateTime stageStartTime;
    static ProcessStatus currentStage = ProcessStatus::STANDBY;
    static bool mainBoardCommandSent = false; // 标记是否已发送主控板命令
    static bool lastWasTesting = false; // 标记上一个状态是否处于测试中
    
    // 【关键修复：检测测试阶段异常复位】
    // 如果之前是测试阶段（非待机），现在突然变成待机（0），说明发生了异常复位
    bool isCurrentlyTesting = (currentStage != ProcessStatus::STANDBY);
    bool isNowStandby = (processValue == ProcessStatus::STANDBY);
    
    if (lastWasTesting && isNowStandby) {
        LOG_WARNING("【异常检测】测试阶段异常复位！从测试状态突然跳回待机状态", "实时监控");
        // 此处可以添加重发启动命令的逻辑
        emit reSendStartCommand(m_currentTestingChannel);
    }
    lastWasTesting = isCurrentlyTesting;
    
    // 如果阶段发生变化，更新阶段开始时间并发送主控板命令
    if (currentStage != processValue) {
        ProcessStatus previousStage = currentStage;
        currentStage = static_cast<ProcessStatus>(processValue);
        stageStartTime = QDateTime::currentDateTime();
        mainBoardCommandSent = false; // 重置命令发送标记
        
        LOG_INFO(QString("测试阶段变化: %1 -> %2").arg(previousStage).arg(processValue), "实时监控");
    }

    // 根据processValue设置不同的状态名称、样式和进度
    switch (processValue) {
    case ProcessStatus::STANDBY: {
        processName = "待机";
        progress = 0;
        frameStyle = "background-color: #1976d2;\n"
                     "border: 2px solid #1565c0;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #e3f2fd;\n"
                     "font-size: 12px;";
        // 当处于待机状态时，重置测试结果为初始状态
        bool tempMonitoringState = isMonitoring;
        isMonitoring = false;
        isMonitoring = tempMonitoringState;
        break;
    }
    case ProcessStatus::FILL:
        processName = "充气";
        // 计算充气阶段的实时进度
        if (fillTime > 0) {
            qint64 elapsed = stageStartTime.secsTo(QDateTime::currentDateTime());
            int stageProgress = qMin(elapsed * 100 / fillTime, 100LL);
            progress = stageProgress * 25 / 100; // 充气阶段占总进度的25%
        } else {
            progress = 25;
        }
        frameStyle = "background-color: #1976d2;\n"
                     "border: 2px solid #1565c0;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #e3f2fd;\n"
                     "font-size: 12px;";
        break;
    case ProcessStatus::STB:
        processName = "保压";
        // 计算保压阶段的实时进度
        if (stabilizationTime > 0) {
            qint64 elapsed = stageStartTime.secsTo(QDateTime::currentDateTime());
            int stageProgress = qMin(elapsed * 100 / stabilizationTime, 100LL);
            progress = 25 + (stageProgress * 25 / 100); // 保压阶段占总进度的25%
        } else {
            progress = 50;
        }
        frameStyle = "background-color: #f57c00;\n"
                     "border: 2px solid #ef6c00;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #fff3e0;\n"
                     "font-size: 12px;";
        break;
    case ProcessStatus::TEST:
        processName = "测试";
        // 计算测试阶段的实时进度
        if (testTime > 0) {
            qint64 elapsed = stageStartTime.secsTo(QDateTime::currentDateTime());
            int stageProgress = qMin(elapsed * 100 / testTime, 100LL);
            progress = 50 + (stageProgress * 25 / 100); // 测试阶段占总进度的25%
        } else {
            progress = 75;
        }
        frameStyle = "background-color: #0097a7;\n"
                     "border: 2px solid #00838f;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #e0f7fa;\n"
                     "font-size: 12px;";
        break;
    case ProcessStatus::DUMP:
        processName = "排气";
        // 计算排气阶段的实时进度
        if (dumpTime > 0) {
            qint64 elapsed = stageStartTime.secsTo(QDateTime::currentDateTime());
            int stageProgress = qMin(elapsed * 100 / dumpTime, 100LL);
            progress = 75 + (stageProgress * 25 / 100); // 排气阶段占总进度的25%
        } else {
            progress = 100;
        }
        frameStyle = "background-color: #546e7a;\n"
                     "border: 2px solid #455a64;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #eceff1;\n"
                     "font-size: 12px;";
        break;
    default:
        processName = QString("未知(%1)").arg(processValue);
        progress = 0;
        frameStyle = "background-color: #9e9e9e;\n"
                     "border: 2px solid #757575;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #f5f5f5;\n"
                     "font-size: 12px;";
        break;
    }

    // 更新进度条
    if (ui->testProgressBar) {
        // 将进度条最大值设置为100，以便显示百分比
        ui->testProgressBar->setMaximum(100);
        ui->testProgressBar->setValue(progress);
        ui->testProgressBar->setVisible(true);
        
        // 根据不同阶段设置进度条样式
        if (progress == 0) {
            ui->testProgressBar->setStyleSheet("QProgressBar { background-color: #e0e0e0; border: 1px solid #bdbdbd; border-radius: 4px; } QProgressBar::chunk { background-color: #9e9e9e; }");
        } else if (progress < 100) {
            ui->testProgressBar->setStyleSheet("QProgressBar { background-color: #e3f2fd; border: 1px solid #1976d2; border-radius: 4px; } QProgressBar::chunk { background-color: #1976d2; }");
        } else {
            ui->testProgressBar->setStyleSheet("QProgressBar { background-color: #e8f5e8; border: 1px solid #43a047; border-radius: 4px; } QProgressBar::chunk { background-color: #43a047; }");
            LOG_INFO("测试已完成，进度达到100%", "实时监控");
        }
    }

    // 更新当前状态文本显示（根据寄存器8707的值更新）
    if (ui->statusText) {
        ui->statusText->setText(QString("🔄 当前状态: %1").arg(processName));
    }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("更新测试状态时发生异常: %1").arg(e.what()), "实时监控");
    } catch (...) {
        LOG_ERROR("更新测试状态时发生未知异常", "实时监控");
    }
}


void RealTimeMonitor::updateTestResultSummary()
{
    try {
        // 从数据库获取测试结果统计
        DatabaseManager* dbManager = DatabaseManager::getInstance();
        if (!dbManager) {
            LOG_ERROR("无法获取数据库管理器实例", "实时监控");
            return;
        }
        
        QMap<QString, int> stats = dbManager->getTestResultStatistics();
        
        int total = stats["total"];
        int pass = stats["pass"];
        int fail = stats["fail"];
        
        // 计算合格率
        double passRate = 0.0;
        if (total > 0) {
            passRate = (static_cast<double>(pass) / total) * 100.0;
        }
        
        // 更新UI（使用主线程）
        QMetaObject::invokeMethod(this, [this, total, pass, fail, passRate]() {
            // 更新测试总数
            if (ui->resultValue1) {
                ui->resultValue1->setText(QString::number(total));
            }
            // 更新合格数 - 绿色显示
            if (ui->resultValue2) {
                ui->resultValue2->setText(QString::number(pass));
                ui->resultValue2->setStyleSheet("color: #00e676; text-shadow: 0 0 8px rgba(0, 230, 118, 0.6);");
            }
            // 更新不合格数 - 红色显示
            if (ui->resultValue3) {
                ui->resultValue3->setText(QString::number(fail));
                ui->resultValue3->setStyleSheet("color: #ff5252; text-shadow: 0 0 8px rgba(255, 82, 82, 0.6);");
            }
            // 更新合格率 - 绿色显示
            if (ui->resultValue4) {
                ui->resultValue4->setText(QString("%1%").arg(QString::number(passRate, 'f', 1)));
                ui->resultValue4->setStyleSheet("color: #00e676; text-shadow: 0 0 8px rgba(0, 230, 118, 0.6);");
            }
            
            LOG_DEBUG(QString("更新测试结果汇总：总数=%1, 合格=%2, 不合格=%3, 合格率=%4%")
                     .arg(total).arg(pass).arg(fail).arg(QString::number(passRate, 'f', 1)), "实时监控");
        }, Qt::QueuedConnection);
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("更新测试结果汇总时发生异常: %1").arg(e.what()), "实时监控");
    } catch (...) {
        LOG_ERROR("更新测试结果汇总时发生未知异常", "实时监控");
    }
}

// 向主控板发送Modbus命令
bool RealTimeMonitor::sendMainBoardCommand(quint16 address, quint16 value, int timeoutMs)
{
    // 检查主控板Modbus客户端是否可用
    if (!mainBoardModbusClient) {
        LOG_DEBUG("主控板Modbus客户端未设置，跳过发送命令", "实时监控");
        return false;
    }
    
    if (mainBoardModbusClient->state() != QModbusDevice::ConnectedState) {
        LOG_DEBUG("主控板未连接，跳过发送命令", "实时监控");
        return false;
    }
    
    // 创建写入数据单元
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, 1);
    writeUnit.setValue(0, value);
    
    if (auto *reply = mainBoardModbusClient->sendWriteRequest(writeUnit, mainBoardSlaveId)) {
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
                LOG_DEBUG(QString("成功向主控板写入寄存器 0x%1 = %2").arg(address, 4, 16, QChar('0')).arg(value), "实时监控");
                reply->deleteLater();
                return true;
            } else {
                LOG_WARNING(QString("向主控板写入寄存器失败: %1").arg(reply->errorString()), "实时监控");
                reply->deleteLater();
                return false;
            }
        } else {
            LOG_WARNING(QString("向主控板写入寄存器超时: 地址=0x%1").arg(address, 4, 16, QChar('0')), "实时监控");
            reply->deleteLater();
            return false;
        }
    } else {
        LOG_WARNING("sendWriteRequest返回nullptr，无法向主控板发送命令", "实时监控");
        return false;
    }
}

// 按钮点击处理槽函数
void RealTimeMonitor::onExpandChartButtonClicked()
{
    // 如果对话框已经打开，直接激活到前台
    if (chartDialog) {
        chartDialog->raise();
        chartDialog->activateWindow();
        return;
    }

    chartDialog = new ChartDialog(this);

    // 用主窗口当前的历史数据初始化对话框
    chartDialog->initWithHistory(
        pressureSeries->points(),
        leakSeries->points(),
        axisX->min(), axisX->max(),
        axisYLeft->min(), axisYLeft->max(),
        axisYRight->min(), axisYRight->max()
    );

    connect(chartDialog, &QDialog::finished, this, &RealTimeMonitor::onChartDialogClosed);

    chartDialog->show();
    chartDialog->raise();
    chartDialog->activateWindow();
}

// 对话框关闭处理槽函数
void RealTimeMonitor::onChartDialogClosed()
{
    if (chartDialog) {
        chartDialog->deleteLater();
        chartDialog = nullptr;
    }
    
    // 使用定时器确保对话框完全关闭后再设置焦点
    QTimer::singleShot(50, this, [this]() {
        // 确保主窗口被激活
        QWidget *mainWindow = this->window();
        if (mainWindow) {
            mainWindow->activateWindow();
            mainWindow->raise();
        }
        
        // 处理所有待处理的事件
        QApplication::processEvents();
        
        // 设置焦点到产品编号输入框
        if (ui->productIdLineEdit) {
            ui->productIdLineEdit->setFocus(Qt::TabFocusReason);
            ui->productIdLineEdit->selectAll();
            LOG_INFO("全屏图表对话框已关闭，已重新聚焦到产品编号输入框", "实时监控");
        }
    });
}

// 设置当前测试通道
void RealTimeMonitor::setCurrentTestingChannel(int channel)
{
    m_currentTestingChannel = channel;
    LOG_INFO(QString("当前测试通道已设置为: %1").arg(channel), "实时监控");
    emit testChannelChanged(channel);
}

// 设置主控设置页面指针
void RealTimeMonitor::setMainControlSetting(MainControlSetting *mainControlSetting)
{
    m_mainControlSetting = mainControlSetting;
    LOG_INFO("主控设置页面指针已设置", "实时监控");
}

// 更新指定通道的测试结果
void RealTimeMonitor::updateChannelTestResult(int channel, const QString& pressureValue, const QString& leakValue, const QString& result)
{
    // 检查通道是否开启
    bool channelEnabled = true;
    if (m_mainControlSetting) {
        channelEnabled = m_mainControlSetting->isChannelEnabled(channel);
    }
    
    if (!channelEnabled) {
        LOG_INFO(QString("通道%1未开启，跳过更新测试结果").arg(channel), "实时监控");
        return;
    }
    
    QMetaObject::invokeMethod(this, [this, channel, pressureValue, leakValue, result]() {
        QString resultStyle;
        if (result == "通过") {
            resultStyle = "font-size: 13px; font-weight: 600; color: #00e676; padding: 2px 6px; border-radius: 3px; background: rgba(0, 230, 118, 0.1); border: 1px solid #00e676;";
        } else {
            resultStyle = "font-size: 13px; font-weight: 600; color: #ff5252; padding: 2px 6px; border-radius: 3px; background: rgba(255, 82, 82, 0.1); border: 1px solid #ff5252;";
        }
        
        switch (channel) {
        case 1:
            if (ui->resultValue5) {
                ui->resultValue5->setText(pressureValue);
            }
            if (ui->resultValue6) {
                ui->resultValue6->setText(leakValue);
            }
            if (ui->resultValue7) {
                ui->resultValue7->setText(result);
                ui->resultValue7->setStyleSheet(resultStyle);
            }
            LOG_INFO(QString("更新测试1结果: 压力=%1, 泄漏=%2, 结果=%3").arg(pressureValue).arg(leakValue).arg(result), "实时监控");
            break;
        case 2:
            if (ui->resultValue8) {
                ui->resultValue8->setText(pressureValue);
            }
            if (ui->resultValue9) {
                ui->resultValue9->setText(leakValue);
            }
            if (ui->resultValue10) {
                ui->resultValue10->setText(result);
                ui->resultValue10->setStyleSheet(resultStyle);
            }
            LOG_INFO(QString("更新测试2结果: 压力=%1, 泄漏=%2, 结果=%3").arg(pressureValue).arg(leakValue).arg(result), "实时监控");
            break;
        case 3:
            if (ui->resultValue11) {
                ui->resultValue11->setText(pressureValue);
            }
            if (ui->resultValue12) {
                ui->resultValue12->setText(leakValue);
            }
            if (ui->resultValue13) {
                ui->resultValue13->setText(result);
                ui->resultValue13->setStyleSheet(resultStyle);
            }
            LOG_INFO(QString("更新测试3结果: 压力=%1, 泄漏=%2, 结果=%3").arg(pressureValue).arg(leakValue).arg(result), "实时监控");
            break;
        default:
            LOG_WARNING(QString("无效的通道号: %1").arg(channel), "实时监控");
            break;
        }
        
        // 记录通道测试结果
        channelTestResults[channel] = (result == "通过");
        
        // 计算并更新总测试结果
        calculateAndUpdateTotalResult();
    }, Qt::QueuedConnection);
}

// 计算并更新总测试结果
void RealTimeMonitor::calculateAndUpdateTotalResult()
{
    // 获取开启的通道列表
    QList<int> enabledChannels;
    if (m_mainControlSetting) {
        enabledChannels = m_mainControlSetting->getEnabledChannels();
    } else {
        // 如果没有设置主控设置页面，默认认为所有通道都开启
        enabledChannels = {1, 2, 3};
    }
    
    // 如果没有开启任何通道
    if (enabledChannels.isEmpty()) {
        if (ui->resultValue14) {
            ui->resultValue14->setText("--");
            ui->resultValue14->setStyleSheet("font-size: 16px; font-weight: 700; color: #9e9e9e; padding: 3px 10px; border-radius: 4px; background: rgba(158, 158, 158, 0.15); border: 2px solid #9e9e9e;");
        }
        LOG_INFO("没有开启任何测试通道，总测试结果设为--", "实时监控");
        return;
    }
    
    // 检查开启通道的测试结果
    bool allPassed = true;
    bool allTested = true;
    bool hasAnyResult = false;
    
    QString resultInfo = "开启通道:";
    for (int channel : enabledChannels) {
        resultInfo += QString(" %1").arg(channel);
        bool passed = channelTestResults.value(channel, false);
        bool tested = channelTestResults.contains(channel);
        
        if (tested) {
            hasAnyResult = true;
            if (!passed) {
                allPassed = false;
            }
        } else {
            allTested = false;
        }
    }
    
    QString totalResult;
    QString totalResultStyle;
    bool finalResultDetermined = false;
    bool isQualified = false;
    
    if (!hasAnyResult) {
        // 还没有任何测试结果
        totalResult = "--";
        totalResultStyle = "font-size: 16px; font-weight: 700; color: #9e9e9e; padding: 3px 10px; border-radius: 4px; background: rgba(158, 158, 158, 0.15); border: 2px solid #9e9e9e;";
    } else if (allPassed && allTested) {
        // 所有开启的通道都已测试并通过
        totalResult = "合格";
        totalResultStyle = "font-size: 16px; font-weight: 700; color: #00e676; padding: 3px 10px; border-radius: 4px; background: rgba(0, 230, 118, 0.15); border: 2px solid #00e676;";
        finalResultDetermined = true;
        isQualified = true;
    } else if (allTested && !allPassed) {
        // 所有开启的通道都已测试，但至少有一个未通过
        totalResult = "不合格";
        totalResultStyle = "font-size: 16px; font-weight: 700; color: #ff5252; padding: 3px 10px; border-radius: 4px; background: rgba(255, 82, 82, 0.15); border: 2px solid #ff5252;";
        finalResultDetermined = true;
        isQualified = false;
    } else {
        // 部分开启的通道尚未测试
        totalResult = "测试中";
        totalResultStyle = "font-size: 16px; font-weight: 700; color: #ff9800; padding: 3px 10px; border-radius: 4px; background: rgba(255, 152, 0, 0.15); border: 2px solid #ff9800;";
    }
    
    // 更新UI显示
    if (ui->resultValue14) {
        ui->resultValue14->setText(totalResult);
        ui->resultValue14->setStyleSheet(totalResultStyle);
    }
    
    // 如果最终结果已确定（合格或不合格），向主控板发送命令
    if (finalResultDetermined && !m_resultSent) {
        m_resultSent = true;
        
        if (m_mainControlSetting) {
            if (isQualified) {
                // 合格：0004写1, 0005写0, 0006写1
                LOG_INFO("总测试结果为合格，向主控板发送命令: 0004=1, 0005=0, 0006=1", "实时监控");
                m_mainControlSetting->sendModbusCommand(0x0004, 0x0001, 500);
                m_mainControlSetting->sendModbusCommand(0x0005, 0x0000, 500);
                m_mainControlSetting->sendModbusCommand(0x0006, 0x0001, 500);
            } else {
                // 不合格：0004写0, 0005写1, 0006写1
                LOG_INFO("总测试结果为不合格，向主控板发送命令: 0004=0, 0005=1, 0006=1", "实时监控");
                m_mainControlSetting->sendModbusCommand(0x0004, 0x0000, 500);
                m_mainControlSetting->sendModbusCommand(0x0005, 0x0001, 500);
                m_mainControlSetting->sendModbusCommand(0x0006, 0x0001, 500);
            }
            
            // 发送完主控板命令后，清空当前编号
            updateCurrentProductId("");
            LOG_INFO("测试完成，已清空当前产品编号", "实时监控");
        } else {
            LOG_WARNING("m_mainControlSetting未设置，无法向主控板发送测试结果命令", "实时监控");
        }
    }
    
    // 重置结果发送标记（当有新的测试结果时）
    if (!finalResultDetermined) {
        m_resultSent = false;
    }
    
    LOG_INFO(QString("总测试结果计算完成: %1, 全部通过=%2, 全部已测试=%3, 总结果=%4")
             .arg(resultInfo)
             .arg(allPassed ? "是" : "否")
             .arg(allTested ? "是" : "否")
             .arg(totalResult), "实时监控");
}

// 检查是否允许启动测试（检查必须扫码设置和产品编号）
bool RealTimeMonitor::canStartTest() const
{
    // 如果设置了必须扫码，但产品编号为空，则不允许启动测试
    if (scanRequired && productId.isEmpty()) {
        LOG_WARNING("必须扫码但产品编号为空，无法启动测试", "实时监控");
        return false;
    }
    return true;
}

// 获取当前产品编号
QString RealTimeMonitor::getProductId() const
{
    return productId;
}

// 获取是否必须扫码
bool RealTimeMonitor::isScanRequired() const
{
    return scanRequired;
}

// 停止数据读取定时器（用于通道切换时避免Modbus通信冲突）
void RealTimeMonitor::stopDataTimers()
{
    if (dataTimer && dataTimer->isActive()) {
        dataTimer->stop();
        LOG_DEBUG("数据读取定时器已停止", "实时监控");
    }
    if (testResultTimer && testResultTimer->isActive()) {
        testResultTimer->stop();
        LOG_DEBUG("测试结果读取定时器已停止", "实时监控");
    }
    if (m_readDataTimeoutTimer && m_readDataTimeoutTimer->isActive()) {
        m_readDataTimeoutTimer->stop();
        LOG_DEBUG("读取数据超时保护定时器已停止", "实时监控");
    }
    
    // 重置读取状态标志，防止通道切换时卡住
    if (m_isReadingData) {
        LOG_INFO("通道切换：重置m_isReadingData标志", "实时监控");
        m_isReadingData = false;
    }
}

// 启动数据读取定时器（用于通道切换后恢复监控）
void RealTimeMonitor::startDataTimers()
{
    if (dataTimer && !dataTimer->isActive()) {
        dataTimer->start(2000); // 修改为2秒间隔，减少Modbus通信压力
        LOG_DEBUG("数据读取定时器已启动", "实时监控");
    }
    if (testResultTimer && !testResultTimer->isActive()) {
        testResultTimer->start(1000); // 1秒间隔读取测试结果
        LOG_DEBUG("测试结果读取定时器已启动", "实时监控");
    }
    
    // 确保m_isReadingData为false
    if (m_isReadingData) {
        LOG_WARNING("启动数据定时器时发现m_isReadingData仍为true，已重置", "实时监控");
        m_isReadingData = false;
    }
}

// 强制检测测试阶段（解决通道2/3阶段无变化问题）
void RealTimeMonitor::forceDetectTestPhase()
{
    if (!airTightModbusClient || airTightModbusClient->state() != QModbusDevice::ConnectedState) {
        LOG_WARNING("气密仪未连接，无法执行强制阶段检测", "实时监控");
        return;
    }
    
    readDeviceDataAsync(9088, 1, QModbusDataUnit::HoldingRegisters,
        [this](bool success, const QModbusDataUnit &data) {
            if (success && data.valueCount() > 0) {
                quint16 phase = data.value(0);
                LOG_DEBUG(QString("强制检测测试阶段，寄存器9088值: %1").arg(phase), "实时监控");
                
                if (phase == 0) {
                    LOG_WARNING(QString("通道%1阶段仍为0，触发重发启动命令信号").arg(m_currentTestingChannel), "实时监控");
                    emit reSendStartCommand(m_currentTestingChannel);
                }
            } else {
                LOG_ERROR("强制检测测试阶段失败", "实时监控");
            }
        });
}

void RealTimeMonitor::resetTestPhaseState()
{
    m_lastReg9088Value = 0;
    LOG_DEBUG("测试阶段状态已重置", "实时监控");
}

// 强制聚焦到产品编号输入框
void RealTimeMonitor::focusProductIdInput()
{
    QTimer::singleShot(50, this, [this]() {
        // 确保主窗口被激活
        QWidget *mainWindow = this->window();
        if (mainWindow) {
            mainWindow->activateWindow();
            mainWindow->raise();
        }
        
        // 处理所有待处理的事件
        QApplication::processEvents();
        
        // 设置焦点到产品编号输入框
        if (ui->productIdLineEdit) {
            ui->productIdLineEdit->setFocus(Qt::TabFocusReason);
            ui->productIdLineEdit->selectAll();
            LOG_INFO("已强制聚焦到产品编号输入框", "实时监控");
        }
    });
}

// 事件过滤器，处理产品编号输入框的焦点事件
bool RealTimeMonitor::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->productIdLineEdit) {
        if (event->type() == QEvent::FocusIn) {
            // 当产品编号输入框获得焦点时，自动选中所有内容
            // 这样扫码枪输入会覆盖原有内容，而不是追加
            ui->productIdLineEdit->selectAll();
            LOG_DEBUG("产品编号输入框获得焦点，已选中所有内容", "实时监控");
        }
    }
    return QWidget::eventFilter(obj, event);
}

// 重写showEvent，确保每次页面显示时自动聚焦到产品编号输入框
void RealTimeMonitor::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    
    // 使用单次延迟确保UI完全渲染后再设置焦点
    QTimer::singleShot(50, this, [this]() {
        // 确保主窗口被激活
        QWidget *mainWindow = this->window();
        if (mainWindow) {
            mainWindow->activateWindow();
            mainWindow->raise();
        }
        
        // 处理所有待处理的事件
        QApplication::processEvents();
        
        // 设置焦点到产品编号输入框
        if (ui->productIdLineEdit && this->isVisible()) {
            ui->productIdLineEdit->setFocus(Qt::TabFocusReason);
            ui->productIdLineEdit->selectAll();
            LOG_INFO("实时监控页面显示，已自动聚焦到产品编号输入框", "实时监控");
        }
    });
}
