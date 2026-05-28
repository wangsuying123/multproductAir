#ifndef MACHINEFINGERPRINT_H
#define MACHINEFINGERPRINT_H

#include <QString>
#include <QMap>

/**
 * @brief 机器指纹生成器 - 获取计算机的唯一硬件特征
 * 
 * 特点：
 * 1. 基于多个硬件特征生成唯一指纹
 * 2. 即使复制程序到其他电脑也无法运行
 * 3. 硬件特征包括：CPU ID、主板序列号、硬盘序列号、MAC地址等
 * 4. 使用多重验证，提高安全性
 */
class MachineFingerprint
{
public:
    MachineFingerprint();
    ~MachineFingerprint();

    // 获取机器指纹（综合多个硬件特征）
    QString getMachineFingerprint();
    
    // 获取详细的硬件信息
    QMap<QString, QString> getHardwareInfo();
    
    // 获取CPU序列号
    QString getCPUSerialNumber();
    
    // 获取主板序列号
    QString getMotherboardSerialNumber();
    
    // 获取硬盘序列号（第一个物理硬盘）
    QString getHardDiskSerialNumber();
    
    // 获取MAC地址（第一个物理网卡）
    QString getMACAddress();
    
    // 获取Windows产品ID
    QString getWindowsProductID();
    
    // 获取计算机名称
    QString getComputerName();
    
    // 验证机器指纹是否匹配
    bool verifyFingerprint(const QString &expectedFingerprint);

private:
    // 执行Windows命令并获取输出
    QString executeCommand(const QString &command);
    
    // 从WMIC输出中提取值
    QString extractWMICValue(const QString &output);
    
    // 清理字符串（去除空格、换行等）
    QString cleanString(const QString &str);
    
    // 生成指纹的哈希值
    QString generateHash(const QString &data);
};

#endif // MACHINEFINGERPRINT_H
