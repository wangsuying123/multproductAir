#include "volumeUnit.h"

/**
 * @brief 从整数值转换为容积单位枚举
 * @param value 整数值
 * @return 对应的容积单位枚举
 */
VolumeUnit VolumeUnitHelper::fromInt(int value) {
    switch (value) {
    case 0:
        return VolumeUnit::Cm3;
    case 256:
        return VolumeUnit::Mm3;
    case 512:
        return VolumeUnit::Ml;
    case 768:
        return VolumeUnit::L;
    case 1024:
        return VolumeUnit::Inch3;
    case 1280:
        return VolumeUnit::Feet3;
    default:
        return VolumeUnit::Cm3; // 默认返回cm3
    }
}

/**
 * @brief 从容积单位枚举转换为整数值
 * @param unit 容积单位枚举
 * @return 对应的整数值
 */
int VolumeUnitHelper::toInt(VolumeUnit unit) {
    return static_cast<int>(unit);
}

/**
 * @brief 从字符串转换为容积单位枚举
 * @param unitString 单位字符串
 * @return 对应的容积单位枚举
 */
VolumeUnit VolumeUnitHelper::fromString(const QString &unitString) {
    QString lowerString = unitString.toLower().trimmed();
    
    if (lowerString == "cm3" || lowerString == "cm³") {
        return VolumeUnit::Cm3;
    } else if (lowerString == "mm3" || lowerString == "mm³") {
        return VolumeUnit::Mm3;
    } else if (lowerString == "ml") {
        return VolumeUnit::Ml;
    } else if (lowerString == "l") {
        return VolumeUnit::L;
    } else if (lowerString == "inch3" || lowerString == "in3" || lowerString == "in³") {
        return VolumeUnit::Inch3;
    } else if (lowerString == "feet3" || lowerString == "ft3" || lowerString == "ft³") {
        return VolumeUnit::Feet3;
    } else {
        return VolumeUnit::Cm3; // 默认返回cm3
    }
}

/**
 * @brief 从容积单位枚举转换为字符串
 * @param unit 容积单位枚举
 * @return 对应的单位字符串
 */
QString VolumeUnitHelper::toString(VolumeUnit unit) {
    switch (unit) {
    case VolumeUnit::Cm3:
        return "cm3";
    case VolumeUnit::Mm3:
        return "mm3";
    case VolumeUnit::Ml:
        return "ml";
    case VolumeUnit::L:
        return "l";
    case VolumeUnit::Inch3:
        return "inch3";
    case VolumeUnit::Feet3:
        return "feet3";
    default:
        return "cm3";
    }
}

/**
 * @brief 获取所有可用的容积单位字符串列表
 * @return 单位字符串列表
 */
QList<QString> VolumeUnitHelper::getUnitList() {
    QList<QString> unitList;
    unitList << "cm3" << "mm3" << "ml" << "l" << "inch3" << "feet3";
    return unitList;
}

/**
 * @brief 根据单位字符串获取对应整数值
 * @param unitString 单位字符串
 * @return 对应的整数值
 */
int VolumeUnitHelper::getUnitValue(const QString &unitString) {
    VolumeUnit unit = fromString(unitString);
    return toInt(unit);
}