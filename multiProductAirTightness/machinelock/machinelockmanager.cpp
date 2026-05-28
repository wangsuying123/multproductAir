#include "machinelockmanager.h"
#include <QFile>
#include <QTextStream>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDebug>
#include <QCoreApplication>
#include <QProcess>

// 程序唯一标识
static const QString PROGRAM_ID = "MULTI_PRODUCT_AIR_TIGHTNESS_V1";
static const QString BASE_KEY = "MACHINE_LOCK_ENCRYPTION_KEY_2026";
static const QString LICENSE_FILE_NAME = ".machine_license";
static const QString TIME_RECORD_FILE = ".time_record";  // 时间记录文件

MachineLockManager::MachineLockManager(QObject *parent)
    : QObject(parent),
      m_fingerprint(nullptr),
      m_isPermanent(true)
{
    m_fingerprint = new MachineFingerprint();
}

MachineLockManager::~MachineLockManager()
{
    if (m_fingerprint) {
        delete m_fingerprint;
        m_fingerprint = nullptr;
    }
}

QString MachineLockManager::getCurrentMachineFingerprint()
{
    return m_fingerprint->getMachineFingerprint();
}

QByteArray MachineLockManager::generateKey()
{
    QString keyMaterial = BASE_KEY + PROGRAM_ID;
    return QCryptographicHash::hash(keyMaterial.toUtf8(), QCryptographicHash::Sha256);
}

QByteArray MachineLockManager::generateRandomIV()
{
    QByteArray iv(16, 0);
    for (int i = 0; i < 16; i++) {
        iv[i] = QRandomGenerator::global()->generate() % 256;
    }
    return iv;
}

QByteArray MachineLockManager::encryptAES(const QByteArray &data, const QByteArray &key, const QByteArray &iv)
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

QByteArray MachineLockManager::decryptAES(const QByteArray &encryptedData, const QByteArray &key, const QByteArray &iv)
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

QString MachineLockManager::encryptLicenseData(const QString &data)
{
    QByteArray key = generateKey();
    QByteArray iv = generateRandomIV();
    QByteArray dataBytes = data.toUtf8();
    QByteArray encryptedData = encryptAES(dataBytes, key, iv);
    
    // 格式: [4字节长度][加密数据][16字节IV]
    QByteArray combinedData;
    quint32 encryptedLen = encryptedData.size();
    combinedData.append(reinterpret_cast<const char*>(&encryptedLen), sizeof(encryptedLen));
    combinedData.append(encryptedData);
    combinedData.append(iv);
    
    return combinedData.toBase64();
}

QString MachineLockManager::decryptLicenseData(const QString &encryptedData)
{
    QByteArray combinedData = QByteArray::fromBase64(encryptedData.toUtf8());
    
    if (combinedData.size() < 20) { // 最小: 4 + 16
        qDebug() << "[错误] 授权数据太短";
        return "";
    }
    
    quint32 encryptedLen = *reinterpret_cast<const quint32*>(combinedData.constData());
    int expectedTotalLen = 4 + encryptedLen + 16;
    
    if (expectedTotalLen != combinedData.size()) {
        qDebug() << "[错误] 授权数据格式错误";
        return "";
    }
    
    QByteArray encryptedContent = combinedData.mid(4, encryptedLen);
    QByteArray iv = combinedData.mid(4 + encryptedLen, 16);
    QByteArray key = generateKey();
    
    QByteArray decryptedData = decryptAES(encryptedContent, key, iv);
    return QString::fromUtf8(decryptedData);
}

bool MachineLockManager::generateLicenseFile(const QString &filePath, const QDateTime &startTime, const QDateTime &endTime)
{
    qDebug() << "[机器锁定] 开始生成授权文件:" << filePath;
    
    // 获取当前机器指纹
    QString fingerprint = m_fingerprint->getMachineFingerprint();
    
    // 构建授权信息
    QString startTimeStr = startTime.isValid() ? startTime.toString("yyyyMMddHHmmss") : "";
    QString endTimeStr = endTime.isValid() ? endTime.toString("yyyyMMddHHmmss") : "";
    
    // 格式: PROGRAM_ID|fingerprint|startTime|endTime
    QString licenseData = PROGRAM_ID + "|" + fingerprint + "|" + startTimeStr + "|" + endTimeStr;
    
    qDebug() << "[机器锁定] 授权数据:" << licenseData;
    
    // 加密授权数据
    QString encryptedData = encryptLicenseData(licenseData);
    
    // 写入文件
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "[错误] 无法创建授权文件:" << file.errorString();
        return false;
    }
    
    QTextStream out(&file);
    out << encryptedData;
    file.close();
    
    // 设置文件为隐藏
    QProcess process;
    process.start("cmd.exe", QStringList() << "/c" << "attrib" << "+h" << filePath);
    process.waitForFinished();
    
    qDebug() << "[机器锁定] 授权文件生成成功";
    return true;
}

bool MachineLockManager::loadLicenseFile(const QString &filePath)
{
    qDebug() << "[机器锁定] 读取授权文件:" << filePath;
    
    QFile file(filePath);
    if (!file.exists()) {
        qDebug() << "[错误] 授权文件不存在";
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[错误] 无法打开授权文件:" << file.errorString();
        return false;
    }
    
    QString encryptedData = file.readAll().trimmed();
    file.close();
    
    // 解密授权数据
    QString licenseData = decryptLicenseData(encryptedData);
    if (licenseData.isEmpty()) {
        qDebug() << "[错误] 授权数据解密失败";
        return false;
    }
    
    qDebug() << "[机器锁定] 解密后的授权数据:" << licenseData;
    
    // 解析授权数据
    QStringList parts = licenseData.split("|");
    if (parts.size() < 2) {
        qDebug() << "[错误] 授权数据格式错误";
        return false;
    }
    
    QString programId = parts[0];
    m_authorizedFingerprint = parts[1];
    
    // 验证程序ID
    if (programId != PROGRAM_ID) {
        qDebug() << "[错误] 程序ID不匹配";
        return false;
    }
    
    // 解析时间信息
    if (parts.size() >= 4) {
        QString startTimeStr = parts[2];
        QString endTimeStr = parts[3];
        
        if (!startTimeStr.isEmpty()) {
            m_startTime = QDateTime::fromString(startTimeStr, "yyyyMMddHHmmss");
        }
        
        if (!endTimeStr.isEmpty()) {
            m_endTime = QDateTime::fromString(endTimeStr, "yyyyMMddHHmmss");
            m_isPermanent = false;
        } else {
            m_isPermanent = true;
        }
    }
    
    qDebug() << "[机器锁定] 授权文件读取成功";
    return true;
}

bool MachineLockManager::verifyMachineLicense()
{
    qDebug() << "[机器锁定] ========== 开始验证机器授权 ==========";
    
    // 0. 检查时间完整性（防止时间篡改）
    if (!checkTimeIntegrity()) {
        qDebug() << "[机器锁定] ✗ 检测到系统时间被篡改";
        return false;
    }
    
    // 1. 查找授权文件
    QString appDir = QCoreApplication::applicationDirPath();
    QString licenseFilePath = appDir + "/" + LICENSE_FILE_NAME;
    
    qDebug() << "[机器锁定] 授权文件路径:" << licenseFilePath;
    
    // 2. 读取授权文件
    if (!loadLicenseFile(licenseFilePath)) {
        qDebug() << "[机器锁定] ✗ 授权文件读取失败";
        return false;
    }
    
    // 3. 验证机器指纹
    QString currentFingerprint = m_fingerprint->getMachineFingerprint();
    if (currentFingerprint != m_authorizedFingerprint) {
        qDebug() << "[机器锁定] ✗ 机器指纹不匹配";
        qDebug() << "[机器锁定] 期望:" << m_authorizedFingerprint;
        qDebug() << "[机器锁定] 实际:" << currentFingerprint;
        return false;
    }
    
    qDebug() << "[机器锁定] ✓ 机器指纹验证通过";
    
    // 4. 验证时间授权
    if (!m_isPermanent) {
        QDateTime currentTime = QDateTime::currentDateTime();
        
        if (m_startTime.isValid() && currentTime < m_startTime) {
            qDebug() << "[机器锁定] ✗ 授权尚未生效";
            return false;
        }
        
        if (m_endTime.isValid() && currentTime > m_endTime) {
            qDebug() << "[机器锁定] ✗ 授权已过期";
            return false;
        }
        
        qDebug() << "[机器锁定] ✓ 时间授权验证通过";
    } else {
        qDebug() << "[机器锁定] ✓ 永久授权";
    }
    
    // 5. 记录当前时间（用于下次检测时间篡改）
    recordCurrentTime();
    
    qDebug() << "[机器锁定] ========== 机器授权验证通过！ ==========";
    return true;
}

bool MachineLockManager::isLicenseExpired()
{
    if (m_isPermanent) {
        return false;
    }
    
    if (!m_endTime.isValid()) {
        return false;
    }
    
    return QDateTime::currentDateTime() > m_endTime;
}

int MachineLockManager::getRemainingDays()
{
    if (m_isPermanent) {
        return -1; // -1表示永久授权
    }
    
    if (!m_endTime.isValid()) {
        return -1;
    }
    
    QDateTime currentTime = QDateTime::currentDateTime();
    qint64 remainingSeconds = currentTime.secsTo(m_endTime);
    int remainingDays = remainingSeconds / (24 * 3600);
    
    return remainingDays;
}

QString MachineLockManager::getLicenseInfo()
{
    QString info;
    info += "程序ID: " + PROGRAM_ID + "\n";
    info += "机器指纹: " + m_authorizedFingerprint.left(32) + "...\n";
    
    if (m_isPermanent) {
        info += "授权类型: 永久授权\n";
    } else {
        info += "授权类型: 限时授权\n";
        if (m_startTime.isValid()) {
            info += "开始时间: " + m_startTime.toString("yyyy-MM-dd HH:mm:ss") + "\n";
        }
        if (m_endTime.isValid()) {
            info += "结束时间: " + m_endTime.toString("yyyy-MM-dd HH:mm:ss") + "\n";
            int remaining = getRemainingDays();
            info += "剩余天数: " + QString::number(remaining) + " 天\n";
        }
    }
    
    return info;
}

bool MachineLockManager::checkTimeIntegrity()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QDateTime lastRecordedTime = readLastRecordedTime();
    
    // 如果是第一次运行，没有记录，直接通过
    if (!lastRecordedTime.isValid()) {
        qDebug() << "[机器锁定] 首次运行，无时间记录";
        return true;
    }
    
    // 允许的时间误差（秒）：考虑到程序可能几秒内多次启动
    const qint64 TIME_TOLERANCE = 10;
    
    // 如果当前时间早于上次记录时间（减去容差），说明时间被回调了
    qint64 timeDiff = lastRecordedTime.secsTo(currentTime);
    
    if (timeDiff < -TIME_TOLERANCE) {
        qDebug() << "[机器锁定] 检测到时间回调！";
        qDebug() << "[机器锁定] 上次时间:" << lastRecordedTime.toString("yyyy-MM-dd HH:mm:ss");
        qDebug() << "[机器锁定] 当前时间:" << currentTime.toString("yyyy-MM-dd HH:mm:ss");
        qDebug() << "[机器锁定] 时间差:" << timeDiff << "秒";
        return false;
    }
    
    qDebug() << "[机器锁定] ✓ 时间完整性验证通过";
    return true;
}

void MachineLockManager::recordCurrentTime()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString timeRecordPath = appDir + "/" + TIME_RECORD_FILE;
    
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timeStr = currentTime.toString("yyyyMMddHHmmss");
    
    // 加密时间数据（防止直接修改文件）
    QString encryptedTime = encryptLicenseData(timeStr);
    
    QFile file(timeRecordPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << encryptedTime;
        file.close();
        
        // 设置文件为隐藏
        QProcess process;
        process.start("cmd.exe", QStringList() << "/c" << "attrib" << "+h" << timeRecordPath);
        process.waitForFinished();
        
        qDebug() << "[机器锁定] 时间记录已更新:" << currentTime.toString("yyyy-MM-dd HH:mm:ss");
    } else {
        qDebug() << "[机器锁定] 时间记录写入失败:" << file.errorString();
    }
}

QDateTime MachineLockManager::readLastRecordedTime()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString timeRecordPath = appDir + "/" + TIME_RECORD_FILE;
    
    QFile file(timeRecordPath);
    if (!file.exists()) {
        return QDateTime();  // 文件不存在，返回无效时间
    }
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[机器锁定] 时间记录读取失败:" << file.errorString();
        return QDateTime();
    }
    
    QString encryptedTime = file.readAll().trimmed();
    file.close();
    
    // 解密时间数据
    QString timeStr = decryptLicenseData(encryptedTime);
    if (timeStr.isEmpty()) {
        qDebug() << "[机器锁定] 时间记录解密失败";
        return QDateTime();
    }
    
    QDateTime recordedTime = QDateTime::fromString(timeStr, "yyyyMMddHHmmss");
    if (!recordedTime.isValid()) {
        qDebug() << "[机器锁定] 时间记录格式错误";
        return QDateTime();
    }
    
    return recordedTime;
}
