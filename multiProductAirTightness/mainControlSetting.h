#ifndef MAINCONTROLSETTING_H
#define MAINCONTROLSETTING_H

#include <QWidget>
#include <QModbusClient>

class TcpCommunicationManager;
class RealTimeMonitor;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainControlSetting;
}
QT_END_NAMESPACE

class MainControlSetting : public QWidget
{
    Q_OBJECT

signals:
    void programNumberSent(int programNumber);

public:
    explicit MainControlSetting(QWidget *parent = nullptr);
    ~MainControlSetting();

    // 设置RealTimeMonitor实例
    void setRealTimeMonitor(RealTimeMonitor *monitor);
    
    // 获取当前程序号（返回第一个开启的通道的程序号）
    int getProgramNumber();
    // 获取指定通道的程序号
    int getProgramNumber(int channel);
    // 获取所有开启的通道列表
    QList<int> getEnabledChannels();
    // 检查指定通道是否开启
    bool isChannelEnabled(int channel);

    // 设置Modbus客户端
    void setModbusClient(QModbusClient *client);
    
    // 设置气密仪Modbus客户端
    void setAirtightModbusClient(QModbusClient *client);
    
    // 设置主控板Modbus客户端
    void setMainBoardModbusClient(QModbusClient *client);
    
    // 设置调压装置Modbus客户端
    void setPressureRegulatorModbusClient(QModbusClient *client);
    
    // 设置从站地址
    void setSlaveId(quint8 id);
    
    // 设置主控板从站地址
    void setMainBoardSlaveId(quint8 id);
    
    // 设置调压装置从站地址
    void setPressureRegulatorSlaveId(quint8 id);
    
    // 设置TCP通信管理器实例
    void setTcpCommunicationManager(TcpCommunicationManager *tcpManager);
    
    // 强制刷新主控板连接状态
    void refreshMainBoardConnectionStatus();
    
 
    
public slots:
    // 连接状态变化槽函数
    void onMainBoardConnectionChanged(bool connected);
    void onAirTightConnectionChanged(bool connected);
    void onPressureRegulatorConnectionChanged(bool connected);
    
    // 测试通道控制槽函数
    void onTestChannel1OpenButtonClicked();
    void onTestChannel1CloseButtonClicked();
    void onTestChannel2OpenButtonClicked();
    void onTestChannel2CloseButtonClicked();
    void onTestChannel3OpenButtonClicked();
    void onTestChannel3CloseButtonClicked();
    
    // 程序号下拉框改变槽函数
    void onProgramNumberComboBox1Changed(int index);
    void onProgramNumberComboBox2Changed(int index);
    void onProgramNumberComboBox3Changed(int index);
    
    // 启动气密仪方法
    bool startAirtightTest();
    // 复位气密仪方法
    void resetAirtightTest();

    // 发送单个Modbus命令
    bool sendModbusCommand(quint16 address, quint16 value, int timeoutMs);

private:
    // 读取单个Modbus寄存器
    bool readModbusRegister(quint16 address, quint16 &value, int timeoutMs);
    
    // 发送参数到设备
    bool sendParamsToDevice(const QMap<QString, QVariant> params);
    
    // 发送压力值到调压装置
    bool sendPressureToRegulator(const QMap<QString, QVariant> params);
    
    // 更新通道按钮状态颜色
    void updateChannelButtonStates();
    
    Ui::MainControlSetting *ui;
    bool m_airTightConnected; // 保存气密仪连接状态
    bool m_mainBoardConnected; // 保存主控板连接状态
    bool m_pressureRegulatorConnected; // 保存调压装置连接状态
    bool m_testChannel1; // 测试1通道状态
    bool m_testChannel2; // 测试2通道状态
    bool m_testChannel3; // 测试3通道状态
    QModbusClient *modbusClient; // 默认Modbus客户端指针
    QModbusClient *airtightModbusClient; // 气密仪Modbus客户端指针
    QModbusClient *mainBoardModbusClient; // 主控板Modbus客户端指针
    QModbusClient *pressureRegulatorModbusClient; // 调压装置Modbus客户端指针
    quint8 slaveId; // 从站地址
    quint8 mainBoardSlaveId; // 主控板从站地址
    quint8 pressureRegulatorSlaveId; // 调压装置从站地址
    TcpCommunicationManager *m_tcpCommunicationManager; // TCP通信管理器指针
    RealTimeMonitor *m_realTimeMonitor; // 实时监控页面指针
    int m_programNumber1; // 通道1程序号
    int m_programNumber2; // 通道2程序号
    int m_programNumber3; // 通道3程序号
};

#endif // MAINCONTROLSETTING_H