#ifndef AIRTIGHTPARAMSETTING_H
#define AIRTIGHTPARAMSETTING_H

#include <QWidget>
#include <QModbusClient>
#include <QTimer>
#include <QStackedWidget>
#include <QFrame>
#include <QPushButton>
#include <QLabel>

QT_BEGIN_NAMESPACE
namespace Ui {
class AirtightParamSetting;
}
QT_END_NAMESPACE

class AirtightParamSetting;
class MainControlSetting;
class RealTimeMonitor;

class AirtightParamSetting : public QWidget
{
    Q_OBJECT

public:
    explicit AirtightParamSetting(QWidget *parent = nullptr);
    ~AirtightParamSetting();

    // 设置Modbus客户端（气密仪）
    void setModbusClient(QModbusClient *client);
    
    // 设置调压装置Modbus客户端
    void setPressureRegulatorModbusClient(QModbusClient *client);
    
    // 设置主控板Modbus客户端
    void setMainBoardModbusClient(QModbusClient *client);
    
    // 设置从站地址
    void setSlaveId(quint8 id);
    
    // 设置调压装置从站地址
    void setPressureRegulatorSlaveId(quint8 id);
    
    // 设置主控板从站地址
    void setMainBoardSlaveId(quint8 id);
    
    // 设置主控设置页面指针
    void setMainControlSetting(MainControlSetting *mainControlSetting);
    
    // 设置实时监控页面指针
    void setRealTimeMonitor(RealTimeMonitor *monitor);

    // 辅助函数：编码时间参数
    static quint16 encodeTimeParameter(double timeValue);

    // 辅助函数：编码压力参数
    static QPair<quint16, quint16> encodePressureParameter(quint16 pressureValue);
 signals:
    // 程序号发送信号
    void programNumberSent(int programNumber);
    // 参数保存成功信号
    void parametersSaved(int programNumber);
    // 检测到0006寄存器为1，即将启动测试信号
    void testStarted();
    // 通道测试完成信号
    void channelTestCompleted(int channel, bool success);
    
public slots:
    // 连接状态变化槽函数
    void onMainBoardConnectionChanged(bool connected);
    void onAirTightConnectionChanged(bool connected);
    void onPressureRegulatorConnectionChanged(bool connected);
    
    // 参数保存和加载槽函数
    void onSaveParametersButtonClicked();
    void onProgramNumberChanged(int index);
    void onSendToDeviceButtonClicked();
    
    // 加载参数方法
    void loadParameters(int programNumber);
    
    // 测试完成处理槽函数（添加通道参数）
    void onTestCompleted(int channel, bool success);
    
    // 启动下一个通道测试
    void startNextChannelTest();

private:
    // 分页相关方法
    void initializePaginatedUI();
    QWidget* createPage1();
    QWidget* createPage2();
    QWidget* createPage3();
    QFrame* createNavigationBar();
    void switchToPage(int pageIndex);
    void updateNavigationButtons();
    void updatePageIndicator();

private slots:
    void onFirstButtonClicked();
    void onPrevButtonClicked();
    void onNextButtonClicked();
    void onLastButtonClicked();

private:
    // 保存参数方法
    void saveParameters(int programNumber);
    
    // 发送单个Modbus命令
    bool sendModbusCommand(quint16 address, quint16 value, int timeoutMs);
    
    // 向主控板发送Modbus命令
    bool sendMainBoardCommand(quint16 address, quint16 value, int timeoutMs);
    
    // 读取Modbus寄存器
    bool readModbusRegister(quint16 address, quint16 &value, int timeoutMs);
    
    // 辅助函数：检查设备连接状态
    bool checkDeviceConnection();
    
    // 辅助函数：准备Modbus写入数据单元
    QModbusDataUnit prepareWriteDataUnit();
    
    // 辅助函数：发送批量写入请求
    bool sendBatchWriteRequest(const QModbusDataUnit &writeUnit, int timeoutMs);

    // 发送压力值到调压装置
    bool sendPressureToRegulator(quint16 pressureValue, int timeoutMs);
    
    // 获取上次发送的填充压力值
    quint16 getLastSentFillPressure() const;
    
    // 获取当前程序号
    int getCurrentProgramNumber();
    
    // 获取所有开启的通道列表
    QList<int> getEnabledChannels();
    
    // 根据通道号获取程序号
    int getProgramNumberForChannel(int channel);
    
    // 校验参数有效性
    bool validateParams(const QMap<QString, QVariant> &params, int channel);
    
    // 发送参数到设备
    bool sendParamsToDevice(const QMap<QString, QVariant> &params);
    
    // 实时检测主控板寄存器
    void realTimeRegisterDetection();
    
    // 启动气密仪
    bool startAirtightTest();
    
    // 复位气密仪
    void resetAirtightTest();
    
    // 重置气密仪设备状态（通道切换前调用）
    void resetAirtightDeviceState();
    
    // 等待气密仪进入待机状态
    void waitForDeviceStandby();
    
    // 等待设备进入测试状态
    void waitForTestStart();
    
    Ui::AirtightParamSetting *ui;
    bool m_airTightConnected; // 保存气密仪连接状态
    bool m_pressureRegulatorConnected; // 保存调压装置连接状态
    bool m_mainBoardConnected; // 保存主控板连接状态
    QModbusClient *modbusClient; // 气密仪Modbus客户端指针
    QModbusClient *m_pressureRegulatorModbusClient; // 调压装置Modbus客户端指针
    QModbusClient *m_mainBoardModbusClient; // 主控板Modbus客户端指针
    quint8 slaveId; // 气密仪从站地址
    quint8 m_pressureRegulatorSlaveId; // 调压装置从站地址
    quint8 m_mainBoardSlaveId; // 主控板从站地址
    quint16 m_lastSentFillPressure; // 保存上次发送的填充压力值
    QTimer *detectionTimer; // 实时检测定时器
    MainControlSetting *m_mainControlSetting; // 主控设置页面指针
    RealTimeMonitor *m_realTimeMonitor; // 实时监控页面指针
    
    // 分页相关成员变量
    QStackedWidget* m_pageStack;        // 页面堆栈容器
    QFrame* m_navigationFrame;          // 导航栏框架
    QPushButton* m_firstButton;         // 首页按钮
    QPushButton* m_prevButton;          // 上一页按钮
    QPushButton* m_nextButton;          // 下一页按钮
    QPushButton* m_lastButton;          // 末页按钮
    QLabel* m_pageIndicator;            // 页码指示器
    int m_currentPage;                  // 当前页码（0-based）
    int m_totalPages;                   // 总页数
    
    // 页面widget指针
    QWidget* m_page1Widget;
    QWidget* m_page2Widget;
    QWidget* m_page3Widget;
    
    // 常量定义
    static constexpr quint16 MODBUS_START_ADDRESS = 8192;        // Modbus起始地址
    static constexpr quint16 MODBUS_END_ADDRESS = 8229;          // Modbus结束地址
    static constexpr int MODBUS_WRITE_TIMEOUT = 1000;           // Modbus写入超时时间（毫秒）
    static constexpr int DEVICE_PREPARE_WAIT_TIME = 500;        // 设备准备等待时间（毫秒）
    static constexpr quint16 DEVICE_PREPARE_COMMAND_VALUE = 26112; // 设备准备命令值
    static constexpr quint16 STANDARD_ATMOSPHERE_VALUE = 32068;  // 标准大气压值
    static constexpr quint16 TEST_REJECT_THRESHOLD = 256;        // 测试拒绝阈值
    static constexpr quint16 FILL_TYPE_DEFAULT = 0;              // 默认填充类型
    static constexpr quint16 REJECT_CALCULATION_DEFAULT = 0;    // 默认拒绝计算方式
    
    // 多通道测试相关成员
    bool m_isMultiChannelTesting;      // 是否正在进行多通道测试
    QList<int> m_enabledChannels;      // 开启的通道列表
    int m_currentChannelIndex;         // 当前正在测试的通道索引
    int m_currentTestingChannel;       // 当前正在测试的通道号

    // 防抖相关成员
    bool m_resetInProgress;            // 复位操作是否正在进行（防抖标志）
    QTimer *m_resetDebounceTimer;      // 复位防抖定时器
};

#endif // AIRTIGHTPARAMSETTING_H
