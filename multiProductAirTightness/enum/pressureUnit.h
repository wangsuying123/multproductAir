#ifndef PRESSUREUNIT_H
#define PRESSUREUNIT_H

#include <QString>
#include <QList>

/**
 * @file pressureUnit.h
 * @brief 压力单位枚举定义及转换方法
 * 
 * 压力单位对应关系：
 * - bar: 768
 * - MPa: 512
 * - KPa: 256
 * - Pa: 0
 */

enum class PressureUnit {
    Pa = 0,      // 帕斯卡
    KPa = 256,   // 千帕
    MPa = 512,   // 兆帕
    Bar = 768    // 巴
};

class PressureUnitHelper {
public:
    /**
     * @brief 从整数值转换为压力单位枚举
     * @param value 整数值
     * @return 对应的压力单位枚举
     */
    static PressureUnit fromInt(int value);
    
    /**
     * @brief 从压力单位枚举转换为整数值
     * @param unit 压力单位枚举
     * @return 对应的整数值
     */
    static int toInt(PressureUnit unit);
    
    /**
     * @brief 从字符串转换为压力单位枚举
     * @param unitString 单位字符串（如"Pa", "kPa", "MPa", "bar"）
     * @return 对应的压力单位枚举
     */
    static PressureUnit fromString(const QString &unitString);
    
    /**
     * @brief 从压力单位枚举转换为字符串
     * @param unit 压力单位枚举
     * @return 对应的单位字符串
     */
    static QString toString(PressureUnit unit);
    
    /**
     * @brief 获取所有可用的压力单位字符串列表
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

#endif // PRESSUREUNIT_H