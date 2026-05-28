#ifndef COMMSPAGE_H
#define COMMSPAGE_H

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSerialPort>
#include <QTcpSocket>
#include <QTimer>
#include <QModbusClient>
#include <QModbusTcpClient>
#include <QModbusRtuSerialClient>
#include <QLabel>
#include "databasemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class CommsPage;
}
QT_END_NAMESPACE

// 设备信息结构体
struct DeviceInfo {
    bool connected;              // 连接状态
    bool autoReconnectEnabled;   // 自动重连开关
    quint8 slaveId;              // 从站ID
    QModbusClient *modbusClient; // Modbus客户端对象
    QSerialPort *serialPort;     // 串口连接对象
    QTcpSocket *tcpSocket;       // TCP连接对象
    QTimer *autoReconnectTimer;  // 自动重连定时器
    
    // 构造函数，初始化所有成员
    DeviceInfo() : connected(false), autoReconnectEnabled(true), slaveId(1), 
                  modbusClient(nullptr), serialPort(nullptr), tcpSocket(nullptr), 
                  autoReconnectTimer(nullptr) {}
};

class CommsPage : public QWidget
{
    Q_OBJECT

public:
    explicit CommsPage(QWidget *parent = nullptr);
    ~CommsPage();

 signals:
    // 连接状态变化信号
    void airTightConnectionChanged(bool connected);
    void mainBoardConnectionChanged(bool connected);
    void pressureRegulatorConnectionChanged(bool connected);
    
    // Modbus客户端变化信号
    void airTightModbusClientChanged(QModbusClient *client);
    void mainBoardModbusClientChanged(QModbusClient *client);
    void pressureRegulatorModbusClientChanged(QModbusClient *client);
    
    // 从站ID变化信号
    void airTightSlaveIdChanged(quint8 slaveId);
    void mainBoardSlaveIdChanged(quint8 slaveId);
    void pressureRegulatorSlaveIdChanged(quint8 slaveId);
    
    // 所有设备连接成功信号
    void allDevicesConnected();

public:
    // 获取连接状态
    bool getAirTightConnected() const;
    bool getMainBoardConnected() const;
    bool getPressureRegulatorConnected() const;
    
    // 获取Modbus客户端实例
    QModbusClient *getAirTightModbusClient() const;
    QModbusClient *getMainBoardModbusClient() const;
    QModbusClient *getPressureRegulatorModbusClient() const;
    
    // 获取从站ID
    quint8 getAirTightSlaveId() const;
    quint8 getMainBoardSlaveId() const;
    quint8 getPressureRegulatorSlaveId() const;
    
    // 设置从站ID
    void setAirTightSlaveId(quint8 slaveId);
    void setMainBoardSlaveId(quint8 slaveId);
    void setPressureRegulatorSlaveId(quint8 slaveId);

private slots:
    void on_protocol_currentIndexChanged(int index);
    void on_protocol_2_currentIndexChanged(int index);
    void on_protocol_3_currentIndexChanged(int index);
    void on_connectionButton_clicked();
    void on_connectionButton_2_clicked();
    void on_connectionButton_3_clicked();
    void on_disconnectButton_clicked();
    void on_disconnectButton_2_clicked();
    void on_disconnectButton_3_clicked();
    void on_refreshSerialPortButton_clicked();
    void on_refreshSerialPortButton_2_clicked();
    void on_refreshSerialPortButton_3_clicked();

private:
    Ui::CommsPage *ui;
    
    // 设备信息数组，包含所有设备的信息
    DeviceInfo m_devices[3];
    
    // 设备类型常量
    enum DeviceType {
        AirTightDevice = 0,
        MainBoardDevice = 1,
        PressureRegulatorDevice = 2
    };
    
    // 常量定义
    static constexpr quint8 DEFAULT_SLAVE_ID = 1;               // 默认从站ID
    static constexpr int TCP_CONNECTION_TIMEOUT = 2000;         // TCP连接超时时间（毫秒）
    static constexpr int DISCONNECT_WAIT_TIMEOUT = 1000;       // 断开连接等待时间（毫秒）
    static constexpr int SERIAL_BAUD_RATE_DEFAULT = 9600;      // 默认串口波特率
    static constexpr int MODBUS_WRITE_TIMEOUT = 1000;           // Modbus写入超时时间（毫秒）
    static constexpr int AUTO_RECONNECT_INTERVAL = 5000;        // 自动重连间隔（毫秒）
    
    void updateProtocolSettings();
    void updateProtocolSettings(int deviceIndex);
    void loadSettings();
    void saveSettings();
    void updateButtonStates();
    void connectDevice(int deviceIndex);
    void disconnectDevice(int deviceIndex);
    void refreshSerialPorts(QComboBox *comboBox);
    
    // 辅助函数
    bool connectSerialPort(QSerialPort *serialPort, const QString &portName, const QString &baudrate, int parity, int dataBits, int stopBits);
    bool connectTcpSocket(QTcpSocket *tcpSocket, const QString &ip, const QString &port);
    
    // 辅助函数：获取设备相关的UI控件
    void getDeviceUIFields(int deviceIndex, QComboBox *&protocolComboBox, QLineEdit *&ipLineEdit, QLineEdit *&portLineEdit,
                           QComboBox *&serialPortComboBox, QComboBox *&baudrateComboBox, QComboBox *&parityComboBox,
                           QComboBox *&dataBitsComboBox, QComboBox *&stopBitsComboBox, QLabel *&statusLabel);
    
    // 辅助函数：保存设备设置到数据库
    void saveDeviceSettings(int deviceIndex, const QString &protocol, const QString &ip, const QString &port,
                           const QString &serialPort, const QString &baudrate, QComboBox *parityComboBox,
                           QComboBox *dataBitsComboBox, QComboBox *stopBitsComboBox);
    
    // 辅助函数：更新设备状态显示
    void updateDeviceStatus(int deviceIndex, bool connected, QLabel *statusLabel);
    
    // 辅助函数：将UI索引转换为串口奇偶校验枚举值
    QSerialPort::Parity parityIndexToEnum(int parityIndex);
    
    // 辅助函数：将UI索引转换为串口数据位枚举值
    QSerialPort::DataBits dataBitsIndexToEnum(int dataBitsIndex);
    
    // 辅助函数：将UI索引转换为串口停止位枚举值
    QSerialPort::StopBits stopBitsIndexToEnum(int stopBitsIndex);
};

#endif // COMMSPAGE_H
