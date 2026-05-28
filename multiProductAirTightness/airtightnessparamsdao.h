#ifndef AIRTIGHTNESSPARAMSDAO_H
#define AIRTIGHTNESSPARAMSDAO_H

#include <QMap>
#include <QVariant>
#include <QList>

class DatabaseManager;

class AirTightnessParamsDao
{
public:
    // 构造函数
    AirTightnessParamsDao();
    
    // 析构函数
    ~AirTightnessParamsDao();
    
    // 保存气密仪参数
    bool saveParams(const QMap<QString, QVariant>& params);
    
    // 修改气密仪参数
    bool updateParams(int id, const QMap<QString, QVariant>& params);
    
    // 删除气密仪参数
    bool deleteParams(int id);
    
    // 根据ID获取气密仪参数
    QMap<QString, QVariant> getParamsById(int id);
    
    // 根据程序号获取气密仪参数
    QList<QMap<QString, QVariant>> getParamsByProgram(int programNumber);
    
    // 获取最后一个错误信息
    QString getLastError() const;

private:
    DatabaseManager* m_dbManager;
    QString m_lastError;
};

#endif // AIRTIGHTNESSPARAMSDAO_H
