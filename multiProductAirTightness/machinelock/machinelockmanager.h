#ifndef MACHINELOCKMANAGER_H
#define MACHINELOCKMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include "machinefingerprint.h"

/**
 * @brief 机器锁定管理器 - 管理程序与机器的绑定
 * 
 * 特点：
 * 1. 程序只能在授权的机器上运行
 * 2. 复制到其他机器无法使用
 * 3. 支持永久授权和限时授权
 * 4. 授权信息加密存储
 */
class MachineLockManager : public QObject
{
    Q_OBJECT

public:
    explicit MachineLockManager(QObject *parent = nullptr);
    ~MachineLockManager();

    // 验证当前机器是否已授权
    bool verifyMachineLicense();
    
    // 生成授权文件（用于授权工具）
    bool generateLicenseFile(const QString &filePath, const QDateTime &startTime = QDateTime(), const QDateTime &endTime = QDateTime());
    
    // 读取授权文件
    bool loadLicenseFile(const QString &filePath);
    
    // 获取当前机器指纹
    QString getCurrentMachineFingerprint();
    
    // 获取授权的机器指纹
    QString getAuthorizedFingerprint() const { return m_authorizedFingerprint; }
    
    // 获取授权信息
    QString getLicenseInfo();
    
    // 检查授权是否过期
    bool isLicenseExpired();
    
    // 获取授权剩余天数（-1表示永久授权）
    int getRemainingDays();

private:
    // 检查系统时间是否被篡改
    bool checkTimeIntegrity();
    
    // 记录当前时间
    void recordCurrentTime();
    
    // 读取上次记录的时间
    QDateTime readLastRecordedTime();
    MachineFingerprint *m_fingerprint;
    QString m_authorizedFingerprint;
    QDateTime m_startTime;
    QDateTime m_endTime;
    bool m_isPermanent;
    
    // 加密授权数据
    QString encryptLicenseData(const QString &data);
    
    // 解密授权数据
    QString decryptLicenseData(const QString &encryptedData);
    
    // 生成加密密钥
    QByteArray generateKey();
    
    // AES加密
    QByteArray encryptAES(const QByteArray &data, const QByteArray &key, const QByteArray &iv);
    
    // AES解密
    QByteArray decryptAES(const QByteArray &encryptedData, const QByteArray &key, const QByteArray &iv);
    
    // 生成随机IV
    QByteArray generateRandomIV();
};

#endif // MACHINELOCKMANAGER_H
