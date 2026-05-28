#ifndef REALTIMEMONITOR_H
#define REALTIMEMONITOR_H

#include <QWidget>
#include <QTimer>
#include <QtCharts>
#include <QModbusClient>
#include <QModbusDevice>
#include <QModbusDataUnit>
#include <functional>
#include "airtightnessparamsdao.h"
#include "databasemanager.h"
#include "enum/pressureUnit.h"
#include "enum/leakUnit.h"
#include "chartdialog.h"

class MainControlSetting;

namespace Ui {
class RealTimeMonitor;
}
class RealTimeMonitor : public QWidget
{
    Q_OBJECT

signals:
    // 实时数据更新信号
    void realtimeDataUpdated(double pressure, double leak, const QString &pressureUnit, const QString &leakUnit, const QString &processName);
    // 测试结果更新信号（添加通道参数）
    void updateTestResult(int channel, bool success);
    // 日志消息信号
    void logMessage(const QString &message, bool isError = false);
    // 测试结果保存成功信号（用于通知TestResultshow自动刷新）
    void testResultSaved(const QMap<QString, QVariant>& testResult);
    // 测试通道变化信号
    void testChannelChanged(int channel);

public:
    explicit RealTimeMonitor(QWidget *parent = nullptr);
    ~RealTimeMonitor();

public slots:
    // 更新操作人员信息
    void updateOperatorInfo(const QString& username, const QString& role);
    // 接收程序号信号
    void onProgramNumberReceived(int programNumber);
    // 更新当前产品编号显示
    void updateCurrentProductId(const QString& productId);
    // 设置当前测试通道
    void setCurrentTestingChannel(int channel);
    // 更新指定通道的测试结果
    void updateChannelTestResult(int channel, const QString& pressureValue, const QString& leakValue, const QString& result);

private slots:
    void updateTime();
    void updateData();
    void onExpandChartButtonClicked();
    void onChartDialogClosed();

public:
    bool writeDeviceData(quint16 address, quint16 value, QModbusDataUnit::RegisterType type);
    void setAirTightModbusClient(QModbusClient *client);
    void setMainBoardModbusClient(QModbusClient *client);
    void setPressureRegulatorModbusClient(QModbusClient *client);
    void setAirTightSlaveId(int slaveId);
    void setMainBoardSlaveId(int slaveId);
    void setPressureRegulatorSlaveId(int slaveId);
    // 清空当前测试结果
    void clearTestResult();
    // 设置主控设置页面指针
    void setMainControlSetting(MainControlSetting *mainControlSetting);
    // 检查是否允许启动测试（检查必须扫码设置和产品编号）
    bool canStartTest() const;
    // 获取当前产品编号
    QString getProductId() const;
    // 获取是否必须扫码
    bool isScanRequired() const;

private:
    Ui::RealTimeMonitor *ui;
    QTimer *timeTimer;
    QTimer *dataTimer;
    QTimer *testResultTimer; // 测试结果读取定时器

    // 枚举定义测试进程状态
    enum ProcessStatus {
        STANDBY = 0,
        FILL = 256,
        STB = 512,
        TEST = 768,
        DUMP = 1024
    };
    
    // 折线图相关成员
    QChart *chart;
    QLineSeries *leakSeries;
    QLineSeries *pressureSeries;
    QValueAxis *axisX;
    QValueAxis *axisYLeft; // 左边Y轴：压力值
    QValueAxis *axisYRight; // 右边Y轴：泄漏值
    int dataCount;
    
    // Modbus相关成员
    QModbusClient *airTightModbusClient;
    QModbusClient *mainBoardModbusClient;
    QModbusClient *pressureRegulatorModbusClient;
    
    int airTightSlaveId;
    int mainBoardSlaveId;
    int pressureRegulatorSlaveId;
    
    // 状态相关成员
    bool isConnected;
    bool isMonitoring;
    quint16 register8707Value;
    int programNumber; // 当前程序号
    int m_currentTestingChannel; // 当前测试通道（1、2、3）
    
    // 通道测试结果跟踪（用于计算总测试结果）
    QMap<int, bool> channelTestResults; // key: channel(1-3), value: 是否通过
    
    // 当前操作员信息
    QString currentOperatorName;
    QString currentOperatorRole;
    
    // 主控设置页面指针
    MainControlSetting *m_mainControlSetting;
    
    // 产品编号和扫码设置
    QString productId;
    bool scanRequired;
    
    // 数据库相关成员
    AirTightnessParamsDao *m_airTightnessParamsDao;
    
    // ChartDialog 相关成员
    ChartDialog *chartDialog;
    
    // 数据处理相关成员函数
    void readRealTimeData();
    void readTestResultData(); // 读取测试结果数据
    void processMainRegisterData(const QModbusDataUnit &data);
    void readDeviceDataAsync(quint16 address, quint16 count, QModbusDataUnit::RegisterType type, const std::function<void(bool, const QModbusDataUnit&)> &callback, int retryCount = 0);
    void processTestResultData(const QModbusDataUnit &data);
    
    // 辅助函数
    void updateTestStatus(int processValue);
    // 更新测试结果汇总统计
    void updateTestResultSummary();
    // 向主控板发送Modbus命令
    bool sendMainBoardCommand(quint16 address, quint16 value, int timeoutMs = 500);
    // 计算并更新总测试结果
    void calculateAndUpdateTotalResult();
};

#endif // REALTIMEMONITOR_H
