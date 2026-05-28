#ifndef LEAKUNIT_H
#define LEAKUNIT_H

#include <QString>
#include <QList>
#include <QPair>

/**
 * @file leakUnit.h
 * @brief 泄露单位枚举定义及转换方法
 * 
 * 泄露单位对应关系：
 * 单位       | 8212编码 | 8217编码
 * Pa         | 0        | 51200
 * Pa/s       | 256      | 0
 * Pa/HR      | 512      | 0
 * Pa/s HR    | 768      | 0
 * Reserved   | 1024     | 7680
 * Reserved   | 1280     | 7680
 * cc/Min     | 1536     | 47115
 * cc/s       | 1792     | 47115
 * cc/h       | 2048     | 47115
 * mm3/s      | 2304     | 47115
 * cc3/s      | 2560     | 47115
 * cc3/Min    | 2816     | 47115
 * cc3/h      | 3072     | 47115
 * ml/s       | 3328     | 47115
 * ml/min     | 3584     | 47115
 * ml/h       | 3840     | 47115
 * in3/s      | 4096     | 47115
 * in3/min    | 4352     | 47115
 * in3/h      | 4608     | 47115
 * Reserved   | 4864     | 47115
 * sccm       | 6144     | 12405
 * points     | 6400     | 768
 */

enum class LeakUnit {
    Pa = 0,          // 帕斯卡
    PaPerSecond = 256,  // 帕斯卡每秒
    PaPerHour = 512,    // 帕斯卡每小时
    PaPerSecondHour = 768,  // 帕斯卡每秒每小时
    Reserved1 = 1024,    // 保留
    Reserved2 = 1280,    // 保留
    CcPerMinute = 1536,  // 立方厘米每分钟
    CcPerSecond = 1792,  // 立方厘米每秒
    CcPerHour = 2048,    // 立方厘米每小时
    Mm3PerSecond = 2304, // 立方毫米每秒
    Cc3PerSecond = 2560, // 立方厘米每秒（cc3）
    Cc3PerMinute = 2816, // 立方厘米每分钟（cc3）
    Cc3PerHour = 3072,   // 立方厘米每小时（cc3）
    MlPerSecond = 3328,  // 毫升每秒
    MlPerMinute = 3584,  // 毫升每分钟
    MlPerHour = 3840,    // 毫升每小时
    In3PerSecond = 4096, // 立方英寸每秒
    In3PerMinute = 4352, // 立方英寸每分钟
    In3PerHour = 4608,   // 立方英寸每小时
    Reserved3 = 4864,    // 保留
    Sccm = 6144,         // 标准立方厘米每分钟
    Points = 6400        // 点数
};

struct LeakUnitCode {
    int register8212;  // 8212寄存器使用的编码
    int register8217;  // 8217寄存器使用的编码
};

class LeakUnitHelper {
public:
    /**
     * @brief 从8212寄存器整数值转换为泄露单位枚举
     * @param value 8212寄存器整数值
     * @return 对应的泄露单位枚举
     */
    static LeakUnit fromRegister8212(int value);
    
    /**
     * @brief 从8217寄存器整数值转换为泄露单位枚举
     * @param value 8217寄存器整数值
     * @return 对应的泄露单位枚举
     */
    static LeakUnit fromRegister8217(int value);
    
    /**
     * @brief 从整数值转换为泄露单位枚举（直接使用枚举值）
     * @param value 整数值
     * @return 对应的泄露单位枚举
     */
    static LeakUnit fromInt(int value);
    
    /**
     * @brief 从泄露单位枚举转换为整数值（直接返回枚举值）
     * @param unit 泄露单位枚举
     * @return 对应的整数值
     */
    static int toInt(LeakUnit unit);
    
    /**
     * @brief 获取泄露单位对应的8212寄存器编码
     * @param unit 泄露单位枚举
     * @return 对应的8212寄存器编码
     */
    static int getRegister8212(LeakUnit unit);
    
    /**
     * @brief 获取泄露单位对应的8217寄存器编码
     * @param unit 泄露单位枚举
     * @return 对应的8217寄存器编码
     */
    static int getRegister8217(LeakUnit unit);
    
    /**
     * @brief 从字符串转换为泄露单位枚举
     * @param unitString 单位字符串
     * @return 对应的泄露单位枚举
     */
    static LeakUnit fromString(const QString &unitString);
    
    /**
     * @brief 从泄露单位枚举转换为字符串
     * @param unit 泄露单位枚举
     * @return 对应的单位字符串
     */
    static QString toString(LeakUnit unit);
    
    /**
     * @brief 获取所有可用的泄露单位字符串列表
     * @return 单位字符串列表
     */
    static QList<QString> getUnitList();
    
    /**
     * @brief 根据单位字符串获取对应的8212寄存器编码
     * @param unitString 单位字符串
     * @return 对应的8212寄存器编码
     */
    static int getRegister8212Value(const QString &unitString);
    
    /**
     * @brief 根据单位字符串获取对应的8217寄存器编码
     * @param unitString 单位字符串
     * @return 对应的8217寄存器编码
     */
    static int getRegister8217Value(const QString &unitString);
    
    /**
     * @brief 获取泄露单位的完整编码信息
     * @param unit 泄露单位枚举
     * @return 包含8212和8217寄存器编码的结构体
     */
    static LeakUnitCode getUnitCode(LeakUnit unit);
};

#endif // LEAKUNIT_H
