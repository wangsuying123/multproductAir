#include "commspage.h"
#include "ui_commspage.h"
#include <QMessageBox>
#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>
#include <QCoreApplication>

CommsPage::CommsPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CommsPage)
{
    // 初始化设备信息，设置默认从站ID为1
    m_devices[AirTightDevice].slaveId = 1;
    m_devices[MainBoardDevice].slaveId = 1;
    m_devices[PressureRegulatorDevice].slaveId = 1;
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
    
    // 刷新串口列表
    refreshSerialPorts(ui->serialPortComboBox);
    refreshSerialPorts(ui->serialPortComboBox_2);
    refreshSerialPorts(ui->serialPortComboBox_3);
    
    // 加载保存的设置
    loadSettings();
    
    // 初始设置协议相关控件的可见性
    updateProtocolSettings(0);
    updateProtocolSettings(1);
    updateProtocolSettings(2);
    
    // 自动连接设备：如果数据库中有参数，就自动连接
    DatabaseManager* dbManager = DatabaseManager::getInstance();
    
    // 初始状态设置为未连接
    ui->airtightStatus->setText("未连接");
    ui->airtightStatus->setStyleSheet("color: #e74c3c; font-weight: 600;");
    ui->mainboardStatus->setText("未连接");
    ui->mainboardStatus->setStyleSheet("color: #e74c3c; font-weight: 600;");
    ui->pressureStatus->setText("未连接");
    ui->pressureStatus->setStyleSheet("color: #e74c3c; font-weight: 600;");
    
    if (dbManager->hasCommunicationParams(AirTightDevice)) {
        connectDevice(0);
    } else {
        m_devices[AirTightDevice].connected = false;
        emit airTightConnectionChanged(false);
    }
    
    if (dbManager->hasCommunicationParams(MainBoardDevice)) {
        connectDevice(1);
    } else {
        m_devices[MainBoardDevice].connected = false;
        emit mainBoardConnectionChanged(false);
    }
    
    if (dbManager->hasCommunicationParams(PressureRegulatorDevice)) {
        connectDevice(2);
    } else {
        m_devices[PressureRegulatorDevice].connected = false;
        emit pressureRegulatorConnectionChanged(false);
    }
    
    // 更新按钮状态
    updateButtonStates();
}

CommsPage::~CommsPage()
{
    qDebug() << "CommsPage析构函数开始执行...";
    
    // 先停止所有自动重连定时器
    for (int i = 0; i < 3; ++i) {
        if (m_devices[i].autoReconnectTimer) {
            m_devices[i].autoReconnectTimer->stop();
            m_devices[i].autoReconnectEnabled = false;
        }
    }
    
    // 断开所有Modbus客户端连接并释放资源
    for (int i = 0; i < 3; ++i) {
        if (m_devices[i].modbusClient) {
            // 断开所有信号连接，防止在析构过程中触发回调
            m_devices[i].modbusClient->disconnect();
            
            // 如果是连接状态，先断开
            if (m_devices[i].modbusClient->state() == QModbusDevice::ConnectedState) {
                m_devices[i].modbusClient->disconnectDevice();
                
                // 等待断开完成，最多等待2秒
                int timeout = 0;
                while (m_devices[i].modbusClient->state() != QModbusDevice::UnconnectedState && timeout < 20) {
                    QThread::msleep(100);
                    QCoreApplication::processEvents();
                    timeout++;
                }
            }
            
            // 删除Modbus客户端，这会释放内部的串口资源
            delete m_devices[i].modbusClient;
            m_devices[i].modbusClient = nullptr;
            qDebug() << "设备" << i << "Modbus客户端已释放";
        }
    }
    
    // 断开TCP连接
    for (int i = 0; i < 3; ++i) {
        if (m_devices[i].tcpSocket) {
            if (m_devices[i].tcpSocket->state() != QAbstractSocket::UnconnectedState) {
                m_devices[i].tcpSocket->disconnectFromHost();
                m_devices[i].tcpSocket->waitForDisconnected(1000);
            }
            delete m_devices[i].tcpSocket;
            m_devices[i].tcpSocket = nullptr;
        }
    }
    
    // 关闭独立的串口（如果有）
    for (int i = 0; i < 3; ++i) {
        if (m_devices[i].serialPort) {
            if (m_devices[i].serialPort->isOpen()) {
                m_devices[i].serialPort->close();
            }
            delete m_devices[i].serialPort;
            m_devices[i].serialPort = nullptr;
        }
    }
    
    // 释放自动重连定时器
    for (int i = 0; i < 3; ++i) {
        if (m_devices[i].autoReconnectTimer) {
            delete m_devices[i].autoReconnectTimer;
            m_devices[i].autoReconnectTimer = nullptr;
        }
    }
    
    // 保存设置
    saveSettings();
    
    // 给系统一点时间释放串口资源
    QThread::msleep(200);
    
    delete ui;
    
    qDebug() << "CommsPage析构函数执行完成";
}

void CommsPage::on_protocol_currentIndexChanged(int /*index*/)
{
    updateProtocolSettings(0);
}

void CommsPage::on_protocol_2_currentIndexChanged(int /*index*/)
{
    updateProtocolSettings(1);
}

void CommsPage::on_protocol_3_currentIndexChanged(int /*index*/)
{
    updateProtocolSettings(2);
}

void CommsPage::on_connectionButton_clicked()
{
    connectDevice(0);
}

void CommsPage::on_connectionButton_2_clicked()
{
    connectDevice(1);
}

void CommsPage::on_connectionButton_3_clicked()
{
    connectDevice(2);
}

void CommsPage::on_disconnectButton_clicked()
{
    disconnectDevice(0);
}

void CommsPage::on_disconnectButton_2_clicked()
{
    disconnectDevice(1);
}

void CommsPage::on_disconnectButton_3_clicked()
{
    disconnectDevice(2);
}

void CommsPage::connectDevice(int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex > 2) {
        return;
    }
    
    // 1. 先彻底断开并释放旧的客户端（关键：避免资源泄漏和重复配置）
    if (m_devices[deviceIndex].modbusClient) {
        QModbusClient *oldClient = m_devices[deviceIndex].modbusClient;
        oldClient->disconnectDevice();
        oldClient->disconnect(this); // 断开所有信号
        delete oldClient;
        m_devices[deviceIndex].modbusClient = nullptr;
    }
    
    QString deviceName;
    QString protocol;
    QString ip;
    QString port;
    QString serialPort;
    QString baudrate;
    int parity;
    int dataBits;
    int stopBits;
    bool connected = false;
    
    switch (deviceIndex) {
    case 0: // 气密仪
        deviceName = "气密仪";
        protocol = ui->protocol->currentText();
        ip = ui->ipLineEdit->text();
        port = ui->portLineEdit->text();
        serialPort = ui->serialPortComboBox->currentText();
        baudrate = ui->baudrateComboBox->currentText();
        parity = ui->parityComboBox->currentIndex();
        dataBits = ui->dataBitsComboBox->currentIndex();
        stopBits = ui->stopBitsComboBox->currentIndex();
        
        if (protocol == "Modbus TCP" || protocol == "以太网") {
            // 检查是否需要创建新的TCP客户端
            QModbusTcpClient *tcpClient = qobject_cast<QModbusTcpClient*>(m_devices[AirTightDevice].modbusClient);
            if (!tcpClient) {
                // 如果客户端存在但类型不匹配，先删除旧客户端
                delete m_devices[AirTightDevice].modbusClient;
                m_devices[AirTightDevice].modbusClient = new QModbusTcpClient(this);
                tcpClient = qobject_cast<QModbusTcpClient*>(m_devices[AirTightDevice].modbusClient);
            }
            if (tcpClient) {
                bool ok;
                int portNum = port.toInt(&ok);
                
                // 端口号有效性检查
                if (!ok || portNum < 1 || portNum > 65535) {
                    QMessageBox::warning(this, "参数错误", QString("[%1] 端口号无效，请输入1-65535之间的整数").arg(deviceName));
                    return;
                }
                
                // 检查IP地址格式是否合法
                QHostAddress ipAddr;
                if (!ipAddr.setAddress(ip)) {
                    QMessageBox::warning(this, "参数错误", QString("[%1] IP地址格式无效：%2").arg(deviceName).arg(ip));
                    return;
                }
                
                // 清空可能存在的无效参数
                tcpClient->setConnectionParameter(QModbusDevice::SerialPortNameParameter, QVariant());
                tcpClient->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, QVariant());
                
                // 设置TCP参数
                tcpClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
                tcpClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, portNum);
                
                qDebug() << "尝试连接" << deviceName << "- TCP - IP:" << ip << "端口:" << portNum;
            }
        } else {
            // 检查是否需要创建新的RTU客户端
            QModbusRtuSerialClient *serialClient = qobject_cast<QModbusRtuSerialClient*>(m_devices[AirTightDevice].modbusClient);
            if (!serialClient) {
                // 如果客户端存在但类型不匹配，先删除旧客户端
                delete m_devices[AirTightDevice].modbusClient;
                m_devices[AirTightDevice].modbusClient = new QModbusRtuSerialClient(this);
                serialClient = qobject_cast<QModbusRtuSerialClient*>(m_devices[AirTightDevice].modbusClient);
            }
            if (serialClient) {
                // 串口号有效性检查
                bool serialPortValid = false;
                const auto serialPortInfos = QSerialPortInfo::availablePorts();
                for (const QSerialPortInfo &info : serialPortInfos) {
                    if (info.portName() == serialPort) {
                        serialPortValid = true;
                        break;
                    }
                }
                if (!serialPortValid) {
                    QString availablePorts;
                    for (const QSerialPortInfo &info : serialPortInfos) {
                        availablePorts += info.portName() + ", ";
                    }
                    if (!availablePorts.isEmpty()) {
                        availablePorts.chop(2); // 移除最后两个字符 ", "
                    }
                    QMessageBox::warning(this, "参数错误", QString("[%1] 串口号无效：%2\n可用串口：%3").arg(deviceName).arg(serialPort).arg(availablePorts));
                    return;
                }
                
                // 波特率处理
                bool baudOk;
                int baudRate = baudrate.toInt(&baudOk);
                if (!baudOk) {
                    baudRate = SERIAL_BAUD_RATE_DEFAULT;
                    qDebug() << "[" << deviceName << "] 波特率无效，使用默认值:" << baudRate;
                }
                
                // 验证波特率是否为标准波特率
                if (!QSerialPortInfo::standardBaudRates().contains(baudRate)) {
                    QMessageBox::warning(this, "参数错误", QString("[%1] 不支持的波特率：%2，将使用默认值9600").arg(deviceName).arg(baudRate));
                    baudRate = 9600;
                }
                
                // 验证数据位、停止位、奇偶校验范围
                if (dataBits < 0 || dataBits > 3) {
                    dataBits = 0; // 8位数据位
                    qDebug() << "[" << deviceName << "] 数据位无效，使用默认值: 8位";
                }
                if (stopBits < 0 || stopBits > 2) {
                    stopBits = 0; // 1位停止位
                    qDebug() << "[" << deviceName << "] 停止位无效，使用默认值: 1位";
                }
                if (parity < 0 || parity > 2) {
                    parity = 0; // 无校验
                    qDebug() << "[" << deviceName << "] 奇偶校验无效，使用默认值: 无校验";
                }
                
                // 清空可能存在的无效参数
                serialClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, QVariant());
                serialClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, QVariant());
                
                // 转换参数为正确的枚举值
                QSerialPort::Parity parityEnum = parityIndexToEnum(parity);
                QSerialPort::DataBits dataBitsEnum = dataBitsIndexToEnum(dataBits);
                QSerialPort::StopBits stopBitsEnum = stopBitsIndexToEnum(stopBits);
                
                // 设置RTU参数
                serialClient->setConnectionParameter(QModbusDevice::SerialPortNameParameter, serialPort);
                serialClient->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baudRate);
                serialClient->setConnectionParameter(QModbusDevice::SerialParityParameter, parityEnum);
                serialClient->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, dataBitsEnum);
                serialClient->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, stopBitsEnum);
                
                qDebug() << "尝试连接气密仪 - 协议:" << protocol << "串口:" << serialPort << "波特率:" << baudRate;
            }
        }
        
        // 连接Modbus客户端
        if (m_devices[AirTightDevice].modbusClient) {
            // 断开旧的信号连接
            m_devices[AirTightDevice].modbusClient->disconnect(this);
            
            // 确保设备处于未连接状态
            if (m_devices[AirTightDevice].modbusClient->state() == QModbusDevice::ConnectedState) {
                m_devices[AirTightDevice].modbusClient->disconnectDevice();
            }
            
            // 连接状态变化信号
            connect(m_devices[AirTightDevice].modbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
                bool isConnected = (state == QModbusDevice::ConnectedState);
                m_devices[AirTightDevice].connected = isConnected;
                emit airTightConnectionChanged(isConnected);
                updateDeviceStatus(AirTightDevice, isConnected, ui->airtightStatus);
                updateButtonStates();
                
                // 自动重连逻辑
                if (isConnected) {
                    // 连接成功，停止自动重连定时器
                    if (m_devices[AirTightDevice].autoReconnectTimer && m_devices[AirTightDevice].autoReconnectTimer->isActive()) {
                        m_devices[AirTightDevice].autoReconnectTimer->stop();
                        qDebug() << "气密仪连接成功，停止自动重连定时器";
                    }
                } else {
                    // 连接断开，启动自动重连定时器
                    if (m_devices[AirTightDevice].autoReconnectTimer && m_devices[AirTightDevice].autoReconnectEnabled && !m_devices[AirTightDevice].autoReconnectTimer->isActive()) {
                        m_devices[AirTightDevice].autoReconnectTimer->start();
                        qDebug() << "气密仪连接断开，启动自动重连定时器，间隔:" << AUTO_RECONNECT_INTERVAL << "ms";
                    }
                }
            });
            
            // 连接错误信号
            connect(m_devices[AirTightDevice].modbusClient, &QModbusDevice::errorOccurred, this, [this]() {
                QString errorMsg = m_devices[AirTightDevice].modbusClient->errorString();
                QModbusDevice::Error error = m_devices[AirTightDevice].modbusClient->error();
                qDebug() << "气密仪连接错误:" << errorMsg << "(错误代码:" << error << ")";
                
                m_devices[AirTightDevice].connected = false;
                emit airTightConnectionChanged(false);
                updateDeviceStatus(AirTightDevice, false, ui->airtightStatus);
                updateButtonStates();
            });
            
            // 设置连接超时
            m_devices[AirTightDevice].modbusClient->setTimeout(3000);
            m_devices[AirTightDevice].modbusClient->setNumberOfRetries(1);
            
            // 初始化自动重连定时器
            if (!m_devices[AirTightDevice].autoReconnectTimer) {
                m_devices[AirTightDevice].autoReconnectTimer = new QTimer(this);
                connect(m_devices[AirTightDevice].autoReconnectTimer, &QTimer::timeout, this, [this]() {
                    if (!m_devices[AirTightDevice].connected && m_devices[AirTightDevice].autoReconnectEnabled) {
                        qDebug() << "自动重连气密仪...";
                        m_devices[AirTightDevice].modbusClient->connectDevice();
                    }
                });
                m_devices[AirTightDevice].autoReconnectTimer->setInterval(AUTO_RECONNECT_INTERVAL);
            }
            
            // 初始状态设置为未连接
            m_devices[AirTightDevice].connected = false;
            m_devices[AirTightDevice].autoReconnectEnabled = true; // 连接时启用自动重连
            updateDeviceStatus(AirTightDevice, false, ui->airtightStatus);
            
            // 启动连接（异步）
            bool connectResult = m_devices[AirTightDevice].modbusClient->connectDevice();
            qDebug() << "气密仪连接请求发送:" << connectResult << "当前状态:" << m_devices[AirTightDevice].modbusClient->state();
            
            // 发送Modbus客户端变化信号
            emit airTightModbusClientChanged(m_devices[AirTightDevice].modbusClient);
        } else {
            connected = false;
            m_devices[AirTightDevice].connected = false;
            updateDeviceStatus(AirTightDevice, false, ui->airtightStatus);
        }
        break;
    case 1: // 主控板
        deviceName = "主控板";
        protocol = ui->protocol_2->currentText();
        ip = ui->ipLineEdit_2->text();
        port = ui->portLineEdit_2->text();
        serialPort = ui->serialPortComboBox_2->currentText();
        baudrate = ui->baudrateComboBox_2->currentText();
        parity = ui->parityComboBox_2->currentIndex();
        dataBits = ui->dataBitsComboBox_2->currentIndex();
        stopBits = ui->stopBitsComboBox_2->currentIndex();
        
        if (protocol == "Modbus TCP" || protocol == "以太网") {
            // 检查是否需要创建新的TCP客户端
            QModbusTcpClient *tcpClient = qobject_cast<QModbusTcpClient*>(m_devices[MainBoardDevice].modbusClient);
            if (!tcpClient) {
                // 如果客户端存在但类型不匹配，先删除旧客户端
                delete m_devices[MainBoardDevice].modbusClient;
                m_devices[MainBoardDevice].modbusClient = new QModbusTcpClient(this);
                tcpClient = qobject_cast<QModbusTcpClient*>(m_devices[MainBoardDevice].modbusClient);
            }
            if (tcpClient) {
                bool ok;
                int portNum = port.toInt(&ok);
                
                // 端口号有效性检查
                if (!ok || portNum < 1 || portNum > 65535) {
                    QMessageBox::warning(this, "参数错误", QString("[%1] 端口号无效，请输入1-65535之间的整数").arg(deviceName));
                    return;
                }
                
                // 检查IP地址格式是否合法
                QHostAddress ipAddr;
                if (!ipAddr.setAddress(ip)) {
                    QMessageBox::warning(this, "参数错误", QString("[%1] IP地址格式无效：%2").arg(deviceName).arg(ip));
                    return;
                }
                
                // 清空可能存在的无效参数
                tcpClient->setConnectionParameter(QModbusDevice::SerialPortNameParameter, QVariant());
                tcpClient->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, QVariant());
                
                // 设置TCP参数
                tcpClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
                tcpClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, portNum);
                
                qDebug() << "尝试连接" << deviceName << "- TCP - IP:" << ip << "端口:" << portNum;
            }
        } else {
            // 检查是否需要创建新的RTU客户端
            QModbusRtuSerialClient *serialClient = qobject_cast<QModbusRtuSerialClient*>(m_devices[MainBoardDevice].modbusClient);
            if (!serialClient) {
                // 如果客户端存在但类型不匹配，先删除旧客户端
                delete m_devices[MainBoardDevice].modbusClient;
                m_devices[MainBoardDevice].modbusClient = new QModbusRtuSerialClient(this);
                serialClient = qobject_cast<QModbusRtuSerialClient*>(m_devices[MainBoardDevice].modbusClient);
            }
            if (serialClient) {
                // 串口号有效性检查
                bool serialPortValid = false;
                const auto serialPortInfos = QSerialPortInfo::availablePorts();
                for (const QSerialPortInfo &info : serialPortInfos) {
                    if (info.portName() == serialPort) {
                        serialPortValid = true;
                        break;
                    }
                }
                if (!serialPortValid) {
                    QString availablePorts;
                    for (const QSerialPortInfo &info : serialPortInfos) {
                        availablePorts += info.portName() + ", ";
                    }
                    if (!availablePorts.isEmpty()) {
                        availablePorts.chop(2); // 移除最后两个字符 ", "
                    }
                    QMessageBox::warning(this, "参数错误", QString("[%1] 串口号无效：%2\n可用串口：%3").arg(deviceName).arg(serialPort).arg(availablePorts));
                    return;
                }
                
                // 波特率处理
                bool baudOk;
                int baudRate = baudrate.toInt(&baudOk);
                if (!baudOk) {
                    baudRate = SERIAL_BAUD_RATE_DEFAULT;
                    qDebug() << "[" << deviceName << "] 波特率无效，使用默认值:" << baudRate;
                }
                
                // 验证波特率是否为标准波特率
                if (!QSerialPortInfo::standardBaudRates().contains(baudRate)) {
                    QMessageBox::warning(this, "参数错误", QString("[%1] 不支持的波特率：%2，将使用默认值9600").arg(deviceName).arg(baudRate));
                    baudRate = 9600;
                }
                
                // 验证数据位、停止位、奇偶校验范围
                if (dataBits < 0 || dataBits > 3) {
                    dataBits = 0; // 8位数据位
                    qDebug() << "[" << deviceName << "] 数据位无效，使用默认值: 8位";
                }
                if (stopBits < 0 || stopBits > 2) {
                    stopBits = 0; // 1位停止位
                    qDebug() << "[" << deviceName << "] 停止位无效，使用默认值: 1位";
                }
                if (parity < 0 || parity > 2) {
                    parity = 0; // 无校验
                    qDebug() << "[" << deviceName << "] 奇偶校验无效，使用默认值: 无校验";
                }
                
                // 清空可能存在的无效参数
                serialClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, QVariant());
                serialClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, QVariant());
                
                // 转换参数为正确的枚举值
                QSerialPort::Parity parityEnum = parityIndexToEnum(parity);
                QSerialPort::DataBits dataBitsEnum = dataBitsIndexToEnum(dataBits);
                QSerialPort::StopBits stopBitsEnum = stopBitsIndexToEnum(stopBits);
                
                // 设置RTU参数
                serialClient->setConnectionParameter(QModbusDevice::SerialPortNameParameter, serialPort);
                serialClient->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baudRate);
                serialClient->setConnectionParameter(QModbusDevice::SerialParityParameter, parityEnum);
                serialClient->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, dataBitsEnum);
                serialClient->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, stopBitsEnum);
                
                qDebug() << "尝试连接主控板 - 协议:" << protocol << "串口:" << serialPort << "波特率:" << baudRate;
            }
        }
        
        // 连接Modbus客户端
        if (m_devices[MainBoardDevice].modbusClient) {
            // 断开旧的信号连接
            m_devices[MainBoardDevice].modbusClient->disconnect(this);
            
            // 确保设备处于未连接状态
            if (m_devices[MainBoardDevice].modbusClient->state() == QModbusDevice::ConnectedState) {
                m_devices[MainBoardDevice].modbusClient->disconnectDevice();
            }
            
            // 连接状态变化信号
            connect(m_devices[MainBoardDevice].modbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
                bool isConnected = (state == QModbusDevice::ConnectedState);
                m_devices[MainBoardDevice].connected = isConnected;
                emit mainBoardConnectionChanged(isConnected);
                updateDeviceStatus(MainBoardDevice, isConnected, ui->mainboardStatus);
                updateButtonStates();
                
                // 自动重连逻辑
                if (isConnected) {
                    // 连接成功，停止自动重连定时器
                    if (m_devices[MainBoardDevice].autoReconnectTimer && m_devices[MainBoardDevice].autoReconnectTimer->isActive()) {
                        m_devices[MainBoardDevice].autoReconnectTimer->stop();
                        qDebug() << "主控板连接成功，停止自动重连定时器";
                    }
                } else {
                    // 连接断开，启动自动重连定时器
                    if (m_devices[MainBoardDevice].autoReconnectTimer && m_devices[MainBoardDevice].autoReconnectEnabled && !m_devices[MainBoardDevice].autoReconnectTimer->isActive()) {
                        m_devices[MainBoardDevice].autoReconnectTimer->start();
                        qDebug() << "主控板连接断开，启动自动重连定时器，间隔:" << AUTO_RECONNECT_INTERVAL << "ms";
                    }
                }
            });
            
            // 连接错误信号
            connect(m_devices[MainBoardDevice].modbusClient, &QModbusDevice::errorOccurred, this, [this]() {
                QString errorMsg = m_devices[MainBoardDevice].modbusClient->errorString();
                QModbusDevice::Error error = m_devices[MainBoardDevice].modbusClient->error();
                qDebug() << "主控板连接错误:" << errorMsg << "(错误代码:" << error << ")";
                
                m_devices[MainBoardDevice].connected = false;
                emit mainBoardConnectionChanged(false);
                updateDeviceStatus(MainBoardDevice, false, ui->mainboardStatus);
                updateButtonStates();
            });
            
            // 设置连接超时
            m_devices[MainBoardDevice].modbusClient->setTimeout(3000);
            m_devices[MainBoardDevice].modbusClient->setNumberOfRetries(1);
            
            // 初始化自动重连定时器
            if (!m_devices[MainBoardDevice].autoReconnectTimer) {
                m_devices[MainBoardDevice].autoReconnectTimer = new QTimer(this);
                connect(m_devices[MainBoardDevice].autoReconnectTimer, &QTimer::timeout, this, [this]() {
                    if (!m_devices[MainBoardDevice].connected && m_devices[MainBoardDevice].autoReconnectEnabled) {
                        qDebug() << "自动重连主控板...";
                        m_devices[MainBoardDevice].modbusClient->connectDevice();
                    }
                });
                m_devices[MainBoardDevice].autoReconnectTimer->setInterval(AUTO_RECONNECT_INTERVAL);
            }
            
            // 初始状态设置为未连接
            m_devices[MainBoardDevice].connected = false;
            m_devices[MainBoardDevice].autoReconnectEnabled = true; // 连接时启用自动重连
            updateDeviceStatus(MainBoardDevice, false, ui->mainboardStatus);
            
            // 启动连接（异步）
            bool connectResult = m_devices[MainBoardDevice].modbusClient->connectDevice();
            qDebug() << "主控板连接请求发送:" << connectResult << "当前状态:" << m_devices[MainBoardDevice].modbusClient->state();
            
            // 发送Modbus客户端变化信号
            emit mainBoardModbusClientChanged(m_devices[MainBoardDevice].modbusClient);
        } else {
            connected = false;
            m_devices[MainBoardDevice].connected = false;
            updateDeviceStatus(MainBoardDevice, false, ui->mainboardStatus);
        }
        break;
    case 2: // 调压
        deviceName = "调压";
        protocol = ui->protocol_3->currentText();
        ip = ui->ipLineEdit_3->text();
        port = ui->portLineEdit_3->text();
        serialPort = ui->serialPortComboBox_3->currentText();
        baudrate = ui->baudrateComboBox_3->currentText();
        parity = ui->parityComboBox_3->currentIndex();
        dataBits = ui->dataBitsComboBox_3->currentIndex();
        stopBits = ui->stopBitsComboBox_3->currentIndex();
        
        if (protocol == "Modbus TCP" || protocol == "以太网") {
            // 检查是否需要创建新的TCP客户端
            QModbusTcpClient *tcpClient = qobject_cast<QModbusTcpClient*>(m_devices[PressureRegulatorDevice].modbusClient);
            if (!tcpClient) {
                // 如果客户端存在但类型不匹配，先删除旧客户端
                delete m_devices[PressureRegulatorDevice].modbusClient;
                m_devices[PressureRegulatorDevice].modbusClient = new QModbusTcpClient(this);
                tcpClient = qobject_cast<QModbusTcpClient*>(m_devices[PressureRegulatorDevice].modbusClient);
            }
            if (tcpClient) {
                bool ok;
                int portNum = port.toInt(&ok);
                
                // 端口号有效性检查
                if (!ok || portNum < 1 || portNum > 65535) {
                    QMessageBox::warning(this, "参数错误", QString("[%1] 端口号无效，请输入1-65535之间的整数").arg(deviceName));
                    return;
                }
                
                // 检查IP地址格式是否合法
                QHostAddress ipAddr;
                if (!ipAddr.setAddress(ip)) {
                    QMessageBox::warning(this, "参数错误", QString("[%1] IP地址格式无效：%2").arg(deviceName).arg(ip));
                    return;
                }
                
                // 清空可能存在的无效参数
                tcpClient->setConnectionParameter(QModbusDevice::SerialPortNameParameter, QVariant());
                tcpClient->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, QVariant());
                
                // 设置TCP参数
                tcpClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
                tcpClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, portNum);
                
                qDebug() << "尝试连接" << deviceName << "- TCP - IP:" << ip << "端口:" << portNum;
            }
        } else {
            // 检查是否需要创建新的RTU客户端
            QModbusRtuSerialClient *serialClient = qobject_cast<QModbusRtuSerialClient*>(m_devices[PressureRegulatorDevice].modbusClient);
            if (!serialClient) {
                // 如果客户端存在但类型不匹配，先删除旧客户端
                delete m_devices[PressureRegulatorDevice].modbusClient;
                m_devices[PressureRegulatorDevice].modbusClient = new QModbusRtuSerialClient(this);
                serialClient = qobject_cast<QModbusRtuSerialClient*>(m_devices[PressureRegulatorDevice].modbusClient);
            }
            if (serialClient) {
                // 串口号有效性检查
                bool serialPortValid = false;
                const auto serialPortInfos = QSerialPortInfo::availablePorts();
                for (const QSerialPortInfo &info : serialPortInfos) {
                    if (info.portName() == serialPort) {
                        serialPortValid = true;
                        break;
                    }
                }
                if (!serialPortValid) {
                    QString availablePorts;
                    for (const QSerialPortInfo &info : serialPortInfos) {
                        availablePorts += info.portName() + ", ";
                    }
                    if (!availablePorts.isEmpty()) {
                        availablePorts.chop(2); // 移除最后两个字符 ", "
                    }
                    QMessageBox::warning(this, "参数错误", QString("[%1] 串口号无效：%2\n可用串口：%3").arg(deviceName).arg(serialPort).arg(availablePorts));
                    return;
                }
                
                // 波特率处理
                bool baudOk;
                int baudRate = baudrate.toInt(&baudOk);
                if (!baudOk) {
                    baudRate = SERIAL_BAUD_RATE_DEFAULT;
                    qDebug() << "[" << deviceName << "] 波特率无效，使用默认值:" << baudRate;
                }
                
                // 验证波特率是否为标准波特率
                if (!QSerialPortInfo::standardBaudRates().contains(baudRate)) {
                    QMessageBox::warning(this, "参数错误", QString("[%1] 不支持的波特率：%2，将使用默认值9600").arg(deviceName).arg(baudRate));
                    baudRate = 9600;
                }
                
                // 验证数据位、停止位、奇偶校验范围
                if (dataBits < 0 || dataBits > 3) {
                    dataBits = 0; // 8位数据位
                    qDebug() << "[" << deviceName << "] 数据位无效，使用默认值: 8位";
                }
                if (stopBits < 0 || stopBits > 2) {
                    stopBits = 0; // 1位停止位
                    qDebug() << "[" << deviceName << "] 停止位无效，使用默认值: 1位";
                }
                if (parity < 0 || parity > 2) {
                    parity = 0; // 无校验
                    qDebug() << "[" << deviceName << "] 奇偶校验无效，使用默认值: 无校验";
                }
                
                // 清空可能存在的无效参数
                serialClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, QVariant());
                serialClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, QVariant());
                
                // 转换参数为正确的枚举值
                QSerialPort::Parity parityEnum = parityIndexToEnum(parity);
                QSerialPort::DataBits dataBitsEnum = dataBitsIndexToEnum(dataBits);
                QSerialPort::StopBits stopBitsEnum = stopBitsIndexToEnum(stopBits);
                
                // 设置RTU参数
                serialClient->setConnectionParameter(QModbusDevice::SerialPortNameParameter, serialPort);
                serialClient->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baudRate);
                serialClient->setConnectionParameter(QModbusDevice::SerialParityParameter, parityEnum);
                serialClient->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, dataBitsEnum);
                serialClient->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, stopBitsEnum);
                
                qDebug() << "尝试连接调压装置 - 协议:" << protocol << "串口:" << serialPort << "波特率:" << baudRate;
            }
        }
        
        // 连接Modbus客户端
        if (m_devices[PressureRegulatorDevice].modbusClient) {
            // 断开旧的信号连接
            m_devices[PressureRegulatorDevice].modbusClient->disconnect(this);
            
            // 确保设备处于未连接状态
            if (m_devices[PressureRegulatorDevice].modbusClient->state() == QModbusDevice::ConnectedState) {
                m_devices[PressureRegulatorDevice].modbusClient->disconnectDevice();
            }
            
            // 连接状态变化信号
            connect(m_devices[PressureRegulatorDevice].modbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
                bool isConnected = (state == QModbusDevice::ConnectedState);
                m_devices[PressureRegulatorDevice].connected = isConnected;
                emit pressureRegulatorConnectionChanged(isConnected);
                updateDeviceStatus(PressureRegulatorDevice, isConnected, ui->pressureStatus);
                updateButtonStates();
                
                // 自动重连逻辑
                if (isConnected) {
                    // 连接成功，停止自动重连定时器
                    if (m_devices[PressureRegulatorDevice].autoReconnectTimer && m_devices[PressureRegulatorDevice].autoReconnectTimer->isActive()) {
                        m_devices[PressureRegulatorDevice].autoReconnectTimer->stop();
                        qDebug() << "调压装置连接成功，停止自动重连定时器";
                    }
                } else {
                    // 连接断开，启动自动重连定时器
                    if (m_devices[PressureRegulatorDevice].autoReconnectTimer && m_devices[PressureRegulatorDevice].autoReconnectEnabled && !m_devices[PressureRegulatorDevice].autoReconnectTimer->isActive()) {
                        m_devices[PressureRegulatorDevice].autoReconnectTimer->start();
                        qDebug() << "调压装置连接断开，启动自动重连定时器，间隔:" << AUTO_RECONNECT_INTERVAL << "ms";
                    }
                }
            });
            
            // 连接错误信号
            connect(m_devices[PressureRegulatorDevice].modbusClient, &QModbusDevice::errorOccurred, this, [this]() {
                QString errorMsg = m_devices[PressureRegulatorDevice].modbusClient->errorString();
                QModbusDevice::Error error = m_devices[PressureRegulatorDevice].modbusClient->error();
                qDebug() << "调压装置连接错误:" << errorMsg << "(错误代码:" << error << ")";
                
                m_devices[PressureRegulatorDevice].connected = false;
                emit pressureRegulatorConnectionChanged(false);
                updateDeviceStatus(PressureRegulatorDevice, false, ui->pressureStatus);
                updateButtonStates();
            });
            
            // 设置连接超时
            m_devices[PressureRegulatorDevice].modbusClient->setTimeout(3000);
            m_devices[PressureRegulatorDevice].modbusClient->setNumberOfRetries(1);
            
            // 初始化自动重连定时器
            if (!m_devices[PressureRegulatorDevice].autoReconnectTimer) {
                m_devices[PressureRegulatorDevice].autoReconnectTimer = new QTimer(this);
                connect(m_devices[PressureRegulatorDevice].autoReconnectTimer, &QTimer::timeout, this, [this]() {
                    if (!m_devices[PressureRegulatorDevice].connected && m_devices[PressureRegulatorDevice].autoReconnectEnabled) {
                        qDebug() << "自动重连调压装置...";
                        m_devices[PressureRegulatorDevice].modbusClient->connectDevice();
                    }
                });
                m_devices[PressureRegulatorDevice].autoReconnectTimer->setInterval(AUTO_RECONNECT_INTERVAL);
            }
            
            // 初始状态设置为未连接
            m_devices[PressureRegulatorDevice].connected = false;
            m_devices[PressureRegulatorDevice].autoReconnectEnabled = true; // 连接时启用自动重连
            updateDeviceStatus(PressureRegulatorDevice, false, ui->pressureStatus);
            
            // 启动连接（异步）
            bool connectResult = m_devices[PressureRegulatorDevice].modbusClient->connectDevice();
            qDebug() << "调压装置连接请求发送:" << connectResult << "当前状态:" << m_devices[PressureRegulatorDevice].modbusClient->state();
            
            // 发送Modbus客户端变化信号
            emit pressureRegulatorModbusClientChanged(m_devices[PressureRegulatorDevice].modbusClient);
        } else {
            connected = false;
            m_devices[PressureRegulatorDevice].connected = false;
            updateDeviceStatus(PressureRegulatorDevice, false, ui->pressureStatus);
        }
        break;
    default:
        return;
    }
    
    // 构建连接信息
    QString message;
    if (connected) {
        message = QString("%1已连接: %2\n").arg(deviceName).arg(protocol);
        
        if (protocol == "Modbus TCP" || protocol == "以太网") {
            message += QString("IP: %1, Port: %2").arg(ip).arg(port);
        } else {
            message += QString("串口: %1").arg(serialPort);
        }
        // QMessageBox::information(this, "连接状态", message);
    } else {
        message = QString("%1连接失败: %2\n").arg(deviceName).arg(protocol);
        
        if (protocol == "Modbus TCP" || protocol == "以太网") {
            message += QString("IP: %1, Port: %2").arg(ip).arg(port);
        } else {
            message += QString("串口: %1").arg(serialPort);
        }
        // QMessageBox::warning(this, "连接状态", message);
    }
    
    // 更新按钮状态
    updateButtonStates();
    
    // 状态更新已经通过Modbus客户端的stateChanged和errorOccurred信号处理
    
    // 连接成功时保存当前设备的设置
    if (connected) {
        // 保存当前设备的设置
        DatabaseManager* dbManager = DatabaseManager::getInstance();
        dbManager->connectDatabase();
        
        QMap<QString, QVariant> params;
        switch (deviceIndex) {
        case 0: // 气密仪
            params["protocol"] = ui->protocol->currentText();
            params["ip_address"] = ui->ipLineEdit->text();
            params["port"] = ui->portLineEdit->text().toInt();
            params["serial_port"] = ui->serialPortComboBox->currentText();
            params["baudrate"] = ui->baudrateComboBox->currentText().toInt();
            params["parity"] = ui->parityComboBox->currentText();
            params["data_bits"] = ui->dataBitsComboBox->currentText().toInt();
            params["stop_bits"] = ui->stopBitsComboBox->currentText();
            break;
        case 1: // 主控板
            params["protocol"] = ui->protocol_2->currentText();
            params["ip_address"] = ui->ipLineEdit_2->text();
            params["port"] = ui->portLineEdit_2->text().toInt();
            params["serial_port"] = ui->serialPortComboBox_2->currentText();
            params["baudrate"] = ui->baudrateComboBox_2->currentText().toInt();
            params["parity"] = ui->parityComboBox_2->currentText();
            params["data_bits"] = ui->dataBitsComboBox_2->currentText().toInt();
            params["stop_bits"] = ui->stopBitsComboBox_2->currentText();
            break;
        case 2: // 调压装置
            params["protocol"] = ui->protocol_3->currentText();
            params["ip_address"] = ui->ipLineEdit_3->text();
            params["port"] = ui->portLineEdit_3->text().toInt();
            params["serial_port"] = ui->serialPortComboBox_3->currentText();
            params["baudrate"] = ui->baudrateComboBox_3->currentText().toInt();
            params["parity"] = ui->parityComboBox_3->currentText();
            params["data_bits"] = ui->dataBitsComboBox_3->currentText().toInt();
            params["stop_bits"] = ui->stopBitsComboBox_3->currentText();
            break;
        default:
            break;
        }
        
        if (dbManager->hasCommunicationParams(deviceIndex)) {
            dbManager->updateCommunicationParams(deviceIndex, params);
        } else {
            dbManager->saveCommunicationParams(deviceIndex, params);
        }
    }
}

void CommsPage::disconnectDevice(int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex > 2) {
        return;
    }
    
    QString deviceName;
    QLabel *statusLabel = nullptr;
    
    switch (deviceIndex) {
    case AirTightDevice: // 气密仪
        deviceName = "气密仪";
        statusLabel = ui->airtightStatus;
        break;
    case MainBoardDevice: // 主控板
        deviceName = "主控板";
        statusLabel = ui->mainboardStatus;
        break;
    case PressureRegulatorDevice: // 调压
        deviceName = "调压";
        statusLabel = ui->pressureStatus;
        break;
    default:
        return;
    }
    
    qDebug() << "开始断开" << deviceName << "连接...";
    
    // 停止自动重连定时器
    if (m_devices[deviceIndex].autoReconnectTimer && m_devices[deviceIndex].autoReconnectTimer->isActive()) {
        m_devices[deviceIndex].autoReconnectTimer->stop();
        qDebug() << "手动断开" << deviceName << "，停止自动重连定时器";
    }
    m_devices[deviceIndex].autoReconnectEnabled = false; // 手动断开时禁用自动重连
    
    // 断开Modbus客户端连接并释放资源
    if (m_devices[deviceIndex].modbusClient) {
        // 断开所有信号连接
        m_devices[deviceIndex].modbusClient->disconnect(this);
        
        if (m_devices[deviceIndex].modbusClient->state() == QModbusDevice::ConnectedState) {
            m_devices[deviceIndex].modbusClient->disconnectDevice();
            
            // 等待断开完成，最多等待1秒
            int timeout = 0;
            while (m_devices[deviceIndex].modbusClient->state() != QModbusDevice::UnconnectedState && timeout < 10) {
                QThread::msleep(100);
                QCoreApplication::processEvents();
                timeout++;
            }
        }
        
        // 删除并释放Modbus客户端，这会释放内部的串口资源
        delete m_devices[deviceIndex].modbusClient;
        m_devices[deviceIndex].modbusClient = nullptr;
        qDebug() << deviceName << "Modbus客户端已释放";
    }
    
    // 断开TCP连接
    if (m_devices[deviceIndex].tcpSocket) {
        if (m_devices[deviceIndex].tcpSocket->isOpen()) {
            m_devices[deviceIndex].tcpSocket->disconnectFromHost();
            m_devices[deviceIndex].tcpSocket->waitForDisconnected(1000);
        }
    }
    
    // 断开串口连接
    if (m_devices[deviceIndex].serialPort) {
        if (m_devices[deviceIndex].serialPort->isOpen()) {
            m_devices[deviceIndex].serialPort->close();
        }
    }
    
    // 更新连接状态
    m_devices[deviceIndex].connected = false;
    statusLabel->setText("未连接");
    statusLabel->setStyleSheet("color: #e74c3c; font-weight: 600;");
    
    // 发送连接状态变化信号
    switch (deviceIndex) {
    case AirTightDevice:
        emit airTightConnectionChanged(false);
        emit airTightModbusClientChanged(nullptr); // 通知客户端已释放
        break;
    case MainBoardDevice:
        emit mainBoardConnectionChanged(false);
        emit mainBoardModbusClientChanged(nullptr); // 通知客户端已释放
        break;
    case PressureRegulatorDevice:
        emit pressureRegulatorConnectionChanged(false);
        emit pressureRegulatorModbusClientChanged(nullptr); // 通知客户端已释放
        break;
    }
    
    // 更新按钮状态
    updateButtonStates();
    
    qDebug() << deviceName << "断开连接完成";
}

void CommsPage::updateButtonStates()
{
    // 气密仪按钮状态
    ui->connectionButton->setEnabled(!m_devices[AirTightDevice].connected);
    ui->disconnectButton->setEnabled(m_devices[AirTightDevice].connected);
    
    // 主控板按钮状态
    ui->connectionButton_2->setEnabled(!m_devices[MainBoardDevice].connected);
    ui->disconnectButton_2->setEnabled(m_devices[MainBoardDevice].connected);
    
    // 调压按钮状态
    ui->connectionButton_3->setEnabled(!m_devices[PressureRegulatorDevice].connected);
    ui->disconnectButton_3->setEnabled(m_devices[PressureRegulatorDevice].connected);
    
    // 检查所有设备是否都已连接
    bool allConnected = (m_devices[AirTightDevice].connected && 
                        m_devices[MainBoardDevice].connected && 
                        m_devices[PressureRegulatorDevice].connected);
    
    if (allConnected) {
        // 所有设备连接成功，发送信号
        emit allDevicesConnected();
    }
}

void CommsPage::updateProtocolSettings()
{
    updateProtocolSettings(0);
    updateProtocolSettings(1);
    updateProtocolSettings(2);
}

void CommsPage::updateProtocolSettings(int deviceIndex)
{
    switch (deviceIndex) {
    case 0: {
        // 气密仪协议设置
        QString protocol = ui->protocol->currentText();
        bool isTcpOrEthernet = (protocol == "Modbus TCP" || protocol == "以太网");
        bool isRtuOrSerial = (protocol == "Modbus RTU" || protocol == "串口");
        
        ui->ipLabel->setVisible(isTcpOrEthernet);
        ui->ipLineEdit->setVisible(isTcpOrEthernet);
        ui->portLabel->setVisible(isTcpOrEthernet);
        ui->portLineEdit->setVisible(isTcpOrEthernet);
        ui->serialPortLabel->setVisible(isRtuOrSerial);
        ui->serialPortComboBox->setVisible(isRtuOrSerial);
        ui->refreshSerialPortButton->setVisible(isRtuOrSerial);
        
        ui->baudrateLabel->setVisible(isRtuOrSerial);
        ui->baudrateComboBox->setVisible(isRtuOrSerial);
        ui->parityLabel->setVisible(isRtuOrSerial);
        ui->parityComboBox->setVisible(isRtuOrSerial);
        ui->dataBitsLabel->setVisible(isRtuOrSerial);
        ui->dataBitsComboBox->setVisible(isRtuOrSerial);
        ui->stopBitsLabel->setVisible(isRtuOrSerial);
        ui->stopBitsComboBox->setVisible(isRtuOrSerial);
        break;
    }
    case 1: {
        // 主控板协议设置
        QString protocol = ui->protocol_2->currentText();
        bool isTcpOrEthernet = (protocol == "Modbus TCP" || protocol == "以太网");
        bool isRtuOrSerial = (protocol == "Modbus RTU" || protocol == "串口");
        
        ui->ipLabel_2->setVisible(isTcpOrEthernet);
        ui->ipLineEdit_2->setVisible(isTcpOrEthernet);
        ui->portLabel_2->setVisible(isTcpOrEthernet);
        ui->portLineEdit_2->setVisible(isTcpOrEthernet);
        ui->serialPortLabel_2->setVisible(isRtuOrSerial);
        ui->serialPortComboBox_2->setVisible(isRtuOrSerial);
        ui->refreshSerialPortButton_2->setVisible(isRtuOrSerial);
        
        ui->baudrateLabel_2->setVisible(isRtuOrSerial);
        ui->baudrateComboBox_2->setVisible(isRtuOrSerial);
        ui->parityLabel_2->setVisible(isRtuOrSerial);
        ui->parityComboBox_2->setVisible(isRtuOrSerial);
        ui->dataBitsLabel_2->setVisible(isRtuOrSerial);
        ui->dataBitsComboBox_2->setVisible(isRtuOrSerial);
        ui->stopBitsLabel_2->setVisible(isRtuOrSerial);
        ui->stopBitsComboBox_2->setVisible(isRtuOrSerial);
        break;
    }
    case 2: {
        // 调压协议设置
        QString protocol = ui->protocol_3->currentText();
        bool isTcpOrEthernet = (protocol == "Modbus TCP" || protocol == "以太网");
        bool isRtuOrSerial = (protocol == "Modbus RTU" || protocol == "串口");
        
        ui->ipLabel_3->setVisible(isTcpOrEthernet);
        ui->ipLineEdit_3->setVisible(isTcpOrEthernet);
        ui->portLabel_3->setVisible(isTcpOrEthernet);
        ui->portLineEdit_3->setVisible(isTcpOrEthernet);
        ui->serialPortLabel_3->setVisible(isRtuOrSerial);
        ui->serialPortComboBox_3->setVisible(isRtuOrSerial);
        ui->refreshSerialPortButton_3->setVisible(isRtuOrSerial);
        
        ui->baudrateLabel_3->setVisible(isRtuOrSerial);
        ui->baudrateComboBox_3->setVisible(isRtuOrSerial);
        ui->parityLabel_3->setVisible(isRtuOrSerial);
        ui->parityComboBox_3->setVisible(isRtuOrSerial);
        ui->dataBitsLabel_3->setVisible(isRtuOrSerial);
        ui->dataBitsComboBox_3->setVisible(isRtuOrSerial);
        ui->stopBitsLabel_3->setVisible(isRtuOrSerial);
        ui->stopBitsComboBox_3->setVisible(isRtuOrSerial);
        break;
    }
    default:
        break;
    }
}

void CommsPage::loadSettings()
{
    // 连接数据库
    DatabaseManager* dbManager = DatabaseManager::getInstance();
    dbManager->connectDatabase();
    
    // 加载气密仪设置
    QMap<QString, QVariant> airtightParams = dbManager->getCommunicationParams(AirTightDevice);
    if (airtightParams.isEmpty()) {
        // 默认设置
        ui->protocol->setCurrentIndex(0);
        ui->ipLineEdit->setText("192.168.1.10");
        ui->portLineEdit->setText("502");
        ui->baudrateComboBox->setCurrentText("9600");
        ui->parityComboBox->setCurrentIndex(0);
        ui->dataBitsComboBox->setCurrentIndex(0);
        ui->stopBitsComboBox->setCurrentIndex(0);
    } else {
        // 从数据库加载
        QString protocol = airtightParams["protocol"].toString();
        int protocolIndex = ui->protocol->findText(protocol);
        if (protocolIndex >= 0) {
            ui->protocol->setCurrentIndex(protocolIndex);
        }
        ui->ipLineEdit->setText(airtightParams["ip_address"].toString());
        ui->portLineEdit->setText(airtightParams["port"].toString());
        
        int airtightSerialIndex = ui->serialPortComboBox->findText(airtightParams["serial_port"].toString());
        if (airtightSerialIndex >= 0) {
            ui->serialPortComboBox->setCurrentIndex(airtightSerialIndex);
        }
        
        ui->baudrateComboBox->setCurrentText(airtightParams["baudrate"].toString());
        
        QString parity = airtightParams["parity"].toString();
        int parityIndex = ui->parityComboBox->findText(parity);
        if (parityIndex >= 0) {
            ui->parityComboBox->setCurrentIndex(parityIndex);
        }
        
        ui->dataBitsComboBox->setCurrentText(airtightParams["data_bits"].toString());
        
        QString stopBits = airtightParams["stop_bits"].toString();
        int stopBitsIndex = ui->stopBitsComboBox->findText(stopBits);
        if (stopBitsIndex >= 0) {
            ui->stopBitsComboBox->setCurrentIndex(stopBitsIndex);
        }
    }
    // 加载主控板设置
    QMap<QString, QVariant> mainboardParams = dbManager->getCommunicationParams(MainBoardDevice);
    if (mainboardParams.isEmpty()) {
        // 默认设置
        ui->protocol_2->setCurrentIndex(0);
        ui->ipLineEdit_2->setText("192.168.1.10");
        ui->portLineEdit_2->setText("503");
        ui->baudrateComboBox_2->setCurrentText("38400");
        ui->parityComboBox_2->setCurrentIndex(0);
        ui->dataBitsComboBox_2->setCurrentIndex(0);
        ui->stopBitsComboBox_2->setCurrentIndex(0);
    } else {
        // 从数据库加载
        QString protocol = mainboardParams["protocol"].toString();
        int protocolIndex = ui->protocol_2->findText(protocol);
        if (protocolIndex >= 0) {
            ui->protocol_2->setCurrentIndex(protocolIndex);
        }
        ui->ipLineEdit_2->setText(mainboardParams["ip_address"].toString());
        ui->portLineEdit_2->setText(mainboardParams["port"].toString());
        
        int mainboardSerialIndex = ui->serialPortComboBox_2->findText(mainboardParams["serial_port"].toString());
        if (mainboardSerialIndex >= 0) {
            ui->serialPortComboBox_2->setCurrentIndex(mainboardSerialIndex);
        }
        
        ui->baudrateComboBox_2->setCurrentText(mainboardParams["baudrate"].toString());
        
        QString parity = mainboardParams["parity"].toString();
        int parityIndex = ui->parityComboBox_2->findText(parity);
        if (parityIndex >= 0) {
            ui->parityComboBox_2->setCurrentIndex(parityIndex);
        }
        
        ui->dataBitsComboBox_2->setCurrentText(mainboardParams["data_bits"].toString());
        
        QString stopBits = mainboardParams["stop_bits"].toString();
        int stopBitsIndex = ui->stopBitsComboBox_2->findText(stopBits);
        if (stopBitsIndex >= 0) {
            ui->stopBitsComboBox_2->setCurrentIndex(stopBitsIndex);
        }
    }
    
    // 加载调压设置
    QMap<QString, QVariant> pressureParams = dbManager->getCommunicationParams(PressureRegulatorDevice);
    if (pressureParams.isEmpty()) {
        // 默认设置
        ui->protocol_3->setCurrentIndex(0);
        ui->ipLineEdit_3->setText("192.168.1.102");
        ui->portLineEdit_3->setText("504");
        ui->baudrateComboBox_3->setCurrentText("9600");
        ui->parityComboBox_3->setCurrentIndex(0);
        ui->dataBitsComboBox_3->setCurrentIndex(0);
        ui->stopBitsComboBox_3->setCurrentIndex(0);
    } else {
        // 从数据库加载
        QString protocol = pressureParams["protocol"].toString();
        int protocolIndex = ui->protocol_3->findText(protocol);
        if (protocolIndex >= 0) {
            ui->protocol_3->setCurrentIndex(protocolIndex);
        }
        ui->ipLineEdit_3->setText(pressureParams["ip_address"].toString());
        ui->portLineEdit_3->setText(pressureParams["port"].toString());
        
        int pressureSerialIndex = ui->serialPortComboBox_3->findText(pressureParams["serial_port"].toString());
        if (pressureSerialIndex >= 0) {
            ui->serialPortComboBox_3->setCurrentIndex(pressureSerialIndex);
        }
        
        ui->baudrateComboBox_3->setCurrentText(pressureParams["baudrate"].toString());
        
        QString parity = pressureParams["parity"].toString();
        int parityIndex = ui->parityComboBox_3->findText(parity);
        if (parityIndex >= 0) {
            ui->parityComboBox_3->setCurrentIndex(parityIndex);
        }
        
        ui->dataBitsComboBox_3->setCurrentText(pressureParams["data_bits"].toString());
        
        QString stopBits = pressureParams["stop_bits"].toString();
        int stopBitsIndex = ui->stopBitsComboBox_3->findText(stopBits);
        if (stopBitsIndex >= 0) {
            ui->stopBitsComboBox_3->setCurrentIndex(stopBitsIndex);
        }
    }
}

void CommsPage::saveSettings()
{
    // 检查UI是否有效
    if (!ui) {
        qDebug() << "saveSettings: UI is null, skipping save";
        return;
    }
    
    // 连接数据库（使用AppData目录的数据库路径）
    DatabaseManager* dbManager = DatabaseManager::getInstance();
    QString dbPath = DatabaseManager::getDatabasePath();
    if (!dbManager->connectDatabase(dbPath)) {
        qDebug() << "saveSettings: Failed to connect database";
        return;
    }
    
    // 确保数据库表已初始化
    if (!dbManager->initializeDatabase()) {
        qDebug() << "saveSettings: Failed to initialize database";
        return;
    }
    
    // 保存气密仪设置
    QMap<QString, QVariant> airtightParams;
    airtightParams["protocol"] = ui->protocol->currentText();
    airtightParams["ip_address"] = ui->ipLineEdit->text();
    airtightParams["port"] = ui->portLineEdit->text().toInt();
    airtightParams["serial_port"] = ui->serialPortComboBox->currentText();
    airtightParams["baudrate"] = ui->baudrateComboBox->currentText().toInt();
    airtightParams["parity"] = ui->parityComboBox->currentText();
    airtightParams["data_bits"] = ui->dataBitsComboBox->currentText().toInt();
    airtightParams["stop_bits"] = ui->stopBitsComboBox->currentText();
    
    if (dbManager->hasCommunicationParams(AirTightDevice)) {
        dbManager->updateCommunicationParams(AirTightDevice, airtightParams);
    } else {
        dbManager->saveCommunicationParams(AirTightDevice, airtightParams);
    }
    
    // 保存主控板设置
    QMap<QString, QVariant> mainboardParams;
    mainboardParams["protocol"] = ui->protocol_2->currentText();
    mainboardParams["ip_address"] = ui->ipLineEdit_2->text();
    mainboardParams["port"] = ui->portLineEdit_2->text().toInt();
    mainboardParams["serial_port"] = ui->serialPortComboBox_2->currentText();
    mainboardParams["baudrate"] = ui->baudrateComboBox_2->currentText().toInt();
    mainboardParams["parity"] = ui->parityComboBox_2->currentText();
    mainboardParams["data_bits"] = ui->dataBitsComboBox_2->currentText().toInt();
    mainboardParams["stop_bits"] = ui->stopBitsComboBox_2->currentText();
    
    if (dbManager->hasCommunicationParams(MainBoardDevice)) {
        dbManager->updateCommunicationParams(MainBoardDevice, mainboardParams);
    } else {
        dbManager->saveCommunicationParams(MainBoardDevice, mainboardParams);
    }
    
    // 保存调压设置
    QMap<QString, QVariant> pressureParams;
    pressureParams["protocol"] = ui->protocol_3->currentText();
    pressureParams["ip_address"] = ui->ipLineEdit_3->text();
    pressureParams["port"] = ui->portLineEdit_3->text().toInt();
    pressureParams["serial_port"] = ui->serialPortComboBox_3->currentText();
    pressureParams["baudrate"] = ui->baudrateComboBox_3->currentText().toInt();
    pressureParams["parity"] = ui->parityComboBox_3->currentText();
    pressureParams["data_bits"] = ui->dataBitsComboBox_3->currentText().toInt();
    pressureParams["stop_bits"] = ui->stopBitsComboBox_3->currentText();
    
    if (dbManager->hasCommunicationParams(PressureRegulatorDevice)) {
        dbManager->updateCommunicationParams(PressureRegulatorDevice, pressureParams);
    } else {
        dbManager->saveCommunicationParams(PressureRegulatorDevice, pressureParams);
    }
}

// 获取气密仪连接状态
bool CommsPage::getAirTightConnected() const
{
    return m_devices[AirTightDevice].connected;
}

// 获取主控板连接状态
bool CommsPage::getMainBoardConnected() const
{
    return m_devices[MainBoardDevice].connected;
}

// 获取调压装置连接状态
bool CommsPage::getPressureRegulatorConnected() const
{
    return m_devices[PressureRegulatorDevice].connected;
}

// 获取气密仪Modbus客户端实例
QModbusClient *CommsPage::getAirTightModbusClient() const
{
    return m_devices[AirTightDevice].modbusClient;
}

// 获取主控板Modbus客户端实例
QModbusClient *CommsPage::getMainBoardModbusClient() const
{
    return m_devices[MainBoardDevice].modbusClient;
}

// 获取调压Modbus客户端实例
QModbusClient *CommsPage::getPressureRegulatorModbusClient() const
{
    return m_devices[PressureRegulatorDevice].modbusClient;
}

// 获取气密仪从站ID
quint8 CommsPage::getAirTightSlaveId() const
{
    return m_devices[AirTightDevice].slaveId;
}

// 获取主控板从站ID
quint8 CommsPage::getMainBoardSlaveId() const
{
    return m_devices[MainBoardDevice].slaveId;
}

// 获取调压从站ID
quint8 CommsPage::getPressureRegulatorSlaveId() const
{
    return m_devices[PressureRegulatorDevice].slaveId;
}

// 设置气密仪从站ID
void CommsPage::setAirTightSlaveId(quint8 slaveId)
{
    if (m_devices[AirTightDevice].slaveId != slaveId) {
        m_devices[AirTightDevice].slaveId = slaveId;
        emit airTightSlaveIdChanged(slaveId);
    }
}

// 设置主控板从站ID
void CommsPage::setMainBoardSlaveId(quint8 slaveId)
{
    if (m_devices[MainBoardDevice].slaveId != slaveId) {
        m_devices[MainBoardDevice].slaveId = slaveId;
        emit mainBoardSlaveIdChanged(slaveId);
    }
}

// 设置调压从站ID
void CommsPage::setPressureRegulatorSlaveId(quint8 slaveId)
{
    if (m_devices[PressureRegulatorDevice].slaveId != slaveId) {
        m_devices[PressureRegulatorDevice].slaveId = slaveId;
        emit pressureRegulatorSlaveIdChanged(slaveId);
    }
}

bool CommsPage::connectSerialPort(QSerialPort *serialPort, const QString &portName, const QString &baudrate, int parity, int dataBits, int stopBits)
{
    if (!serialPort) {
        return false;
    }
    
    // 如果串口已经打开，先关闭
    if (serialPort->isOpen()) {
        serialPort->close();
    }
    
    // 设置串口参数
    serialPort->setPortName(portName);
    serialPort->setBaudRate(baudrate.toInt());
    
    // 设置奇偶校验
    switch (parity) {
    case 0: // 无
        serialPort->setParity(QSerialPort::NoParity);
        break;
    case 1: // 奇
        serialPort->setParity(QSerialPort::OddParity);
        break;
    case 2: // 偶
        serialPort->setParity(QSerialPort::EvenParity);
        break;
    default:
        serialPort->setParity(QSerialPort::NoParity);
        break;
    }
    
    // 设置数据位
    switch (dataBits) {
    case 0: // 8
        serialPort->setDataBits(QSerialPort::Data8);
        break;
    case 1: // 7
        serialPort->setDataBits(QSerialPort::Data7);
        break;
    case 2: // 6
        serialPort->setDataBits(QSerialPort::Data6);
        break;
    case 3: // 5
        serialPort->setDataBits(QSerialPort::Data5);
        break;
    default:
        serialPort->setDataBits(QSerialPort::Data8);
        break;
    }
    
    // 设置停止位
    switch (stopBits) {
    case 0: // 1
        serialPort->setStopBits(QSerialPort::OneStop);
        break;
    case 1: // 1.5
        serialPort->setStopBits(QSerialPort::OneAndHalfStop);
        break;
    case 2: // 2
        serialPort->setStopBits(QSerialPort::TwoStop);
        break;
    default:
        serialPort->setStopBits(QSerialPort::OneStop);
        break;
    }
    
    // 打开串口
    if (!serialPort->open(QIODevice::ReadWrite)) {
        return false;
    }
    
    return true;
}

bool CommsPage::connectTcpSocket(QTcpSocket *tcpSocket, const QString &ip, const QString &port)
{
    if (!tcpSocket) {
        return false;
    }
    
    // 如果socket已经连接，先断开
    if (tcpSocket->state() == QAbstractSocket::ConnectedState) {
        tcpSocket->disconnectFromHost();
        tcpSocket->waitForDisconnected(1000);
    }
    
    // 连接到服务器
    tcpSocket->connectToHost(ip, port.toInt());
    
    // 等待连接成功
    if (!tcpSocket->waitForConnected(2000)) {
        return false;
    }
    
    return true;
}

// 辅助函数：获取设备相关的UI控件
void CommsPage::getDeviceUIFields(int deviceIndex, QComboBox *&protocolComboBox, QLineEdit *&ipLineEdit, QLineEdit *&portLineEdit,
                                 QComboBox *&serialPortComboBox, QComboBox *&baudrateComboBox, QComboBox *&parityComboBox,
                                 QComboBox *&dataBitsComboBox, QComboBox *&stopBitsComboBox, QLabel *&statusLabel)
{
    protocolComboBox = nullptr;
    ipLineEdit = nullptr;
    portLineEdit = nullptr;
    serialPortComboBox = nullptr;
    baudrateComboBox = nullptr;
    parityComboBox = nullptr;
    dataBitsComboBox = nullptr;
    stopBitsComboBox = nullptr;
    statusLabel = nullptr;
    
    switch (deviceIndex) {
    case AirTightDevice: // 气密仪
        protocolComboBox = ui->protocol;
        ipLineEdit = ui->ipLineEdit;
        portLineEdit = ui->portLineEdit;
        serialPortComboBox = ui->serialPortComboBox;
        baudrateComboBox = ui->baudrateComboBox;
        parityComboBox = ui->parityComboBox;
        dataBitsComboBox = ui->dataBitsComboBox;
        stopBitsComboBox = ui->stopBitsComboBox;
        statusLabel = ui->airtightStatus;
        break;
    case MainBoardDevice: // 主控板
        protocolComboBox = ui->protocol_2;
        ipLineEdit = ui->ipLineEdit_2;
        portLineEdit = ui->portLineEdit_2;
        serialPortComboBox = ui->serialPortComboBox_2;
        baudrateComboBox = ui->baudrateComboBox_2;
        parityComboBox = ui->parityComboBox_2;
        dataBitsComboBox = ui->dataBitsComboBox_2;
        stopBitsComboBox = ui->stopBitsComboBox_2;
        statusLabel = ui->mainboardStatus;
        break;
    case PressureRegulatorDevice: // 调压
        protocolComboBox = ui->protocol_3;
        ipLineEdit = ui->ipLineEdit_3;
        portLineEdit = ui->portLineEdit_3;
        serialPortComboBox = ui->serialPortComboBox_3;
        baudrateComboBox = ui->baudrateComboBox_3;
        parityComboBox = ui->parityComboBox_3;
        dataBitsComboBox = ui->dataBitsComboBox_3;
        stopBitsComboBox = ui->stopBitsComboBox_3;
        statusLabel = ui->pressureStatus;
        break;
    default:
        break;
    }
}

// 辅助函数：保存设备设置到数据库
void CommsPage::saveDeviceSettings(int deviceIndex, const QString &protocol, const QString &ip, const QString &port,
                                 const QString &serialPort, const QString &baudrate, QComboBox *parityComboBox,
                                 QComboBox *dataBitsComboBox, QComboBox *stopBitsComboBox)
{
    // 保存当前设备的设置
    DatabaseManager* dbManager = DatabaseManager::getInstance();
    dbManager->connectDatabase();
    
    QMap<QString, QVariant> params;
    params["protocol"] = protocol;
    params["ip_address"] = ip;
    params["port"] = port.toInt();
    params["serial_port"] = serialPort;
    params["baudrate"] = baudrate.toInt();
    params["parity"] = parityComboBox->currentText();
    params["data_bits"] = dataBitsComboBox->currentText().toInt();
    params["stop_bits"] = stopBitsComboBox->currentText();
    
    if (dbManager->hasCommunicationParams(deviceIndex)) {
        dbManager->updateCommunicationParams(deviceIndex, params);
    } else {
        dbManager->saveCommunicationParams(deviceIndex, params);
    }
}

// 辅助函数：更新设备状态显示
void CommsPage::updateDeviceStatus(int deviceIndex, bool connected, QLabel *statusLabel)
{
    if (!statusLabel) {
        return;
    }
    
    if (connected) {
        statusLabel->setText("已连接");
        statusLabel->setStyleSheet("color: #27ae60; font-weight: 600;");
    } else {
        statusLabel->setText("未连接");
        statusLabel->setStyleSheet("color: #e74c3c; font-weight: 600;");
    }
    
    // 发送连接状态变化信号
    switch (deviceIndex) {
    case AirTightDevice:
        emit airTightConnectionChanged(connected);
        break;
    case MainBoardDevice:
        emit mainBoardConnectionChanged(connected);
        break;
    case PressureRegulatorDevice:
        emit pressureRegulatorConnectionChanged(connected);
        break;
    }
}

void CommsPage::refreshSerialPorts(QComboBox *comboBox)
{
    // 保存当前选择的串口号
    QString currentPort = comboBox->currentText();
    
    // 清空现有列表
    comboBox->clear();
    
    // 获取所有可用串口
    const auto serialPortInfos = QSerialPortInfo::availablePorts();
    
    for (const QSerialPortInfo &info : serialPortInfos) {
        comboBox->addItem(info.portName());
    }
    
    // 如果之前的串口号仍然可用，重新选择它
    int index = comboBox->findText(currentPort);
    if (index >= 0) {
        comboBox->setCurrentIndex(index);
    }
}

void CommsPage::on_refreshSerialPortButton_clicked()
{
    refreshSerialPorts(ui->serialPortComboBox);
}

void CommsPage::on_refreshSerialPortButton_2_clicked()
{
    refreshSerialPorts(ui->serialPortComboBox_2);
}

void CommsPage::on_refreshSerialPortButton_3_clicked()
{
    refreshSerialPorts(ui->serialPortComboBox_3);
}

// 辅助函数：将UI索引转换为串口奇偶校验枚举值
QSerialPort::Parity CommsPage::parityIndexToEnum(int parityIndex)
{
    // UI的parityComboBox选项：0=None，1=Odd，2=Even
    switch (parityIndex) {
    case 0: return QSerialPort::NoParity;
    case 1: return QSerialPort::OddParity;
    case 2: return QSerialPort::EvenParity;
    default:
        qWarning() << "无效的奇偶校验索引:" << parityIndex << "，使用无校验";
        return QSerialPort::NoParity;
    }
}

// 辅助函数：将UI索引转换为串口数据位枚举值
QSerialPort::DataBits CommsPage::dataBitsIndexToEnum(int dataBitsIndex)
{
    // UI的dataBitsComboBox选项：0=8，1=7，2=6，3=5
    switch (dataBitsIndex) {
    case 0: return QSerialPort::Data8;
    case 1: return QSerialPort::Data7;
    case 2: return QSerialPort::Data6;
    case 3: return QSerialPort::Data5;
    default:
        qWarning() << "无效的数据位索引:" << dataBitsIndex << "，使用8位";
        return QSerialPort::Data8;
    }
}

// 辅助函数：将UI索引转换为串口停止位枚举值
QSerialPort::StopBits CommsPage::stopBitsIndexToEnum(int stopBitsIndex)
{
    // UI的stopBitsComboBox选项：0=1，1=1.5，2=2
    switch (stopBitsIndex) {
    case 0: return QSerialPort::OneStop;
    case 2: return QSerialPort::TwoStop;
    case 1:
        qWarning() << "1.5停止位兼容性差，建议改为1或2停止位";
        return QSerialPort::OneStop; // 强制替换为1停止位
    default:
        qWarning() << "无效的停止位索引:" << stopBitsIndex << "，使用1停止位";
        return QSerialPort::OneStop;
    }
}
