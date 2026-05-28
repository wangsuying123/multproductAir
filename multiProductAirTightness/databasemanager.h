#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QString>
#include <QVariant>
#include <QList>
#include <QMap>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>

class DatabaseManager
{
public:
    // 单例模式获取实例
    static DatabaseManager* getInstance();

    // 连接数据库（默认使用AppData目录）
    bool connectDatabase(const QString& databasePath = "");
    
    // 获取数据库文件路径（AppData目录）
    static QString getDatabasePath();

    // 初始化数据库（创建表）
    bool initializeDatabase();

    // 添加记录
    bool addRecord(const QString& tableName, const QMap<QString, QVariant>& data);

    // 修改记录
    bool updateRecord(const QString& tableName, const QMap<QString, QVariant>& data, const QString& condition);

    // 删除记录
    bool deleteRecord(const QString& tableName, const QString& condition);

    // 统计总数
    int getRecordCount(const QString& tableName, const QString& condition = "");

    // 执行自定义查询（公共方法，方便后续调用）
    QSqlQuery executeQuery(const QString& queryString, const QList<QVariant>& bindings = QList<QVariant>());

    // 气密仪参数表专用方法
    // 保存气密仪参数
    bool saveAirTightnessParams(const QMap<QString, QVariant>& params);
    // 修改气密仪参数
    bool updateAirTightnessParams(int id, const QMap<QString, QVariant>& params);
    // 删除气密仪参数
    bool deleteAirTightnessParams(int id);
    // 根据ID获取气密仪参数
    QMap<QString, QVariant> getAirTightnessParamsById(int id);
    // 根据程序号获取气密仪参数
    QList<QMap<QString, QVariant>> getAirTightnessParamsByProgram(int programNumber);

    // 获取最后一个错误信息
    QString getLastError() const;

    // 关闭数据库连接
    void closeDatabase();

    // 通信参数相关方法
    // 保存通信参数
    bool saveCommunicationParams(int deviceType, const QMap<QString, QVariant>& params);
    // 更新通信参数
    bool updateCommunicationParams(int deviceType, const QMap<QString, QVariant>& params);
    // 获取通信参数
    QMap<QString, QVariant> getCommunicationParams(int deviceType);
    // 判断是否存在通信参数
    bool hasCommunicationParams(int deviceType);

    // 用户相关方法
    // 创建用户表
    bool createUserTable();
    // 添加用户
    int addUser(const QString& username, const QString& employeeId, const QString& password, const QString& role, bool enabled = true);
    // 更新用户
    bool updateUser(int id, const QString& username, const QString& employeeId, const QString& password, const QString& role, bool enabled);
    // 删除用户
    bool deleteUser(int id);
    // 根据ID获取用户
    QMap<QString, QVariant> getUserById(int id);
    // 根据用户名获取用户
    QMap<QString, QVariant> getUserByUsername(const QString& username);
    // 获取所有用户
    QList<QMap<QString, QVariant>> getAllUsers();
    // 更新用户状态
    bool updateUserStatus(int id, bool enabled);
    // 密码加密
    QString encryptPassword(const QString& password);
    // 密码验证
    bool verifyPassword(const QString& password, const QString& hashedPassword);
    // 权限相关方法
    // 保存用户权限
    bool saveUserPermissions(int userId, const QString& permissionsJson);
    // 获取用户权限
    QString getUserPermissions(int userId);
    // 更新用户权限
    bool updateUserPermissions(int userId, const QString& permissionsJson);

    // 测试结果相关方法
    // 创建测试结果表
    bool createTestResultTable();
    // 添加测试结果
    bool addTestResult(const QMap<QString, QVariant>& result);
    // 查询测试结果（支持条件查询和分页）
    QList<QMap<QString, QVariant>> getTestResults(const QString& productId = "", const QString& operatorName = "", const QString& testResult = "", const QDateTime& startTime = QDateTime(), const QDateTime& endTime = QDateTime(), int page = 1, int pageSize = 20);
    // 获取测试结果总数
    int getTestResultCount(const QString& productId = "", const QString& operatorName = "", const QString& testResult = "", const QDateTime& startTime = QDateTime(), const QDateTime& endTime = QDateTime());
    // 导出测试结果
    QList<QMap<QString, QVariant>> exportTestResults(const QString& productId = "", const QString& operatorName = "", const QString& testResult = "", const QDateTime& startTime = QDateTime(), const QDateTime& endTime = QDateTime());
    // 获取测试结果统计（测试总数、合格数、不合格数）
    QMap<QString, int> getTestResultStatistics();
    
    // 操作日志相关方法
    // 创建操作日志表
    bool createOperationLogTable();
    // 添加操作日志
    bool addOperationLog(const QString& operatorName, const QString& operation, const QString& module, const QString& details, bool success = true);
    // 查询操作日志（支持条件查询和分页）
    QList<QMap<QString, QVariant>> getOperationLogs(const QString& operatorName = "", const QString& module = "", const QDateTime& startTime = QDateTime(), const QDateTime& endTime = QDateTime(), int page = 1, int pageSize = 20);
    // 获取操作日志总数
    int getOperationLogCount(const QString& operatorName = "", const QString& module = "", const QDateTime& startTime = QDateTime(), const QDateTime& endTime = QDateTime());

    // 测试通道配置相关方法
    // 创建测试通道配置表
    bool createTestChannelConfigTable();
    // 保存测试通道配置
    bool saveTestChannelConfig(int channelNumber, bool isEnabled, int programNumber);
    // 更新测试通道配置
    bool updateTestChannelConfig(int channelNumber, bool isEnabled, int programNumber);
    // 获取测试通道配置
    QMap<QString, QVariant> getTestChannelConfig(int channelNumber);
    // 获取所有测试通道配置
    QList<QMap<QString, QVariant>> getAllTestChannelConfigs();
    // 判断是否存在测试通道配置
    bool hasTestChannelConfig(int channelNumber);

private:
    // 私有构造函数（单例模式）
    DatabaseManager();
    ~DatabaseManager();

    // 禁止拷贝构造和赋值运算符
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    static DatabaseManager* m_instance;
    QSqlDatabase m_database;
    QString m_lastError;
};

#endif // DATABASEMANAGER_H
