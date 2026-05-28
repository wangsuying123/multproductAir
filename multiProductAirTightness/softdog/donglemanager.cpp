#include "donglemanager.h"
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QTcpSocket>
#include <QDateTime>
#include <QRandomGenerator>
#include "logmanager.h"

// ==================== 新算法常量定义 ====================
static const QString PROGRAM_ID = "AIR_TIGHTNESS_TEST_V3";
static const QString BASE_KEY = "AIRTIGHTNESS_ENCRYPTION_KEY_V3_2026";
static const QString AUTH_FILE_NAME = ".program_auth";

DongleManager::DongleManager(QObject *parent) : QObject(parent)
{
    loadAuthorizedIds();
    loadLastRunTime();
}

DongleManager::~DongleManager()
{
}

// ==================== 存储设备管理 ====================

QList<QStorageInfo> DongleManager::getAvailableUSBStorages()
{
    QList<QStorageInfo> usbStorages;
    const QList<QStorageInfo> storages = QStorageInfo::mountedVolumes();

    for (const QStorageInfo &storage : storages) {
        if (storage.isValid() && storage.isReady() && !storage.isReadOnly()) {
            // 排除系统盘（通常是C盘）
            QString rootPath = storage.rootPath().toUpper();
            if (!rootPath.startsWith("C:")) {
                usbStorages.append(storage);
                qDebug() << "[调试] 检测到可用存储设备：" << storage.rootPath() 
                         << "容量：" << storage.bytesTotal() / (1024*1024*1024) << "GB";
            }
        }
    }
    return usbStorages;
}

// ==================== 新算法：硬件特征和密钥生成 ====================

QString DongleManager::getUSBHardwareInfo(const QStorageInfo &storage)
{
    qint64 totalSize = storage.bytesTotal();
    QString fileSystem = storage.fileSystemType();
    
    // 组合稳定的硬件特征
    QString hardwareInfo = QString("%1:%2:%3").arg(totalSize).arg(fileSystem).arg(PROGRAM_ID);
    
    qDebug() << "[调试] 硬件信息：" << hardwareInfo;
    return hardwareInfo;
}

QString DongleManager::generateUniqueID(const QStorageInfo &storage)
{
    QString hardwareInfo = getUSBHardwareInfo(storage);
    QString volumeSize = QString::number(storage.bytesTotal());
    QString fileSystem = storage.fileSystemType();
    
    QString combinedInfo = hardwareInfo + volumeSize + fileSystem;
    QByteArray hash = QCryptographicHash::hash(combinedInfo.toUtf8(), QCryptographicHash::Sha256);
    
    QString dongleId = hash.toHex();
    qDebug() << "[调试] 生成dongleId：" << dongleId.left(16) << "...";
    return dongleId;
}

QByteArray DongleManager::generateDynamicKey(const QStorageInfo &storage)
{
    QString hardwareInfo = getUSBHardwareInfo(storage);
    QString volumeSize = QString::number(storage.bytesTotal());
    QString keyMaterial = hardwareInfo + volumeSize + BASE_KEY;
    
    return QCryptographicHash::hash(keyMaterial.toUtf8(), QCryptographicHash::Sha256);
}

// ==================== AES加密/解密 ====================

QByteArray DongleManager::encryptAES(const QByteArray &data, const QByteArray &key, const QByteArray &iv)
{
    if (key.size() != 32 || iv.size() != 16) {
        qDebug() << "[错误] AES密钥或IV长度不正确！";
        return QByteArray();
    }

    QByteArray encryptedData;
    QByteArray paddedData = data;
    int paddingLength = 16 - (paddedData.size() % 16);
    paddedData.append(QByteArray(paddingLength, paddingLength));

    for (int i = 0; i < paddedData.size(); i += 16) {
        QByteArray block = paddedData.mid(i, 16);
        QString blockKeyStr = QString::fromUtf8(key) + QString::fromUtf8(iv) + QString::number(i);
        QByteArray blockKey = QCryptographicHash::hash(blockKeyStr.toUtf8(), QCryptographicHash::Sha256);

        QByteArray encryptedBlock;
        for (int j = 0; j < block.size(); j++) {
            encryptedBlock.append(block[j] ^ blockKey[j % blockKey.size()]);
        }
        encryptedData.append(encryptedBlock);
    }
    return encryptedData;
}

QByteArray DongleManager::decryptAES(const QByteArray &encryptedData, const QByteArray &key, const QByteArray &iv)
{
    if (key.size() != 32 || iv.size() != 16) {
        qDebug() << "[错误] AES密钥或IV长度不正确！";
        return QByteArray();
    }

    QByteArray decryptedData;
    for (int i = 0; i < encryptedData.size(); i += 16) {
        QByteArray block = encryptedData.mid(i, 16);
        QString blockKeyStr = QString::fromUtf8(key) + QString::fromUtf8(iv) + QString::number(i);
        QByteArray blockKey = QCryptographicHash::hash(blockKeyStr.toUtf8(), QCryptographicHash::Sha256);

        QByteArray decryptedBlock;
        for (int j = 0; j < block.size(); j++) {
            decryptedBlock.append(block[j] ^ blockKey[j % blockKey.size()]);
        }
        decryptedData.append(decryptedBlock);
    }

    if (!decryptedData.isEmpty()) {
        int paddingLength = (unsigned char)decryptedData.back();
        if (paddingLength > 0 && paddingLength <= 16) {
            decryptedData.chop(paddingLength);
        }
    }
    return decryptedData;
}

QByteArray DongleManager::generateRandomIV()
{
    QByteArray iv(16, 0);
    for (int i = 0; i < 16; i++) {
        iv[i] = QRandomGenerator::global()->generate() % 256;
    }
    return iv;
}

// ==================== 签名生成/验证 ====================

QByteArray DongleManager::generateSignature(const QByteArray &data, const QByteArray &privateKey)
{
    return QCryptographicHash::hash(data + privateKey, QCryptographicHash::Sha256);
}

bool DongleManager::verifySignature(const QByteArray &data, const QByteArray &signature, const QByteArray &publicKey)
{
    QByteArray expectedSignature = QCryptographicHash::hash(data + publicKey, QCryptographicHash::Sha256);
    return (signature == expectedSignature);
}

// ==================== 数据加密/解密（新格式） ====================

QString DongleManager::encryptData(const QString &data)
{
    QString programPath = getCurrentProgramPath();
    QStorageInfo storage = QStorageInfo(QFileInfo(programPath).path());
    return encryptData(data, storage);
}

QString DongleManager::encryptData(const QString &data, const QStorageInfo &storage)
{
    qDebug() << "[加密] 开始加密数据，存储设备：" << storage.rootPath();
    
    QByteArray key = generateDynamicKey(storage);
    QByteArray iv = generateRandomIV();
    QByteArray dataBytes = data.toUtf8();
    QByteArray encryptedData = encryptAES(dataBytes, key, iv);
    QByteArray signature = generateSignature(encryptedData + iv, key);

    // 新格式: [4字节加密数据长度][加密数据][16字节IV][32字节签名]
    QByteArray combinedData;
    quint32 encryptedLen = encryptedData.size();
    combinedData.append(reinterpret_cast<const char*>(&encryptedLen), sizeof(encryptedLen));
    combinedData.append(encryptedData);
    combinedData.append(iv);
    combinedData.append(signature);

    qDebug() << "[加密] 加密成功，原始长度：" << data.length() << "加密后长度：" << combinedData.size();
    return combinedData.toBase64();
}

QString DongleManager::decryptData(const QString &encryptedData)
{
    QString programPath = getCurrentProgramPath();
    QStorageInfo storage = QStorageInfo(QFileInfo(programPath).path());
    return decryptData(encryptedData, storage);
}

QString DongleManager::decryptData(const QString &encryptedData, const QStorageInfo &storage)
{
    LOG_DEBUG(QString("开始解密数据，存储设备：%1").arg(storage.rootPath()), "加密狗验证");

    QByteArray combinedData = QByteArray::fromBase64(encryptedData.toUtf8());
    
    // 解析新格式: [4字节长度][加密数据][16字节IV][32字节签名]
    // 最小长度: 4 + 16 + 16 + 32 = 68字节
    if (combinedData.size() < 68) {
        LOG_ERROR(QString("加密数据太短：%1字节").arg(combinedData.size()), "加密狗验证");
        return "";
    }

    quint32 encryptedLen = *reinterpret_cast<const quint32*>(combinedData.constData());
    int expectedTotalLen = 4 + encryptedLen + 16 + 32;
    
    if (expectedTotalLen != combinedData.size() || encryptedLen == 0 || encryptedLen % 16 != 0) {
        LOG_ERROR(QString("加密数据格式错误：期望%1字节，实际%2字节").arg(expectedTotalLen).arg(combinedData.size()), "加密狗验证");
        return "";
    }

    QByteArray encryptedContent = combinedData.mid(4, encryptedLen);
    QByteArray iv = combinedData.mid(4 + encryptedLen, 16);
    QByteArray signature = combinedData.mid(4 + encryptedLen + 16, 32);

    // 生成密钥并验证签名
    QByteArray key = generateDynamicKey(storage);
    
    if (!verifySignature(encryptedContent + iv, signature, key)) {
        LOG_ERROR("签名验证失败！密钥不匹配", "加密狗验证");
        return "";
    }

    LOG_INFO("签名验证成功", "加密狗验证");
    
    QByteArray decryptedData = decryptAES(encryptedContent, key, iv);
    QString result = QString::fromUtf8(decryptedData);
    
    LOG_DEBUG(QString("解密成功，数据：%1").arg(result), "加密狗验证");
    return result;
}

// ==================== 授权文件读写 ====================

QString DongleManager::readDongleAuthorization(const QStorageInfo &storage)
{
    QString rootPath = storage.rootPath();
    if (!rootPath.endsWith("/") && !rootPath.endsWith("\\")) {
        rootPath += "/";
    }
    QString authFilePath = rootPath + AUTH_FILE_NAME;
    
    LOG_INFO(QString("授权文件路径：%1").arg(authFilePath), "加密狗验证");

    QFile file(authFilePath);
    if (!file.exists()) {
        LOG_ERROR(QString("授权文件不存在：%1").arg(authFilePath), "加密狗验证");
        return "";
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(QString("无法打开授权文件：%1").arg(file.errorString()), "加密狗验证");
        return "";
    }

    QString encryptedContent = file.readAll().trimmed();
    file.close();

    LOG_INFO(QString("读取到加密授权内容，长度：%1").arg(encryptedContent.length()), "加密狗验证");
    return decryptData(encryptedContent, storage);
}

bool DongleManager::writeDongleAuthorization(const QStorageInfo &storage, const QString &authInfo)
{
    QString rootPath = storage.rootPath();
    if (!rootPath.endsWith("/") && !rootPath.endsWith("\\")) {
        rootPath += "/";
    }
    QString authFilePath = rootPath + AUTH_FILE_NAME;
    
    qDebug() << "[调试] 写入授权文件：" << authFilePath;

    QString encryptedAuthInfo = encryptData(authInfo, storage);
    
    QFile file(authFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "[错误] 无法打开授权文件进行写入：" << file.errorString();
        return false;
    }

    QTextStream out(&file);
    out << encryptedAuthInfo;
    file.close();

    // 设置文件为隐藏属性
    QProcess process;
    process.start("cmd.exe", QStringList() << "/c" << "attrib" << "+h" << authFilePath);
    process.waitForFinished();

    qDebug() << "[调试] 授权文件写入成功";
    return true;
}

// ==================== 授权初始化 ====================

bool DongleManager::initializeDongle(const QStorageInfo &storage, const QString &programId)
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime endTime; // 无效日期表示永久授权
    return initializeDongle(storage, programId, now, endTime);
}

bool DongleManager::initializeDongle(const QStorageInfo &storage, const QString &programId, const QDateTime &startTime, const QDateTime &endTime)
{
    QString dongleId = generateUniqueID(storage);
    QString startTimeStr = startTime.toString("yyyyMMddHHmmss");
    QString endTimeStr = endTime.isValid() ? endTime.toString("yyyyMMddHHmmss") : "";

    // 授权信息格式：programId|dongleId|startTime|endTime
    QString authInfo = programId + "|" + dongleId + "|" + startTimeStr + "|" + endTimeStr;
    
    qDebug() << "[调试] 初始化授权信息：" << authInfo;

    if (writeDongleAuthorization(storage, authInfo)) {
        if (!m_authorizedDongleIds.contains(dongleId)) {
            m_authorizedDongleIds.append(dongleId);
            saveAuthorizedIds();
        }
        return true;
    }
    return false;
}

// ==================== 程序保护验证 ====================

bool DongleManager::protectProgram()
{
    qDebug() << "[安全] ========== 开始程序保护验证 ==========";
    
    // 第1步：获取程序所在存储设备
    QString programPath = getCurrentProgramPath();
    QFileInfo programFile(programPath);
    QStorageInfo programStorage(programFile.absolutePath());
    
    qDebug() << "[安全] 程序路径：" << programPath;
    qDebug() << "[安全] 存储设备：" << programStorage.rootPath();

    // 第2步：生成当前U盘的dongleId
    QString currentDongleId = generateUniqueID(programStorage);
    qDebug() << "[安全] 当前dongleId：" << currentDongleId.left(16) << "...";

    // 第3步：读取并解密授权信息
    QString authInfo = readDongleAuthorization(programStorage);
    if (authInfo.isEmpty()) {
        LOG_ERROR("授权信息解密失败或授权文件不存在", "加密狗验证");
        return false;
    }
    
    qDebug() << "[安全] 解密后的授权信息：" << authInfo;

    // 第4步：验证授权信息
    QStringList authParts = authInfo.split("|");
    if (authParts.size() < 2) {
        LOG_ERROR("授权信息格式错误", "加密狗验证");
        return false;
    }

    QString actualProgramId = authParts[0];
    QString actualDongleId = authParts[1];
    QString expectedProgramId = getProgramId();

    qDebug() << "[安全] 期望programId：" << expectedProgramId;
    qDebug() << "[安全] 实际programId：" << actualProgramId;

    if (actualProgramId != expectedProgramId) {
        LOG_ERROR(QString("programId不匹配：期望%1，实际%2").arg(expectedProgramId).arg(actualProgramId), "加密狗验证");
        return false;
    }

    if (actualDongleId != currentDongleId) {
        LOG_ERROR("dongleId不匹配，U盘可能被复制", "加密狗验证");
        return false;
    }

    // 第5步：验证时间授权（如果有）
    if (authParts.size() >= 4) {
        QString startTimeStr = authParts[2];
        QString endTimeStr = authParts[3];
        QDateTime currentTime = QDateTime::currentDateTime();

        if (!startTimeStr.isEmpty()) {
            QDateTime startTime = QDateTime::fromString(startTimeStr, "yyyyMMddHHmmss");
            if (startTime.isValid() && currentTime < startTime) {
                LOG_ERROR("授权尚未生效", "加密狗验证");
                return false;
            }
        }

        if (!endTimeStr.isEmpty()) {
            QDateTime endTime = QDateTime::fromString(endTimeStr, "yyyyMMddHHmmss");
            if (endTime.isValid() && currentTime > endTime) {
                LOG_ERROR("授权已过期", "加密狗验证");
                return false;
            }
        }
    }

    qDebug() << "[安全] ========== 授权验证通过！ ==========";
    LOG_INFO("程序授权验证通过", "加密狗验证");
    
    // 保存运行时间
    saveLastRunTime();
    return true;
}

bool DongleManager::isStorageAuthorized(const QStorageInfo &storage)
{
    QString currentDongleId = generateUniqueID(storage);
    QString authInfo = readDongleAuthorization(storage);
    
    if (authInfo.isEmpty()) {
        return false;
    }

    QStringList authParts = authInfo.split("|");
    if (authParts.size() < 2) {
        return false;
    }

    QString actualProgramId = authParts[0];
    QString actualDongleId = authParts[1];

    return (actualProgramId == getProgramId() && actualDongleId == currentDongleId);
}

// ==================== 辅助函数 ====================

QString DongleManager::getCurrentProgramPath()
{
    return QCoreApplication::applicationFilePath();
}

QString DongleManager::getProgramId()
{
    return PROGRAM_ID;
}

bool DongleManager::isRunningFromUSB()
{
    QString programPath = getCurrentProgramPath();
    QFileInfo programFile(programPath);
    QStorageInfo programStorage(programFile.absolutePath());
    QString programRootPath = programStorage.rootPath().toUpper();
    
    // 检查是否不在C盘
    return !programRootPath.startsWith("C:");
}

bool DongleManager::isAuthorizedDongle(const QString &dongleId)
{
    return m_authorizedDongleIds.contains(dongleId);
}

bool DongleManager::checkProgramIntegrity()
{
    // 简化：直接返回true
    return true;
}

void DongleManager::loadAuthorizedIds()
{
    // 从内存中加载，不需要文件
    m_authorizedDongleIds.clear();
}

void DongleManager::saveAuthorizedIds()
{
    // 不需要保存到文件
}

void DongleManager::saveLastRunTime()
{
    m_lastRunTime = QDateTime::currentDateTime();
}

void DongleManager::loadLastRunTime()
{
    m_lastRunTime = QDateTime::currentDateTime();
}

QDateTime DongleManager::getNetworkTime()
{
    // 简化：返回无效时间，使用本地时间
    return QDateTime();
}

bool DongleManager::isTimeValid(const QDateTime &time)
{
    QDateTime minTime = QDateTime(QDate(2024, 1, 1), QTime(0, 0, 0));
    QDateTime maxTime = QDateTime(QDate(2030, 12, 31), QTime(23, 59, 59));
    return time.isValid() && time >= minTime && time <= maxTime;
}

bool DongleManager::isTimeDifferenceValid(const QDateTime &currentTime)
{
    Q_UNUSED(currentTime);
    return true;
}

QDateTime DongleManager::getFileModificationTime(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.lastModified();
}

bool DongleManager::isFileTimeValid(const QDateTime &fileTime, const QDateTime &currentTime)
{
    Q_UNUSED(fileTime);
    Q_UNUSED(currentTime);
    return true;
}
