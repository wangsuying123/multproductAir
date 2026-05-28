#ifndef TCPCOMMUNICATIONMANAGER_H
#define TCPCOMMUNICATIONMANAGER_H

#include <QObject>
#include <QDateTime>
#include <QString>
#include "TcpServerManager.h"

// 测试结果结构体
struct TestResult {
    int programNumber;          // 程序号
    float channelPressure;      // 通道压力
    QString pressureUnit;       // 压力单位
    float channelLeak;          // 通道泄漏值
    QString leakUnit;           // 泄漏单位
    bool isPassed;              // 是否通过
    bool isFailed;              // 是否失败
    QDateTime createTime;       // 创建时间
};

// 设备运行状态枚举（根据实际业务扩展）
enum DeviceStatus {
    Status_Idle,       // 空闲
    Status_Running,    // 运行中
    Status_Error,      // 故障
    Status_Paused      // 暂停
};

class TcpCommunicationManager : public QObject
{
    Q_OBJECT

public:
    explicit TcpCommunicationManager(QObject *parent = nullptr);
    ~TcpCommunicationManager();
    
    // 初始化TCP通信
    void initialize();
    
    // 启动TCP服务器
    bool startServer();
    
    // 停止TCP服务器
    void stopServer();
    
    // 发送参数给所有连接的客户端
    bool sendParams(const QMap<QString, QVariant> &params);

    // 主动推送设备状态给所有连接的客户端
    void sendDeviceStatus(DeviceStatus status, float pressure, const QString& pressureUnit, 
                          float leakValue, const QString& leakUnit, 
                          const QString& testProcess, const QString& errorMsg = "");
    // 推送设备参数给所有连接的客户端
    void sendDeviceParams(const QString& programNumber, const QString& fillTime, const QString& stabilizationTime, const QString& testTime, 
                         const QString& dumpTime, const QString& leakThreshold, const QString& fillPressure);

    // 推送测试结果给所有连接的客户端
    void sendTestResult(const TestResult& result);
    
    // 推送完整气密参数给所有连接的客户端
    void sendFullParams(const QMap<QString, QVariant>& params);
    
    // 推送实时数据给所有连接的客户端
    void sendRealtimeData(double pressure, double leak, const QString& pressureUnit, 
                          const QString& leakUnit, const QString& processName, int programNumber);
    
    // 推送设备连接状态给所有连接的客户端
    void sendConnectionStatus(bool airTightConnected, bool mainBoardConnected, bool pressureRegulatorConnected);
    
    // 推送测试统计数据给所有连接的客户端
    void sendTestStatistics(int totalCount, int passCount, int failCount, double passRate);
    
    // 推送运行参数给所有连接的客户端（实时监控页面显示的参数）
    void sendRunningParams(int programNumber, int fillTime, int stabilizationTime, int testTime, 
                           int dumpTime, double fillPressure, const QString& pressureUnit);
    
    // 发送测试数据（用于验证TCP连接）
    void sendTestData();

    // 检查服务器是否正在运行
    bool isServerRunning() const;
    
    // 获取当前连接的客户端数量
    int getClientCount() const;
    
    // 获取当前服务器端口
    int getServerPort() const;
    
    // 设置服务器端口
    void setServerPort(int port);

 signals:
    // 客户端连接状态变化信号
    void clientConnected(QTcpSocket *client);
    void clientDisconnected(QTcpSocket *client);
    
    // 服务器状态变化信号
    void serverStarted(int port);
    void serverStopped();
    
    // 数据接收信号
    void dataReceived(QTcpSocket *client, const QByteArray &data);
    
    // 错误信号
    void errorOccurred(const QString &errorMsg);

private slots:
    // 处理TCP服务器信号
    void onTcpClientConnected(QTcpSocket *client);
    void onTcpClientDisconnected(QTcpSocket *client);
    void onTcpDataReceived(QTcpSocket *client, const QByteArray &data);
    void onTcpErrorOccurred(const QString &errorMsg);

private:
    // TCP服务器管理器
    TcpServerManager *m_tcpServerManager;
    
    // 当前使用的端口
    int m_serverPort;
    
    // 当前设备状态缓存
    DeviceStatus m_currentStatus;
    float m_currentPressure;
    QString m_currentPressureUnit;
    float m_currentLeakValue;
    QString m_currentLeakUnit;
    QString m_currentTestProcess;
    QString m_currentErrorMsg;

    // 序列化状态为JSON字符串
    QString packStatusData(DeviceStatus status, float pressure, const QString& pressureUnit, 
                          float leakValue, const QString& leakUnit,
                          const QString& testProcess, const QString& errorMsg);
    // 序列化参数为JSON字符串
    QString packParamsData(const QString& programNumber, const QString& fillTime, const QString& stabilizationTime, const QString& testTime, 
                          const QString& dumpTime, const QString& leakThreshold, const QString& fillPressure);
    
    // 序列化完整气密参数为JSON字符串
    QString packFullParamsData(const QMap<QString, QVariant>& params);
    
    // 序列化实时数据为JSON字符串
    QString packRealtimeData(double pressure, double leak, const QString& pressureUnit, 
                            const QString& leakUnit, const QString& processName, int programNumber);
    
    // 序列化设备连接状态为JSON字符串
    QString packConnectionStatusData(bool airTightConnected, bool mainBoardConnected, bool pressureRegulatorConnected);
    
    // 序列化测试统计数据为JSON字符串
    QString packTestStatisticsData(int totalCount, int passCount, int failCount, double passRate);
    
    // 序列化运行参数为JSON字符串
    QString packRunningParamsData(int programNumber, int fillTime, int stabilizationTime, int testTime, 
                                  int dumpTime, double fillPressure, const QString& pressureUnit);
    
    // 序列化测试结果为JSON字符串
    QString packTestResultData(const TestResult& result);
    
    // 发送数据给所有客户端
    void sendDataToAllClients(const QString& data);
    
    // 常量定义
    static constexpr int TCP_SERVER_DEFAULT_PORT = 8088;        // TCP服务器默认端口
};

#endif // TCPCOMMUNICATIONMANAGER_H
