#ifndef DONGLEMANAGER_H
#define DONGLEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QStorageInfo>

class DongleManager : public QObject
{
    Q_OBJECT

public:
    explicit DongleManager(QObject *parent = nullptr);
    ~DongleManager();

    // 获取所有可用的U盘
    QList<QStorageInfo> getAvailableUSBStorages();

    // 生成U盘的唯一标识（基于硬件信息）
    QString generateUniqueID(const QStorageInfo &storage);

    // 验证U盘是否为授权的加密狗
    bool isAuthorizedDongle(const QString &dongleId);

    // 初始化U盘作为加密狗（带时间限制）
    bool initializeDongle(const QStorageInfo &storage, const QString &programId, const QDateTime &startTime, const QDateTime &endTime);

    // 初始化U盘作为加密狗（永久授权）
    bool initializeDongle(const QStorageInfo &storage, const QString &programId);

    // 检查程序是否在U盘上运行
    bool isRunningFromUSB();

    // 保护程序不被复制
    bool protectProgram();

    // 获取当前运行程序的路径
    QString getCurrentProgramPath();

    // 检查指定存储设备是否已授权
    bool isStorageAuthorized(const QStorageInfo &storage);

public:
    // 获取程序的唯一标识
    QString getProgramId();

private:
    // 存储授权的加密狗ID
    QList<QString> m_authorizedDongleIds;

    // 上次运行时间缓存
    QDateTime m_lastRunTime;

    // 从配置文件加载授权ID
    void loadAuthorizedIds();

    // 保存授权ID到配置文件
    void saveAuthorizedIds();

    // 保存上次运行时间
    void saveLastRunTime();

    // 加载上次运行时间
    void loadLastRunTime();

    // 读取U盘上的授权文件
    QString readDongleAuthorization(const QStorageInfo &storage);

    // 写入授权信息到U盘
    bool writeDongleAuthorization(const QStorageInfo &storage, const QString &authInfo);

    // 生成基于硬件特征的动态密钥
    QByteArray generateDynamicKey(const QStorageInfo &storage);

    // AES-256加密
    QByteArray encryptAES(const QByteArray &data, const QByteArray &key, const QByteArray &iv);

    // AES-256解密
    QByteArray decryptAES(const QByteArray &encryptedData, const QByteArray &key, const QByteArray &iv);

    // 生成随机IV
    QByteArray generateRandomIV();

    // 生成数字签名
    QByteArray generateSignature(const QByteArray &data, const QByteArray &privateKey);

    // 验证数字签名
    bool verifySignature(const QByteArray &data, const QByteArray &signature, const QByteArray &publicKey);

    // 加密数据（使用AES-256和数字签名）
    QString encryptData(const QString &data);
    QString encryptData(const QString &data, const QStorageInfo &storage);

    // 解密数据（使用AES-256和数字签名）
    QString decryptData(const QString &encryptedData);
    QString decryptData(const QString &encryptedData, const QStorageInfo &storage);

    // 获取U盘的硬件特征（如序列号）
    QString getUSBHardwareInfo(const QStorageInfo &storage);

    // 程序完整性校验
    bool checkProgramIntegrity();

    // 获取网络时间
    QDateTime getNetworkTime();

    // 验证时间是否合理
    bool isTimeValid(const QDateTime &time);

    // 检查时间差是否合理（防止时间回拨）
    bool isTimeDifferenceValid(const QDateTime &currentTime);

    // 获取文件的创建/修改时间
    QDateTime getFileModificationTime(const QString &filePath);

    // 验证文件时间是否合理
    bool isFileTimeValid(const QDateTime &fileTime, const QDateTime &currentTime);
};

#endif // DONGLEMANAGER_H
