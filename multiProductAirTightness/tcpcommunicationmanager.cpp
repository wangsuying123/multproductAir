#include "tcpcommunicationmanager.h"
#include "logmanager.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

TcpCommunicationManager::TcpCommunicationManager(QObject *parent)
    : QObject(parent),
      m_tcpServerManager(nullptr),
      m_serverPort(TCP_SERVER_DEFAULT_PORT),
      m_currentStatus(Status_Idle),
      m_currentPressure(0.0f),
      m_currentLeakValue(0.0f)
{
}

TcpCommunicationManager::~TcpCommunicationManager()
{
    stopServer();
    delete m_tcpServerManager;
}

void TcpCommunicationManager::initialize()
{
    if (!m_tcpServerManager) {
        m_tcpServerManager = new TcpServerManager(this);
        
        // 连接TCP服务器信号到内部槽函数
        connect(m_tcpServerManager, &TcpServerManager::clientConnected, this, &TcpCommunicationManager::onTcpClientConnected);
        connect(m_tcpServerManager, &TcpServerManager::clientDisconnected, this, &TcpCommunicationManager::onTcpClientDisconnected);
        connect(m_tcpServerManager, &TcpServerManager::dataReceived, this, &TcpCommunicationManager::onTcpDataReceived);
        connect(m_tcpServerManager, &TcpServerManager::errorOccurred, this, &TcpCommunicationManager::onTcpErrorOccurred);
        connect(m_tcpServerManager, &TcpServerManager::serverStarted, this, &TcpCommunicationManager::serverStarted);
        connect(m_tcpServerManager, &TcpServerManager::serverStopped, this, &TcpCommunicationManager::serverStopped);
    }
}

bool TcpCommunicationManager::startServer()
{
    if (!m_tcpServerManager) {
        initialize();
    }
    
    return m_tcpServerManager->startServer(m_serverPort);
}

int TcpCommunicationManager::getServerPort() const
{
    return m_serverPort;
}

void TcpCommunicationManager::setServerPort(int port)
{
    if (port > 0 && port <= 65535) {
        m_serverPort = port;
    }
}

void TcpCommunicationManager::stopServer()
{
    if (m_tcpServerManager) {
        m_tcpServerManager->stopServer();
    }
}

bool TcpCommunicationManager::sendParams(const QMap<QString, QVariant> &params)
{
    if (!m_tcpServerManager || !m_tcpServerManager->isRunning()) {
        LOG_WARNING("TCP服务器未运行，无法发送参数", "TCP通信");
        return false;
    }
    
    return m_tcpServerManager->sendParams(params);
}

bool TcpCommunicationManager::isServerRunning() const
{
    if (!m_tcpServerManager) {
        return false;
    }
    
    return m_tcpServerManager->isRunning();
}

int TcpCommunicationManager::getClientCount() const
{
    if (!m_tcpServerManager) {
        return 0;
    }
    
    return m_tcpServerManager->getClientCount();
}

// 处理TCP服务器信号
void TcpCommunicationManager::onTcpClientConnected(QTcpSocket *client)
{
    LOG_INFO(QString("TCP客户端连接: %1:%2").arg(client->peerAddress().toString()).arg(client->peerPort()), "TCP通信");
    emit clientConnected(client);
}

void TcpCommunicationManager::onTcpClientDisconnected(QTcpSocket *client)
{
    LOG_INFO(QString("TCP客户端断开连接: %1:%2").arg(client->peerAddress().toString()).arg(client->peerPort()), "TCP通信");
    emit clientDisconnected(client);
}

void TcpCommunicationManager::onTcpDataReceived(QTcpSocket *client, const QByteArray &data)
{
    LOG_DEBUG(QString("从客户端 %1 接收数据: %2").arg(client->peerAddress().toString()).arg(QString(data)), "TCP通信");
    emit dataReceived(client, data);
}

void TcpCommunicationManager::onTcpErrorOccurred(const QString &errorMsg)
{
    LOG_ERROR(QString("TCP服务器错误: %1").arg(errorMsg), "TCP通信");
    emit errorOccurred(errorMsg);
}

// 序列化参数为JSON字符串（完整版）
QString TcpCommunicationManager::packParamsData(const QString& programNumber, const QString& fillTime, const QString& stabilizationTime, const QString& testTime, 
                                        const QString& dumpTime, const QString& leakThreshold, const QString& fillPressure)
{
    QJsonObject json;
    json["type"] = "params"; // 数据类型：参数推送
    json["program_number"] = programNumber;     // 程序号
    json["fill_time"] = fillTime;              // 填充时间
    json["stabilization_time"] = stabilizationTime; // 稳压时间
    json["test_time"] = testTime;              // 测试时间
    json["dump_time"] = dumpTime;              // 排气时间
    json["leak_threshold"] = leakThreshold;    // 泄露阈值
    json["fill_pressure"] = fillPressure;      // 填充压力
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"); // 时间戳

    // 转换为JSON字符串
    QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Compact);
}

// 序列化完整气密参数为JSON字符串
QString TcpCommunicationManager::packFullParamsData(const QMap<QString, QVariant>& params)
{
    QJsonObject json;
    json["type"] = "full_params"; // 数据类型：完整参数推送
    
    // 基本信息
    json["param_name"] = params.value("param_name").toString();           // 参数名称
    json["program_number"] = params.value("program_number").toInt();      // 程序号
    
    // 时间参数
    json["fill_time"] = params.value("fill_time").toDouble();             // 填充时间(秒)
    json["stabilization_time"] = params.value("stabilization_time").toDouble(); // 稳压时间(秒)
    json["test_time"] = params.value("test_time").toDouble();             // 测试时间(秒)
    json["dump_time"] = params.value("dump_time").toDouble();             // 排气时间(秒)
    
    // 压力参数
    json["pressure_unit"] = params.value("pressure_unit").toInt();        // 压力单位
    json["pressure_max"] = params.value("pressure_max").toDouble();       // 压力上限
    json["pressure_min"] = params.value("pressure_min").toDouble();       // 压力下限
    json["pressure_set_fill"] = params.value("pressure_set_fill").toDouble(); // 填充压力设定值
    json["fill_type"] = params.value("fill_type").toInt();                // 填充类型
    
    // 泄漏参数
    json["leak_unit"] = params.value("leak_unit").toInt();                // 泄漏单位
    json["leak_unit2"] = params.value("leak_unit2").toInt();              // 泄漏单位2
    json["test_reject"] = params.value("test_reject").toDouble();         // 测试拒绝阈值
    json["ref_reject"] = params.value("ref_reject").toDouble();           // 参考拒绝阈值
    json["offset"] = params.value("offset").toDouble();                   // 偏移量
    
    // 环境参数
    json["std_atm"] = params.value("std_atm").toDouble();                 // 标准大气压
    json["std_temp"] = params.value("std_temp").toDouble();               // 标准温度
    
    // 容积参数
    json["volume"] = params.value("volume").toDouble();                   // 容积
    json["volume_unit"] = params.value("volume_unit").toInt();            // 容积单位
    json["reject_calc"] = params.value("reject_calc").toInt();            // 拒绝计算方式
    
    // 时间戳
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Compact);
}

// 序列化测试结果为JSON字符串
QString TcpCommunicationManager::packTestResultData(const TestResult& result)
{
    QJsonObject json;
    json["type"] = "test_result"; // 数据类型：测试结果
    json["program_number"] = result.programNumber;    // 程序号
    json["channel_pressure"] = result.channelPressure; // 通道压力
    json["pressure_unit"] = result.pressureUnit;       // 压力单位
    json["channel_leak"] = result.channelLeak;         // 通道泄漏值
    json["leak_unit"] = result.leakUnit;               // 泄漏单位
    json["is_passed"] = result.isPassed;               // 通过状态
    json["is_failed"] = result.isFailed;               // 不通过状态
    json["create_time"] = result.createTime.toString("yyyy-MM-dd hh:mm:ss"); // 创建时间
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"); // 发送时间戳

    // 转换为JSON字符串
    QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Compact);
}

QString TcpCommunicationManager::packStatusData(DeviceStatus status, float pressure, const QString& pressureUnit, 
                                        float leakValue, const QString& leakUnit, 
                                        const QString& testProcess, const QString& errorMsg)
{
    QJsonObject json;
    json["type"] = "status"; // 数据类型：状态上报
    // 状态转换为字符串（易读）
    switch (status) {
    case Status_Idle: json["device_status"] = "idle"; break;
    case Status_Running: json["device_status"] = "running"; break;
    case Status_Error: json["device_status"] = "error"; break;
    case Status_Paused: json["device_status"] = "paused"; break;
    }
    json["pressure"] = pressure;               // 气密仪压力值
    json["pressure_unit"] = pressureUnit;      // 压力单位
    json["leak_value"] = leakValue;            // 当前泄露值
    json["leak_unit"] = leakUnit;              // 泄露单位
    json["test_process"] = testProcess;        // 当前测试进程
    json["error_msg"] = errorMsg;              // 错误信息
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"); // 时间戳

    // 转换为JSON字符串
    QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Compact);
}

void TcpCommunicationManager::sendDeviceStatus(DeviceStatus status, float pressure, const QString& pressureUnit, 
                                        float leakValue, const QString& leakUnit, 
                                        const QString& testProcess, const QString& errorMsg)
{
    // 更新缓存状态
    m_currentStatus = status;
    m_currentPressure = pressure;
    m_currentPressureUnit = pressureUnit;
    m_currentLeakValue = leakValue;
    m_currentLeakUnit = leakUnit;
    m_currentTestProcess = testProcess;
    m_currentErrorMsg = errorMsg;

    // 序列化状态为JSON字符串
    QString statusData = packStatusData(status, pressure, pressureUnit, 
                                       leakValue, leakUnit, 
                                       testProcess, errorMsg);
    
    // 发送给所有客户端
    sendDataToAllClients(statusData);
}

// 推送设备参数给所有连接的客户端
void TcpCommunicationManager::sendDeviceParams(const QString& programNumber, const QString& fillTime, const QString& stabilizationTime, const QString& testTime, 
                                       const QString& dumpTime,  const QString& leakThreshold, const QString& fillPressure)
{
    // 序列化参数为JSON字符串
    QString paramsData = packParamsData(programNumber, fillTime, stabilizationTime, testTime, 
                                       dumpTime, leakThreshold, fillPressure);
    
    // 发送给所有客户端
    sendDataToAllClients(paramsData);
}

// 推送测试结果给所有连接的客户端
void TcpCommunicationManager::sendTestResult(const TestResult& result)
{
    // 序列化测试结果为JSON字符串
    QString resultData = packTestResultData(result);
    
    // 发送给所有客户端
    sendDataToAllClients(resultData);
}

// 推送完整气密参数给所有连接的客户端
void TcpCommunicationManager::sendFullParams(const QMap<QString, QVariant>& params)
{
    // 序列化完整参数为JSON字符串
    QString paramsData = packFullParamsData(params);
    
    // 发送给所有客户端
    sendDataToAllClients(paramsData);
    
    LOG_INFO(QString("已发送完整气密参数，程序号: %1").arg(params.value("program_number").toInt()), "TCP通信");
}

// 推送实时数据给所有连接的客户端
void TcpCommunicationManager::sendRealtimeData(double pressure, double leak, const QString& pressureUnit, 
                                               const QString& leakUnit, const QString& processName, int programNumber)
{
    // 序列化实时数据为JSON字符串
    QString realtimeData = packRealtimeData(pressure, leak, pressureUnit, leakUnit, processName, programNumber);
    
    // 发送给所有客户端
    sendDataToAllClients(realtimeData);
}

// 序列化实时数据为JSON字符串
QString TcpCommunicationManager::packRealtimeData(double pressure, double leak, const QString& pressureUnit, 
                                                  const QString& leakUnit, const QString& processName, int programNumber)
{
    QJsonObject json;
    json["type"] = "realtime_data";           // 数据类型：实时数据
    json["program_number"] = programNumber;    // 程序号
    json["pressure"] = pressure;               // 当前压力值
    json["pressure_unit"] = pressureUnit;      // 压力单位
    json["leak"] = leak;                       // 当前泄漏值
    json["leak_unit"] = leakUnit;              // 泄漏单位
    json["process_name"] = processName;        // 当前测试进程名称
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"); // 精确时间戳

    QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Compact);
}

// 序列化设备连接状态为JSON字符串
QString TcpCommunicationManager::packConnectionStatusData(bool airTightConnected, bool mainBoardConnected, bool pressureRegulatorConnected)
{
    QJsonObject json;
    json["type"] = "connection_status";        // 数据类型：设备连接状态
    json["airtight_connected"] = airTightConnected;           // 气密仪连接状态
    json["mainboard_connected"] = mainBoardConnected;         // 主控板连接状态
    json["pressure_regulator_connected"] = pressureRegulatorConnected; // 调压装置连接状态
    json["all_connected"] = airTightConnected && mainBoardConnected && pressureRegulatorConnected; // 全部连接
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Compact);
}

// 推送设备连接状态给所有连接的客户端
void TcpCommunicationManager::sendConnectionStatus(bool airTightConnected, bool mainBoardConnected, bool pressureRegulatorConnected)
{
    QString statusData = packConnectionStatusData(airTightConnected, mainBoardConnected, pressureRegulatorConnected);
    sendDataToAllClients(statusData);
}

// 序列化测试统计数据为JSON字符串
QString TcpCommunicationManager::packTestStatisticsData(int totalCount, int passCount, int failCount, double passRate)
{
    QJsonObject json;
    json["type"] = "test_statistics";          // 数据类型：测试统计
    json["total_count"] = totalCount;          // 总测试次数
    json["pass_count"] = passCount;            // 合格次数
    json["fail_count"] = failCount;            // 不合格次数
    json["pass_rate"] = passRate;              // 合格率 (0-100)
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Compact);
}

// 推送测试统计数据给所有连接的客户端
void TcpCommunicationManager::sendTestStatistics(int totalCount, int passCount, int failCount, double passRate)
{
    QString statsData = packTestStatisticsData(totalCount, passCount, failCount, passRate);
    sendDataToAllClients(statsData);
}

// 序列化运行参数为JSON字符串
QString TcpCommunicationManager::packRunningParamsData(int programNumber, int fillTime, int stabilizationTime, int testTime, 
                                                       int dumpTime, double fillPressure, const QString& pressureUnit)
{
    QJsonObject json;
    json["type"] = "running_params";           // 数据类型：运行参数
    json["program_number"] = programNumber;    // 程序号
    json["fill_time"] = fillTime;              // 填充时间(秒)
    json["stabilization_time"] = stabilizationTime; // 稳压时间(秒)
    json["test_time"] = testTime;              // 测试时间(秒)
    json["dump_time"] = dumpTime;              // 排气时间(秒)
    json["fill_pressure"] = fillPressure;      // 填充压力
    json["pressure_unit"] = pressureUnit;      // 压力单位
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Compact);
}

// 推送运行参数给所有连接的客户端
void TcpCommunicationManager::sendRunningParams(int programNumber, int fillTime, int stabilizationTime, int testTime, 
                                                int dumpTime, double fillPressure, const QString& pressureUnit)
{
    QString paramsData = packRunningParamsData(programNumber, fillTime, stabilizationTime, testTime, 
                                               dumpTime, fillPressure, pressureUnit);
    sendDataToAllClients(paramsData);
}

// 发送数据给所有客户端
void TcpCommunicationManager::sendDataToAllClients(const QString& data)
{
    if (!m_tcpServerManager || !m_tcpServerManager->isRunning()) {
        return;
    }
    
    // 直接发送JSON数据，添加换行符作为消息分隔
    QByteArray rawData = data.toUtf8();
    rawData.append('\n');
    m_tcpServerManager->sendRawData(rawData);
}

// 发送测试数据（用于验证TCP连接）
void TcpCommunicationManager::sendTestData()
{
    if (!m_tcpServerManager || !m_tcpServerManager->isRunning()) {
        LOG_WARNING("TCP服务器未运行，无法发送测试数据", "TCP通信");
        return;
    }
    
    QJsonObject json;
    json["type"] = "test_data";
    json["message"] = "TCP连接测试成功";
    json["server_port"] = m_serverPort;
    json["client_count"] = m_tcpServerManager->getClientCount();
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    QJsonDocument doc(json);
    QString testData = doc.toJson(QJsonDocument::Compact);
    
    sendDataToAllClients(testData);
    LOG_INFO(QString("已发送测试数据到 %1 个客户端").arg(m_tcpServerManager->getClientCount()), "TCP通信");
}
