#ifndef VOLUMEUNIT_H
#define VOLUMEUNIT_H

#include <QString>
#include <QList>

/**
 * @file volumeUnit.h
 * @brief 容积单位枚举定义及转换方法
 * 
 * 容积单位对应关系：
 * 单位       | 对应编码
 * cm3        | 0
 * mm3        | 256
 * ml         | 512
 * l          | 768
 * inch3      | 1024
 * feet3      | 1280
 */

enum class VolumeUnit {
    Cm3 = 0,       // 立方厘米
    Mm3 = 256,     // 立方毫米
    Ml = 512,      // 毫升
    L = 768,       // 升
    Inch3 = 1024,  // 立方英寸
    Feet3 = 1280   // 立方英尺
};

class VolumeUnitHelper {
public:
    /**
     * @brief 从整数值转换为容积单位枚举
     * @param value 整数值
     * @return 对应的容积单位枚举
     */
    static VolumeUnit fromInt(int value);
    
    /**
     * @brief 从容积单位枚举转换为整数值
     * @param unit 容积单位枚举
     * @return 对应的整数值
     */
    static int toInt(VolumeUnit unit);
    
    /**
     * @brief 从字符串转换为容积单位枚举
     * @param unitString 单位字符串
     * @return 对应的容积单位枚举
     */
    static VolumeUnit fromString(const QString &unitString);
    
    /**
     * @brief 从容积单位枚举转换为字符串
     * @param unit 容积单位枚举
     * @return 对应的单位字符串
     */
    static QString toString(VolumeUnit unit);
    
    /**
     * @brief 获取所有可用的容积单位字符串列表
     * @return 单位字符串列表
     */
    static QList<QString> getUnitList();
    
    /**
     * @brief 根据单位字符串获取对应整数值
     * @param unitString 单位字符串
     * @return 对应的整数值
     */
    static int getUnitValue(const QString &unitString);
};

#endif // VOLUMEUNIT_H