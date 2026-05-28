#include "pressureUnit.h"

/**
 * @brief 从整数值转换为压力单位枚举
 * @param value 整数值
 * @return 对应的压力单位枚举
 */
PressureUnit PressureUnitHelper::fromInt(int value) {
    switch (value) {
    case 0:
        return PressureUnit::Pa;
    case 256:
        return PressureUnit::KPa;
    case 512:
        return PressureUnit::MPa;
    case 768:
        return PressureUnit::Bar;
    default:
        return PressureUnit::Pa; // 默认返回Pa
    }
}

/**
 * @brief 从压力单位枚举转换为整数值
 * @param unit 压力单位枚举
 * @return 对应的整数值
 */
int PressureUnitHelper::toInt(PressureUnit unit) {
    return static_cast<int>(unit);
}

/**
 * @brief 从字符串转换为压力单位枚举
 * @param unitString 单位字符串
 * @return 对应的压力单位枚举
 */
PressureUnit PressureUnitHelper::fromString(const QString &unitString) {
    QString lowerString = unitString.toLower().trimmed();
    if (lowerString == "pa") {
        return PressureUnit::Pa;
    } else if (lowerString == "kpa") {
        return PressureUnit::KPa;
    } else if (lowerString == "mpa") {
        return PressureUnit::MPa;
    } else if (lowerString == "bar") {
        return PressureUnit::Bar;
    } else {
        return PressureUnit::Pa; // 默认返回Pa
    }
}

/**
 * @brief 从压力单位枚举转换为字符串
 * @param unit 压力单位枚举
 * @return 对应的单位字符串
 */
QString PressureUnitHelper::toString(PressureUnit unit) {
    switch (unit) {
    case PressureUnit::Pa:
        return "Pa";
    case PressureUnit::KPa:
        return "kPa";
    case PressureUnit::MPa:
        return "MPa";
    case PressureUnit::Bar:
        return "bar";
    default:
        return "Pa";
    }
}

/**
 * @brief 获取所有可用的压力单位字符串列表
 * @return 单位字符串列表
 */
QList<QString> PressureUnitHelper::getUnitList() {
    QList<QString> unitList;
    unitList << "Pa" << "kPa" << "MPa" << "bar";
    return unitList;
}

/**
 * @brief 根据单位字符串获取对应整数值
 * @param unitString 单位字符串
 * @return 对应的整数值
 */
int PressureUnitHelper::getUnitValue(const QString &unitString) {
    PressureUnit unit = fromString(unitString);
    return toInt(unit);
}