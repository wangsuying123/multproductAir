#include "leakUnit.h"

/**
 * @brief 从整数值转换为泄露单位枚举（直接使用枚举值）
 * @param value 整数值
 * @return 对应的泄露单位枚举
 */
LeakUnit LeakUnitHelper::fromInt(int value) {
    // 直接调用fromRegister8212，因为8212寄存器编码与枚举值相同
    return fromRegister8212(value);
}

/**
 * @brief 从泄露单位枚举转换为整数值（直接返回枚举值）
 * @param unit 泄露单位枚举
 * @return 对应的整数值
 */
int LeakUnitHelper::toInt(LeakUnit unit) {
    // 直接返回枚举值的整数值
    return static_cast<int>(unit);
}

/**
 * @brief 从8212寄存器整数值转换为泄露单位枚举
 * @param value 8212寄存器整数值
 * @return 对应的泄露单位枚举
 */
LeakUnit LeakUnitHelper::fromRegister8212(int value) {
    switch (value) {
    case 0:
        return LeakUnit::Pa;
    case 256:
        return LeakUnit::PaPerSecond;
    case 512:
        return LeakUnit::PaPerHour;
    case 768:
        return LeakUnit::PaPerSecondHour;
    case 1024:
        return LeakUnit::Reserved1;
    case 1280:
        return LeakUnit::Reserved2;
    case 1536:
        return LeakUnit::CcPerMinute;
    case 1792:
        return LeakUnit::CcPerSecond;
    case 2048:
        return LeakUnit::CcPerHour;
    case 2304:
        return LeakUnit::Mm3PerSecond;
    case 2560:
        return LeakUnit::Cc3PerSecond;
    case 2816:
        return LeakUnit::Cc3PerMinute;
    case 3072:
        return LeakUnit::Cc3PerHour;
    case 3328:
        return LeakUnit::MlPerSecond;
    case 3584:
        return LeakUnit::MlPerMinute;
    case 3840:
        return LeakUnit::MlPerHour;
    case 4096:
        return LeakUnit::In3PerSecond;
    case 4352:
        return LeakUnit::In3PerMinute;
    case 4608:
        return LeakUnit::In3PerHour;
    case 4864:
        return LeakUnit::Reserved3;
    case 6144:
        return LeakUnit::Sccm;
    case 6400:
        return LeakUnit::Points;
    default:
        return LeakUnit::Pa; // 默认返回Pa
    }
}

/**
 * @brief 从8217寄存器整数值转换为泄露单位枚举
 * @param value 8217寄存器整数值
 * @return 对应的泄露单位枚举
 */
LeakUnit LeakUnitHelper::fromRegister8217(int value) {
    // 注意：8217编码对应多个单位，这里根据常见使用情况返回一个合理的默认值
    switch (value) {
    case 0:
        return LeakUnit::Pa;  // Pa单位8217编码为0
    case 768:
        return LeakUnit::Points;
    case 7680:
        return LeakUnit::Reserved1; // 或Reserved2，两者8217编码相同
    case 12405:
        return LeakUnit::Sccm;
    case 47115:
        return LeakUnit::MlPerMinute; // 常用单位，作为默认返回
    default:
        return LeakUnit::Pa; // 默认返回Pa
    }
}

/**
 * @brief 获取泄露单位对应的8212寄存器编码
 * @param unit 泄露单位枚举
 * @return 对应的8212寄存器编码
 */
int LeakUnitHelper::getRegister8212(LeakUnit unit) {
    switch (unit) {
    case LeakUnit::Pa:
        return 0;
    case LeakUnit::PaPerSecond:
        return 256;
    case LeakUnit::PaPerHour:
        return 512;
    case LeakUnit::PaPerSecondHour:
        return 768;
    case LeakUnit::Reserved1:
        return 1024;
    case LeakUnit::Reserved2:
        return 1280;
    case LeakUnit::CcPerMinute:
        return 1536;
    case LeakUnit::CcPerSecond:
        return 1792;
    case LeakUnit::CcPerHour:
        return 2048;
    case LeakUnit::Mm3PerSecond:
        return 2304;
    case LeakUnit::Cc3PerSecond:
        return 2560;
    case LeakUnit::Cc3PerMinute:
        return 2816;
    case LeakUnit::Cc3PerHour:
        return 3072;
    case LeakUnit::MlPerSecond:
        return 3328;
    case LeakUnit::MlPerMinute:
        return 3584;
    case LeakUnit::MlPerHour:
        return 3840;
    case LeakUnit::In3PerSecond:
        return 4096;
    case LeakUnit::In3PerMinute:
        return 4352;
    case LeakUnit::In3PerHour:
        return 4608;
    case LeakUnit::Reserved3:
        return 4864;
    case LeakUnit::Sccm:
        return 6144;
    case LeakUnit::Points:
        return 6400;
    default:
        return 0;
    }
}

/**
 * @brief 获取泄露单位对应的8217寄存器编码
 * @param unit 泄露单位枚举
 * @return 对应的8217寄存器编码
 */
int LeakUnitHelper::getRegister8217(LeakUnit unit) {
    switch (unit) {
    case LeakUnit::Pa:
        return 0;  // Pa单位8217编码为0
    case LeakUnit::PaPerSecond:
        return 0;
    case LeakUnit::PaPerHour:
        return 0;
    case LeakUnit::PaPerSecondHour:
        return 0;
    case LeakUnit::Reserved1:
    case LeakUnit::Reserved2:
        return 7680;
    case LeakUnit::CcPerMinute:
    case LeakUnit::CcPerSecond:
    case LeakUnit::CcPerHour:
    case LeakUnit::Mm3PerSecond:
    case LeakUnit::Cc3PerSecond:
    case LeakUnit::Cc3PerMinute:
    case LeakUnit::Cc3PerHour:
    case LeakUnit::MlPerSecond:
    case LeakUnit::MlPerMinute:
    case LeakUnit::MlPerHour:
    case LeakUnit::In3PerSecond:
    case LeakUnit::In3PerMinute:
    case LeakUnit::In3PerHour:
    case LeakUnit::Reserved3:
        return 47115;
    case LeakUnit::Sccm:
        return 12405;
    case LeakUnit::Points:
        return 768;
    default:
        return 0;
    }
}

/**
 * @brief 从字符串转换为泄露单位枚举
 * @param unitString 单位字符串
 * @return 对应的泄露单位枚举
 */
LeakUnit LeakUnitHelper::fromString(const QString &unitString) {
    QString lowerString = unitString.toLower().trimmed();
    
    if (lowerString == "pa") {
        return LeakUnit::Pa;
    } else if (lowerString == "pa/s") {
        return LeakUnit::PaPerSecond;
    } else if (lowerString == "pa/hr") {
        return LeakUnit::PaPerHour;
    } else if (lowerString == "pa/s hr") {
        return LeakUnit::PaPerSecondHour;
    } else if (lowerString == "cc/min") {
        return LeakUnit::CcPerMinute;
    } else if (lowerString == "cc/s") {
        return LeakUnit::CcPerSecond;
    } else if (lowerString == "cc/h") {
        return LeakUnit::CcPerHour;
    } else if (lowerString == "mm3/s") {
        return LeakUnit::Mm3PerSecond;
    } else if (lowerString == "cc3/s") {
        return LeakUnit::Cc3PerSecond;
    } else if (lowerString == "cc3/min") {
        return LeakUnit::Cc3PerMinute;
    } else if (lowerString == "cc3/h") {
        return LeakUnit::Cc3PerHour;
    } else if (lowerString == "ml/s") {
        return LeakUnit::MlPerSecond;
    } else if (lowerString == "ml/min") {
        return LeakUnit::MlPerMinute;
    } else if (lowerString == "ml/h") {
        return LeakUnit::MlPerHour;
    } else if (lowerString == "in3/s") {
        return LeakUnit::In3PerSecond;
    } else if (lowerString == "in3/min") {
        return LeakUnit::In3PerMinute;
    } else if (lowerString == "in3/h") {
        return LeakUnit::In3PerHour;
    } else if (lowerString == "sccm") {
        return LeakUnit::Sccm;
    } else if (lowerString == "points") {
        return LeakUnit::Points;
    } else {
        return LeakUnit::Pa; // 默认返回Pa
    }
}

/**
 * @brief 从泄露单位枚举转换为字符串
 * @param unit 泄露单位枚举
 * @return 对应的单位字符串
 */
QString LeakUnitHelper::toString(LeakUnit unit) {
    switch (unit) {
    case LeakUnit::Pa:
        return "Pa";
    case LeakUnit::PaPerSecond:
        return "Pa/s";
    case LeakUnit::PaPerHour:
        return "Pa/HR";
    case LeakUnit::PaPerSecondHour:
        return "Pa/s HR";
    case LeakUnit::Reserved1:
    case LeakUnit::Reserved2:
    case LeakUnit::Reserved3:
        return "Reserved";
    case LeakUnit::CcPerMinute:
        return "cc/Min";
    case LeakUnit::CcPerSecond:
        return "cc/s";
    case LeakUnit::CcPerHour:
        return "cc/h";
    case LeakUnit::Mm3PerSecond:
        return "mm3/s";
    case LeakUnit::Cc3PerSecond:
        return "cc3/s";
    case LeakUnit::Cc3PerMinute:
        return "cc3/Min";
    case LeakUnit::Cc3PerHour:
        return "cc3/h";
    case LeakUnit::MlPerSecond:
        return "ml/s";
    case LeakUnit::MlPerMinute:
        return "ml/min";
    case LeakUnit::MlPerHour:
        return "ml/h";
    case LeakUnit::In3PerSecond:
        return "in3/s";
    case LeakUnit::In3PerMinute:
        return "in3/min";
    case LeakUnit::In3PerHour:
        return "in3/h";
    case LeakUnit::Sccm:
        return "sccm";
    case LeakUnit::Points:
        return "points";
    default:
        return "Pa";
    }
}

/**
 * @brief 获取所有可用的泄露单位字符串列表
 * @return 单位字符串列表
 */
QList<QString> LeakUnitHelper::getUnitList() {
    QList<QString> unitList;
    unitList << "Pa" << "Pa/s" << "Pa/HR" << "Pa/s HR" << 
               "cc/Min" << "cc/s" << "cc/h" << "mm3/s" << 
               "cc3/s" << "cc3/Min" << "cc3/h" << "ml/s" << 
               "ml/min" << "ml/h" << "in3/s" << "in3/min" << 
               "in3/h" << "sccm" << "points";
    return unitList;
}

/**
 * @brief 根据单位字符串获取对应的8212寄存器编码
 * @param unitString 单位字符串
 * @return 对应的8212寄存器编码
 */
int LeakUnitHelper::getRegister8212Value(const QString &unitString) {
    LeakUnit unit = fromString(unitString);
    return getRegister8212(unit);
}

/**
 * @brief 根据单位字符串获取对应的8217寄存器编码
 * @param unitString 单位字符串
 * @return 对应的8217寄存器编码
 */
int LeakUnitHelper::getRegister8217Value(const QString &unitString) {
    LeakUnit unit = fromString(unitString);
    return getRegister8217(unit);
}

/**
 * @brief 获取泄露单位的完整编码信息
 * @param unit 泄露单位枚举
 * @return 包含8212和8217寄存器编码的结构体
 */
LeakUnitCode LeakUnitHelper::getUnitCode(LeakUnit unit) {
    LeakUnitCode code;
    code.register8212 = getRegister8212(unit);
    code.register8217 = getRegister8217(unit);
    return code;
}