#include "machinefingerprint.h"
#include <QProcess>
#include <QCryptographicHash>
#include <QDebug>
#include <QRegularExpression>

MachineFingerprint::MachineFingerprint()
{
}

MachineFingerprint::~MachineFingerprint()
{
}

QString MachineFingerprint::executeCommand(const QString &command)
{
    QProcess process;
    process.start("cmd.exe", QStringList() << "/c" << command);
    process.waitForFinished(5000); // 等待5秒
    
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    return output;
}

QString MachineFingerprint::extractWMICValue(const QString &output)
{
    // WMIC输出格式通常是多行，第二行是值
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.size() >= 2) {
        return cleanString(lines[1]);
    }
    return "";
}

QString MachineFingerprint::cleanString(const QString &str)
{
    QString cleaned = str.trimmed();
    cleaned.remove('\r');
    cleaned.remove('\n');
    cleaned = cleaned.simplified();
    return cleaned;
}

QString MachineFingerprint::generateHash(const QString &data)
{
    QByteArray hash = QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex();
}

QString MachineFingerprint::getCPUSerialNumber()
{
    QString output = executeCommand("wmic cpu get ProcessorId");
    QString cpuId = extractWMICValue(output);
    
    qDebug() << "[机器指纹] CPU ID:" << cpuId;
    return cpuId;
}

QString MachineFingerprint::getMotherboardSerialNumber()
{
    QString output = executeCommand("wmic baseboard get SerialNumber");
    QString serialNumber = extractWMICValue(output);
    
    qDebug() << "[机器指纹] 主板序列号:" << serialNumber;
    return serialNumber;
}

QString MachineFingerprint::getHardDiskSerialNumber()
{
    QString output = executeCommand("wmic diskdrive get SerialNumber");
    QString serialNumber = extractWMICValue(output);
    
    qDebug() << "[机器指纹] 硬盘序列号:" << serialNumber;
    return serialNumber;
}

QString MachineFingerprint::getMACAddress()
{
    QString output = executeCommand("getmac /fo csv /nh");
    
    // 提取第一个物理网卡的MAC地址
    QRegularExpression re("\"([0-9A-F]{2}-[0-9A-F]{2}-[0-9A-F]{2}-[0-9A-F]{2}-[0-9A-F]{2}-[0-9A-F]{2})\"");
    QRegularExpressionMatch match = re.match(output);
    
    QString macAddress;
    if (match.hasMatch()) {
        macAddress = match.captured(1);
    }
    
    qDebug() << "[机器指纹] MAC地址:" << macAddress;
    return macAddress;
}

QString MachineFingerprint::getWindowsProductID()
{
    QString output = executeCommand("wmic os get SerialNumber");
    QString productId = extractWMICValue(output);
    
    qDebug() << "[机器指纹] Windows产品ID:" << productId;
    return productId;
}

QString MachineFingerprint::getComputerName()
{
    QString output = executeCommand("hostname");
    QString computerName = cleanString(output);
    
    qDebug() << "[机器指纹] 计算机名:" << computerName;
    return computerName;
}

QMap<QString, QString> MachineFingerprint::getHardwareInfo()
{
    QMap<QString, QString> info;
    
    info["CPU_ID"] = getCPUSerialNumber();
    info["Motherboard_SN"] = getMotherboardSerialNumber();
    info["HardDisk_SN"] = getHardDiskSerialNumber();
    info["MAC_Address"] = getMACAddress();
    info["Windows_ID"] = getWindowsProductID();
    info["Computer_Name"] = getComputerName();
    
    return info;
}

QString MachineFingerprint::getMachineFingerprint()
{
    qDebug() << "[机器指纹] ========== 开始生成机器指纹 ==========";
    
    // 获取所有硬件信息
    QMap<QString, QString> hwInfo = getHardwareInfo();
    
    // 组合所有硬件特征（使用多个特征提高唯一性）
    QString combined;
    combined += "CPU:" + hwInfo["CPU_ID"] + "|";
    combined += "MB:" + hwInfo["Motherboard_SN"] + "|";
    combined += "HD:" + hwInfo["HardDisk_SN"] + "|";
    combined += "MAC:" + hwInfo["MAC_Address"] + "|";
    combined += "WIN:" + hwInfo["Windows_ID"];
    
    qDebug() << "[机器指纹] 组合特征:" << combined;
    
    // 生成SHA-256哈希作为最终指纹
    QString fingerprint = generateHash(combined);
    
    qDebug() << "[机器指纹] 最终指纹:" << fingerprint;
    qDebug() << "[机器指纹] ========== 机器指纹生成完成 ==========";
    
    return fingerprint;
}

bool MachineFingerprint::verifyFingerprint(const QString &expectedFingerprint)
{
    QString currentFingerprint = getMachineFingerprint();
    bool matched = (currentFingerprint == expectedFingerprint);
    
    if (matched) {
        qDebug() << "[机器指纹] ✓ 指纹验证通过";
    } else {
        qDebug() << "[机器指纹] ✗ 指纹验证失败";
        qDebug() << "[机器指纹] 期望:" << expectedFingerprint;
        qDebug() << "[机器指纹] 实际:" << currentFingerprint;
    }
    
    return matched;
}
