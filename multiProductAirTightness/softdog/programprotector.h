#ifndef PROGRAMPROTECTOR_H
#define PROGRAMPROTECTOR_H

#include <QObject>
#include <QTimer>
#include <QMutex>

class DongleManager;

/**
 * @brief 程序保护器 - 定期检查加密狗授权状态
 * 
 * 特点：
 * 1. 使用容错机制，连续多次失败才会终止程序
 * 2. 异步检查，不阻塞主线程
 * 3. 详细的日志记录，便于调试
 */
class ProgramProtector : public QObject
{
    Q_OBJECT

public:
    explicit ProgramProtector(QObject *parent = nullptr);
    ~ProgramProtector();

    // 初始化保护机制
    bool initialize();

    // 启动定期保护检查
    void startPeriodicChecks(int intervalMs = 60000);  // 默认60秒检查一次

    // 停止定期保护检查
    void stopPeriodicChecks();

    // 检查程序完整性（单次检查）
    bool checkProgramIntegrity();
    
    // 设置最大允许连续失败次数
    void setMaxFailures(int count);
    
    // 获取当前连续失败次数
    int getCurrentFailureCount() const;

signals:
    // 授权检查失败信号（可用于UI提示）
    void authorizationFailed(const QString &reason);
    
    // 授权检查成功信号
    void authorizationSuccess();
    
    // 程序即将终止信号
    void programTerminating();

private slots:
    // 定期检查槽函数
    void onPeriodicCheck();

private:
    // 检查是否存在授权的加密狗（带失败原因）
    bool checkDongleAuthorization(QString &failureReason);
    
    // 检查是否存在授权的加密狗（兼容旧接口）
    bool checkDongleAuthorization();
    
    // 终止程序
    void terminateProgram(const QString &reason);

    QTimer *m_checkTimer;           // 定时器
    DongleManager *m_dongleManager; // 加密狗管理器
    int m_failureCount;             // 连续失败次数
    int m_maxFailures;              // 最大允许连续失败次数
    bool m_initialized;             // 是否已初始化
    QMutex m_mutex;                 // 线程安全锁
    QString m_lastFailureReason;    // 最后一次失败原因
};

#endif // PROGRAMPROTECTOR_H
