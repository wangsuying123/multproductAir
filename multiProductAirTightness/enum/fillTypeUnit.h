#ifndef FILLTYPEUNIT_H
#define FILLTYPEUNIT_H

#include <QString>
#include <QList>

/**
 * @file fillTypeUnit.h
 * @brief 填充类型枚举定义及转换方法
 * 
 * 填充类型对应关系：
 * 类型名称       | 对应编码
 * Ramp Control   | 1024
 * Ramp           | 768
 * Instruction    | 512
 * Auto           | 256
 * Standard       | 0
 */

enum class FillType {
    Standard = 0,       // 标准
    Auto = 256,         // 自动
    Instruction = 512,  // 指令
    Ramp = 768,         // 斜坡
    RampControl = 1024  // 斜坡控制
};

class FillTypeHelper {
public:
    /**
     * @brief 从整数值转换为填充类型枚举
     * @param value 整数值
     * @return 对应的填充类型枚举
     */
    static FillType fromInt(int value);
    
    /**
     * @brief 从填充类型枚举转换为整数值
     * @param type 填充类型枚举
     * @return 对应的整数值
     */
    static int toInt(FillType type);
    
    /**
     * @brief 从字符串转换为填充类型枚举
     * @param typeString 类型字符串
     * @return 对应的填充类型枚举
     */
    static FillType fromString(const QString &typeString);
    
    /**
     * @brief 从填充类型枚举转换为字符串
     * @param type 填充类型枚举
     * @return 对应的类型字符串
     */
    static QString toString(FillType type);
    
    /**
     * @brief 获取所有可用的填充类型字符串列表
     * @return 类型字符串列表
     */
    static QList<QString> getTypeList();
    
    /**
     * @brief 根据类型字符串获取对应整数值
     * @param typeString 类型字符串
     * @return 对应的整数值
     */
    static int getTypeValue(const QString &typeString);
};

#endif // FILLTYPEUNIT_H