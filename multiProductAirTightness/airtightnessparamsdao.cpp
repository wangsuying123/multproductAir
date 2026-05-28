#include "airtightnessparamsdao.h"
#include "databasemanager.h"

AirTightnessParamsDao::AirTightnessParamsDao()
{
    // 获取DatabaseManager实例
    m_dbManager = DatabaseManager::getInstance();
}

AirTightnessParamsDao::~AirTightnessParamsDao()
{
    // 不需要手动删除m_dbManager，因为它是单例模式
}

// 保存气密仪参数
bool AirTightnessParamsDao::saveParams(const QMap<QString, QVariant>& params)
{
    bool result = m_dbManager->saveAirTightnessParams(params);
    if (!result) {
        m_lastError = m_dbManager->getLastError();
    }
    return result;
}

// 修改气密仪参数
bool AirTightnessParamsDao::updateParams(int id, const QMap<QString, QVariant>& params)
{
    bool result = m_dbManager->updateAirTightnessParams(id, params);
    if (!result) {
        m_lastError = m_dbManager->getLastError();
    }
    return result;
}

// 删除气密仪参数
bool AirTightnessParamsDao::deleteParams(int id)
{
    bool result = m_dbManager->deleteAirTightnessParams(id);
    if (!result) {
        m_lastError = m_dbManager->getLastError();
    }
    return result;
}

// 根据ID获取气密仪参数
QMap<QString, QVariant> AirTightnessParamsDao::getParamsById(int id)
{
    QMap<QString, QVariant> result = m_dbManager->getAirTightnessParamsById(id);
    if (result.isEmpty()) {
        m_lastError = m_dbManager->getLastError();
    }
    return result;
}

// 根据程序号获取气密仪参数
QList<QMap<QString, QVariant>> AirTightnessParamsDao::getParamsByProgram(int programNumber)
{
    QList<QMap<QString, QVariant>> result = m_dbManager->getAirTightnessParamsByProgram(programNumber);
    if (result.isEmpty()) {
        m_lastError = m_dbManager->getLastError();
    }
    return result;
}

// 获取最后一个错误信息
QString AirTightnessParamsDao::getLastError() const
{
    return m_lastError;
}
