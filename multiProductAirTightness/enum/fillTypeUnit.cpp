#include "fillTypeUnit.h"

/**
 * @brief 从整数值转换为填充类型枚举
 * @param value 整数值
 * @return 对应的填充类型枚举
 */
FillType FillTypeHelper::fromInt(int value) {
    switch (value) {
    case 0:
        return FillType::Standard;
    case 256:
        return FillType::Auto;
    case 512:
        return FillType::Instruction;
    case 768:
        return FillType::Ramp;
    case 1024:
        return FillType::RampControl;
    default:
        return FillType::Standard; // 默认返回Standard
    }
}

/**
 * @brief 从填充类型枚举转换为整数值
 * @param type 填充类型枚举
 * @return 对应的整数值
 */
int FillTypeHelper::toInt(FillType type) {
    return static_cast<int>(type);
}

/**
 * @brief 从字符串转换为填充类型枚举
 * @param typeString 类型字符串
 * @return 对应的填充类型枚举
 */
FillType FillTypeHelper::fromString(const QString &typeString) {
    QString lowerString = typeString.toLower().trimmed();
    
    if (lowerString == "standard") {
        return FillType::Standard;
    } else if (lowerString == "auto") {
        return FillType::Auto;
    } else if (lowerString == "instruction") {
        return FillType::Instruction;
    } else if (lowerString == "ramp") {
        return FillType::Ramp;
    } else if (lowerString == "ramp control") {
        return FillType::RampControl;
    } else {
        return FillType::Standard; // 默认返回Standard
    }
}

/**
 * @brief 从填充类型枚举转换为字符串
 * @param type 填充类型枚举
 * @return 对应的类型字符串
 */
QString FillTypeHelper::toString(FillType type) {
    switch (type) {
    case FillType::Standard:
        return "Standard";
    case FillType::Auto:
        return "Auto";
    case FillType::Instruction:
        return "Instruction";
    case FillType::Ramp:
        return "Ramp";
    case FillType::RampControl:
        return "Ramp Control";
    default:
        return "Standard";
    }
}

/**
 * @brief 获取所有可用的填充类型字符串列表
 * @return 类型字符串列表
 */
QList<QString> FillTypeHelper::getTypeList() {
    QList<QString> typeList;
    typeList << "Standard" << "Auto" << "Instruction" << "Ramp" << "Ramp Control";
    return typeList;
}

/**
 * @brief 根据类型字符串获取对应整数值
 * @param typeString 类型字符串
 * @return 对应的整数值
 */
int FillTypeHelper::getTypeValue(const QString &typeString) {
    FillType type = fromString(typeString);
    return toInt(type);
}