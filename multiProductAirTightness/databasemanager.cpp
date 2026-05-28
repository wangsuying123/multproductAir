#include "databasemanager.h"
#include "logmanager.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QDir>
#include <QDate>
#include <QStandardPaths>
#include <QCoreApplication>

// 初始化静态实例
DatabaseManager* DatabaseManager::m_instance = nullptr;

DatabaseManager::DatabaseManager()
{
    m_lastError = "";
}

DatabaseManager::~DatabaseManager()
{
    closeDatabase();
}

DatabaseManager* DatabaseManager::getInstance()
{
    if (m_instance == nullptr) {
        m_instance = new DatabaseManager();
    }
    return m_instance;
}

QString DatabaseManager::getDatabasePath()
{
    QString appPath = QCoreApplication::applicationDirPath();
    
    QDir dir(appPath);
    if (!dir.exists()) {
        dir.mkpath(appPath);
    }
    
    QString dbPath = appPath + "/airtight.db";
    qDebug() << "数据库路径：" << dbPath;
    return dbPath;
}

bool DatabaseManager::connectDatabase(const QString& databasePath)
{
    QString dbPath = databasePath.isEmpty() ? getDatabasePath() : databasePath;
    
    if (m_database.isOpen() && m_database.databaseName() == dbPath) {
        return true;
    }
    
    LOG_INFO(QString("尝试连接数据库: %1").arg(dbPath), "数据库");

    if (m_database.isValid()) {
        QString connectionName = m_database.connectionName();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    }

    QFileInfo fileInfo(dbPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        LOG_WARNING(QString("数据库目录不存在，尝试创建: %1").arg(dir.absolutePath()), "数据库");
        if (!dir.mkpath(".")) {
            LOG_ERROR(QString("创建数据库目录失败: %1").arg(dir.absolutePath()), "数据库");
        }
    }

    m_database = QSqlDatabase::addDatabase("QSQLITE");
    m_database.setDatabaseName(dbPath);

    if (!m_database.open()) {
        m_lastError = m_database.lastError().text();
        LOG_ERROR(QString("数据库连接失败: %1 Error opening database").arg(m_lastError), "数据库");
        qDebug() << "Database connection failed:" << m_lastError;
        return false;
    }

    LOG_INFO(QString("数据库连接成功: %1").arg(dbPath), "数据库");
    qDebug() << "Database connected successfully:" << dbPath;
    return true;
}

bool DatabaseManager::initializeDatabase()
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    // 创建气密仪参数表
    QString createTableQuery = "CREATE TABLE IF NOT EXISTS t_air_tightness_full_params ( " 
                              "id INTEGER PRIMARY KEY AUTOINCREMENT, " 
                              "param_name TEXT NOT NULL, " 
                              "program_number INTEGER DEFAULT 1, " 
                              "fill_time REAL, " 
                              "stabilization_time REAL, " 
                              "test_time REAL, " 
                              "dump_time REAL, " 
                              "pressure_unit INTEGER, " 
                              "pressure_max REAL, " 
                              "pressure_min REAL, " 
                              "pressure_set_fill REAL, " 
                              "fill_type INTEGER, " 
                              "leak_unit INTEGER, " 
                              "leak_unit2 INTEGER, " 
                              "test_reject REAL, " 
                              "ref_reject REAL, " 
                              "offset REAL, " 
                              "std_atm REAL, " 
                              "std_temp REAL, " 
                              "volume REAL, " 
                              "volume_unit INTEGER, " 
                              "reject_calc INTEGER, " 
                              "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, " 
                              "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

    QSqlQuery query(m_database);
    if (!query.exec(createTableQuery)) {
        m_lastError = query.lastError().text();
        qDebug() << "Table creation failed:" << m_lastError;
        return false;
    }

    // 创建通信参数表
    QString createCommParamsTableQuery = "CREATE TABLE IF NOT EXISTS t_communication_params (" 
                                       "id INTEGER PRIMARY KEY AUTOINCREMENT," 
                                       "device_type INTEGER NOT NULL," 
                                       "protocol TEXT NOT NULL," 
                                       "ip_address TEXT," 
                                       "port INTEGER," 
                                       "slave_address INTEGER," 
                                       "serial_port TEXT," 
                                       "baudrate INTEGER," 
                                       "parity TEXT," 
                                       "data_bits INTEGER," 
                                       "stop_bits TEXT," 
                                       "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP," 
                                       "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

    if (!query.exec(createCommParamsTableQuery)) {
        m_lastError = query.lastError().text();
        qDebug() << "Communication params table creation failed:" << m_lastError;
        return false;
    }

    // 创建用户表
    if (!createUserTable()) {
        qDebug() << "User table creation failed";
        return false;
    }

    // 创建测试结果表
    if (!createTestResultTable()) {
        qDebug() << "Test result table creation failed";
        return false;
    }
    
    // 创建操作日志表
    if (!createOperationLogTable()) {
        qDebug() << "Operation log table creation failed";
        return false;
    }
    
    // 创建测试通道配置表
    if (!createTestChannelConfigTable()) {
        qDebug() << "Test channel config table creation failed";
        return false;
    }

    qDebug() << "Database initialized successfully";
    return true;
}

bool DatabaseManager::addRecord(const QString& tableName, const QMap<QString, QVariant>& data)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        qDebug() << "Add record failed: Database is not connected";
        return false;
    }

    QStringList keys = data.keys();
    QStringList values;
    QStringList placeholders;

    // 准备插入语句
    for (const QString& key : keys) {
        values << key;
        placeholders << ":" + key;
    }

    QString queryString = "INSERT INTO " + tableName + " (" 
                        + values.join(", ") 
                        + ") VALUES (" 
                        + placeholders.join(", ") 
                        + ")";

    // 调试：打印SQL语句和参数
    qDebug() << "addRecord SQL:" << queryString;
    qDebug() << "addRecord table:" << tableName << "fields count:" << keys.size();

    QSqlQuery query(m_database);
    if (!query.prepare(queryString)) {
        m_lastError = query.lastError().text();
        qDebug() << "Add record prepare failed:" << m_lastError;
        return false;
    }

    // 绑定参数
    for (const QString& key : keys) {
        query.bindValue(":" + key, data.value(key));
        qDebug() << "Binding:" << key << "=" << data.value(key);
    }

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qDebug() << "Add record exec failed:" << m_lastError;
        return false;
    }

    return true;
}

bool DatabaseManager::updateRecord(const QString& tableName, const QMap<QString, QVariant>& data, const QString& condition)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        qDebug() << "Update record failed: Database is not connected";
        return false;
    }

    QStringList updateClauses;
    QStringList keys = data.keys();

    for (const QString& key : keys) {
        updateClauses << key + " = :" + key;
    }

    QString queryString = "UPDATE " + tableName + " SET " 
                        + updateClauses.join(", ");

    if (!condition.isEmpty()) {
        queryString += " WHERE " + condition;
    }

    qDebug() << "updateRecord SQL:" << queryString;

    QSqlQuery query(m_database);
    if (!query.prepare(queryString)) {
        m_lastError = query.lastError().text();
        qDebug() << "Update record prepare failed:" << m_lastError;
        return false;
    }

    for (const QString& key : keys) {
        query.bindValue(":" + key, data.value(key));
        qDebug() << "Binding:" << key << "=" << data.value(key);
    }

    m_database.transaction();
    if (!query.exec()) {
        m_database.rollback();
        m_lastError = query.lastError().text();
        qDebug() << "Update record exec failed:" << m_lastError;
        return false;
    }
    
    if (!m_database.commit()) {
        m_database.rollback();
        m_lastError = m_database.lastError().text();
        qDebug() << "Update record commit failed:" << m_lastError;
        return false;
    }

    qDebug() << "Update record successful, rows affected:" << query.numRowsAffected();
    return true;
}

bool DatabaseManager::deleteRecord(const QString& tableName, const QString& condition)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QString queryString = "DELETE FROM " + tableName;
    if (!condition.isEmpty()) {
        queryString += " WHERE " + condition;
    }

    QSqlQuery query(m_database);
    if (!query.exec(queryString)) {
        m_lastError = query.lastError().text();
        qDebug() << "Delete record failed:" << m_lastError;
        return false;
    }

    return true;
}

int DatabaseManager::getRecordCount(const QString& tableName, const QString& condition)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return -1;
    }

    QString queryString = "SELECT COUNT(*) FROM " + tableName;
    if (!condition.isEmpty()) {
        queryString += " WHERE " + condition;
    }

    QSqlQuery query(m_database);
    if (!query.exec(queryString)) {
        m_lastError = query.lastError().text();
        qDebug() << "Count records failed:" << m_lastError;
        return -1;
    }

    if (query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

QSqlQuery DatabaseManager::executeQuery(const QString& queryString, const QList<QVariant>& bindings)
{
    QSqlQuery query(m_database);

    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        qDebug() << "Execute query failed: Database is not connected";
        return query;
    }

    // 调试：打印SQL语句和参数数量
    qDebug() << "executeQuery SQL:" << queryString;
    qDebug() << "executeQuery bindings count:" << bindings.size();

    if (!query.prepare(queryString)) {
        m_lastError = query.lastError().text();
        qDebug() << "Execute query prepare failed:" << m_lastError;
        return query;
    }

    // 绑定参数
    for (int i = 0; i < bindings.size(); ++i) {
        query.bindValue(i, bindings.at(i));
        qDebug() << "Binding index" << i << "=" << bindings.at(i);
    }

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qDebug() << "Execute query exec failed:" << m_lastError;
    }

    return query;
}

QString DatabaseManager::getLastError() const
{
    return m_lastError;
}

void DatabaseManager::closeDatabase()
{
    if (m_database.isOpen()) {
        m_database.close();
        qDebug() << "Database closed";
    }
}

// 保存气密仪参数
bool DatabaseManager::saveAirTightnessParams(const QMap<QString, QVariant>& params)
{
    return addRecord("t_air_tightness_full_params", params);
}

// 修改气密仪参数
bool DatabaseManager::updateAirTightnessParams(int id, const QMap<QString, QVariant>& params)
{
    QMap<QString, QVariant> updateParams = params;
    updateParams["updated_at"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    return updateRecord("t_air_tightness_full_params", updateParams, QString("id = %1").arg(id));
}

// 删除气密仪参数
bool DatabaseManager::deleteAirTightnessParams(int id)
{
    return deleteRecord("t_air_tightness_full_params", QString("id = %1").arg(id));
}

// 根据ID获取气密仪参数
QMap<QString, QVariant> DatabaseManager::getAirTightnessParamsById(int id)
{
    QMap<QString, QVariant> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    QString queryString = "SELECT * FROM t_air_tightness_full_params WHERE id = ?";
    QSqlQuery query = executeQuery(queryString, {id});

    if (query.next()) {
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            result[fieldName] = value;
        }
    }

    return result;
}

// 根据程序号获取气密仪参数
QList<QMap<QString, QVariant>> DatabaseManager::getAirTightnessParamsByProgram(int programNumber)
{
    QList<QMap<QString, QVariant>> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    QString queryString = "SELECT * FROM t_air_tightness_full_params WHERE program_number = ?";
    QSqlQuery query = executeQuery(queryString, {programNumber});

    while (query.next()) {
        QMap<QString, QVariant> record;
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            record[fieldName] = value;
        }
        result.append(record);
    }

    return result;
}

// 保存通信参数
bool DatabaseManager::saveCommunicationParams(int deviceType, const QMap<QString, QVariant>& params)
{
    QMap<QString, QVariant> saveParams = params;
    saveParams["device_type"] = deviceType;
    return addRecord("t_communication_params", saveParams);
}

// 更新通信参数
bool DatabaseManager::updateCommunicationParams(int deviceType, const QMap<QString, QVariant>& params)
{
    QMap<QString, QVariant> updateParams = params;
    updateParams["updated_at"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    return updateRecord("t_communication_params", updateParams, QString("device_type = %1").arg(deviceType));
}

// 获取通信参数
QMap<QString, QVariant> DatabaseManager::getCommunicationParams(int deviceType)
{
    QMap<QString, QVariant> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    QString queryString = "SELECT * FROM t_communication_params WHERE device_type = ?";
    QSqlQuery query = executeQuery(queryString, {deviceType});

    if (query.next()) {
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            result[fieldName] = value;
        }
    }

    return result;
}

// 判断是否存在通信参数
bool DatabaseManager::hasCommunicationParams(int deviceType)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QString queryString = "SELECT COUNT(*) FROM t_communication_params WHERE device_type = ?";
    QSqlQuery query = executeQuery(queryString, {deviceType});

    if (query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

// 创建用户表
bool DatabaseManager::createUserTable()
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QString createUserTableQuery = "CREATE TABLE IF NOT EXISTS t_users ( " 
                                 "id INTEGER PRIMARY KEY AUTOINCREMENT," 
                                 "username TEXT NOT NULL UNIQUE," 
                                 "employee_id TEXT," 
                                 "password TEXT NOT NULL," 
                                 "role TEXT NOT NULL," 
                                 "enabled INTEGER NOT NULL DEFAULT 1," 
                                 "permissions TEXT," 
                                 "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP," 
                                 "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

    QSqlQuery query(m_database);
    if (!query.exec(createUserTableQuery)) {
        m_lastError = query.lastError().text();
        qDebug() << "User table creation failed:" << m_lastError;
        return false;
    }
    
    // 检查permissions列是否存在，如果不存在则添加（用于升级现有数据库）
    QString checkColumnQuery = "PRAGMA table_info(t_users)";
    query = executeQuery(checkColumnQuery);
    bool hasPermissionsColumn = false;
    while (query.next()) {
        QString columnName = query.value(1).toString();
        if (columnName == "permissions") {
            hasPermissionsColumn = true;
            break;
        }
    }
    
    if (!hasPermissionsColumn) {
        qDebug() << "Adding permissions column to t_users table...";
        QString addColumnQuery = "ALTER TABLE t_users ADD COLUMN permissions TEXT";
        QSqlQuery alterQuery(m_database);
        if (!alterQuery.exec(addColumnQuery)) {
            m_lastError = alterQuery.lastError().text();
            qDebug() << "Failed to add permissions column:" << m_lastError;
            // 不返回false，因为表已经创建成功，只是添加列失败
        } else {
            qDebug() << "Permissions column added successfully";
        }
    }

    // 检查admin用户是否存在
    int userCount = getRecordCount("t_users");
    qDebug() << "当前用户表记录数:" << userCount;
    
    // 检查admin用户是否存在
    QMap<QString, QVariant> adminUser = getUserByUsername("admin");
    if (adminUser.isEmpty()) {
        qDebug() << "admin用户不存在，正在创建默认管理员用户...";
        int userId = addUser("admin", "", "admin123", "管理员");
        if (userId > 0) {
            qDebug() << "默认管理员用户创建成功，ID:" << userId;
        } else {
            qDebug() << "默认管理员用户创建失败:" << m_lastError;
        }
    } else {
        qDebug() << "admin用户已存在，ID:" << adminUser["id"].toInt();
    }
    
    // 检查张三用户是否存在
    QMap<QString, QVariant> zhangSanUser = getUserByUsername("张三");
    if (zhangSanUser.isEmpty()) {
        qDebug() << "张三用户不存在，正在创建默认操作员用户...";
        int userId = addUser("张三", "", "123456", "操作员");
        if (userId > 0) {
            qDebug() << "默认操作员用户（张三）创建成功，ID:" << userId;
        } else {
            qDebug() << "默认操作员用户（张三）创建失败:" << m_lastError;
        }
    } else {
        qDebug() << "张三用户已存在，ID:" << zhangSanUser["id"].toInt();
    }

    return true;
}

// 密码加密
QString DatabaseManager::encryptPassword(const QString& password)
{
    // 使用QCryptographicHash进行密码加密
    // 注意：在实际项目中，应该使用更安全的加密方式，如bcrypt
    QByteArray passwordData = password.toUtf8();
    QByteArray hashData = QCryptographicHash::hash(passwordData, QCryptographicHash::Sha256);
    return hashData.toHex();
}

// 密码验证
bool DatabaseManager::verifyPassword(const QString& password, const QString& hashedPassword)
{
    QString hashedInput = encryptPassword(password);
    return hashedInput == hashedPassword;
}

// 添加用户
int DatabaseManager::addUser(const QString& username, const QString& employeeId, const QString& password, const QString& role, bool enabled)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return -1;
    }

    QMap<QString, QVariant> userData;
    userData["username"] = username;
    userData["employee_id"] = employeeId;
    userData["password"] = encryptPassword(password);
    userData["role"] = role;
    userData["enabled"] = enabled ? 1 : 0;

    if (addRecord("t_users", userData)) {
        QSqlQuery query = executeQuery("SELECT last_insert_rowid()");
        if (query.next()) {
            return query.value(0).toInt();
        }
    }
    return -1;
}

// 更新用户
bool DatabaseManager::updateUser(int id, const QString& username, const QString& employeeId, const QString& password, const QString& role, bool enabled)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QMap<QString, QVariant> userData;
    userData["username"] = username;
    userData["employee_id"] = employeeId;
    if (!password.isEmpty()) {
        userData["password"] = encryptPassword(password);
    }
    userData["role"] = role;
    userData["enabled"] = enabled ? 1 : 0;
    userData["updated_at"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    return updateRecord("t_users", userData, QString("id = %1").arg(id));
}

// 删除用户
bool DatabaseManager::deleteUser(int id)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    return deleteRecord("t_users", QString("id = %1").arg(id));
}

// 根据ID获取用户
QMap<QString, QVariant> DatabaseManager::getUserById(int id)
{
    QMap<QString, QVariant> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    QString queryString = "SELECT * FROM t_users WHERE id = ?";
    QSqlQuery query = executeQuery(queryString, {id});

    if (query.next()) {
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            result[fieldName] = value;
        }
    }

    return result;
}

// 根据用户名获取用户
QMap<QString, QVariant> DatabaseManager::getUserByUsername(const QString& username)
{
    QMap<QString, QVariant> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    QString queryString = "SELECT * FROM t_users WHERE username = ?";
    QSqlQuery query = executeQuery(queryString, {username});

    if (query.next()) {
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            result[fieldName] = value;
        }
    }

    return result;
}

// 获取所有用户
QList<QMap<QString, QVariant>> DatabaseManager::getAllUsers()
{
    QList<QMap<QString, QVariant>> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    QString queryString = "SELECT * FROM t_users ORDER BY id";
    QSqlQuery query = executeQuery(queryString);

    while (query.next()) {
        QMap<QString, QVariant> userRecord;
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            userRecord[fieldName] = value;
        }
        result.append(userRecord);
    }

    return result;
}

// 更新用户状态
bool DatabaseManager::updateUserStatus(int id, bool enabled)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QMap<QString, QVariant> userData;
    userData["enabled"] = enabled ? 1 : 0;
    userData["updated_at"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    return updateRecord("t_users", userData, QString("id = %1").arg(id));
}

// 创建操作日志表
bool DatabaseManager::createOperationLogTable()
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QString createLogTableQuery = "CREATE TABLE IF NOT EXISTS t_operation_logs ( " \
                               "id INTEGER PRIMARY KEY AUTOINCREMENT, " \
                               "operator_name TEXT NOT NULL, " \
                               "operation TEXT NOT NULL, " \
                               "module TEXT NOT NULL, " \
                               "details TEXT, " \
                               "success INTEGER NOT NULL, " \
                               "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

    QSqlQuery query(m_database);
    if (!query.exec(createLogTableQuery)) {
        m_lastError = query.lastError().text();
        qDebug() << "Operation log table creation failed:" << m_lastError;
        return false;
    }

    return true;
}

// 添加操作日志
bool DatabaseManager::addOperationLog(const QString& operatorName, const QString& operation, const QString& module, const QString& details, bool success)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QMap<QString, QVariant> logData;
    logData["operator_name"] = operatorName;
    logData["operation"] = operation;
    logData["module"] = module;
    logData["details"] = details;
    logData["success"] = success ? 1 : 0;
    
    return addRecord("t_operation_logs", logData);
}

// 查询操作日志（支持条件查询和分页）
QList<QMap<QString, QVariant>> DatabaseManager::getOperationLogs(const QString& operatorName, const QString& module, const QDateTime& startTime, const QDateTime& endTime, int page, int pageSize)
{
    QList<QMap<QString, QVariant>> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    QString queryString = "SELECT * FROM t_operation_logs WHERE 1=1";
    QList<QVariant> bindings;
    
    if (!operatorName.isEmpty()) {
        queryString += " AND operator_name LIKE ?";
        bindings.append("%" + operatorName + "%");
    }
    
    if (!module.isEmpty()) {
        queryString += " AND module LIKE ?";
        bindings.append("%" + module + "%");
    }
    
    if (startTime.isValid()) {
        queryString += " AND created_at >= ?";
        bindings.append(startTime.toString("yyyy-MM-dd HH:mm:ss"));
    }
    
    if (endTime.isValid()) {
        queryString += " AND created_at <= ?";
        bindings.append(endTime.toString("yyyy-MM-dd HH:mm:ss"));
    }
    
    queryString += " ORDER BY created_at DESC";
    
    // 添加分页
    if (page > 0 && pageSize > 0) {
        int offset = (page - 1) * pageSize;
        queryString += " LIMIT ? OFFSET ?";
        bindings.append(pageSize);
        bindings.append(offset);
    }
    
    QSqlQuery query = executeQuery(queryString, bindings);
    
    while (query.next()) {
        QMap<QString, QVariant> logRecord;
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            logRecord[fieldName] = value;
        }
        result.append(logRecord);
    }
    
    return result;
}

// 获取操作日志总数
int DatabaseManager::getOperationLogCount(const QString& operatorName, const QString& module, const QDateTime& startTime, const QDateTime& endTime)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return -1;
    }

    QString queryString = "SELECT COUNT(*) FROM t_operation_logs WHERE 1=1";
    QList<QVariant> bindings;
    
    if (!operatorName.isEmpty()) {
        queryString += " AND operator_name LIKE ?";
        bindings.append("%" + operatorName + "%");
    }
    
    if (!module.isEmpty()) {
        queryString += " AND module LIKE ?";
        bindings.append("%" + module + "%");
    }
    
    if (startTime.isValid()) {
        queryString += " AND created_at >= ?";
        bindings.append(startTime.toString("yyyy-MM-dd HH:mm:ss"));
    }
    
    if (endTime.isValid()) {
        queryString += " AND created_at <= ?";
        bindings.append(endTime.toString("yyyy-MM-dd HH:mm:ss"));
    }
    
    QSqlQuery query = executeQuery(queryString, bindings);
    
    if (query.next()) {
        return query.value(0).toInt();
    }
    
    return 0;
}

// 创建测试结果表
bool DatabaseManager::createTestResultTable()
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QString createTestResultTableQuery = "CREATE TABLE IF NOT EXISTS t_test_results ( " 
                                      "id INTEGER PRIMARY KEY AUTOINCREMENT, " 
                                      "serial_number INTEGER, "
                                      "channel INTEGER, "
                                      "product_id TEXT, " 
                                      "operator_name TEXT, " 
                                      "test_result TEXT NOT NULL, " 
                                      "alarm_code TEXT, " 
                                      "pressure_value REAL, " 
                                      "pressure_unit TEXT, " 
                                      "leak_value REAL, " 
                                      "leak_unit TEXT, " 
                                      "standard_atm REAL, " 
                                      "standard_atm_unit TEXT, " 
                                      "temperature REAL, " 
                                      "temperature_unit TEXT, " 
                                      "test_time DATETIME DEFAULT CURRENT_TIMESTAMP)";

    QSqlQuery query(m_database);
    if (!query.exec(createTestResultTableQuery)) {
        m_lastError = query.lastError().text();
        qDebug() << "Test result table creation failed:" << m_lastError;
        return false;
    }

    return true;
}

// 添加测试结果
bool DatabaseManager::addTestResult(const QMap<QString, QVariant>& result)
{
    return addRecord("t_test_results", result);
}

// 查询测试结果（支持条件查询和分页）
QList<QMap<QString, QVariant>> DatabaseManager::getTestResults(const QString& productId, const QString& operatorName, const QString& testResult, const QDateTime& startTime, const QDateTime& endTime, int page, int pageSize)
{
    QList<QMap<QString, QVariant>> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    // 构建查询条件
    QStringList conditions;
    if (!productId.isEmpty()) {
        conditions << QString("product_id LIKE '%%1%'").arg(productId);
    }
    if (!operatorName.isEmpty()) {
        conditions << QString("operator_name LIKE '%%1%'").arg(operatorName);
    }
    if (!testResult.isEmpty()) {
        conditions << QString("test_result = '%1'").arg(testResult);
    }
    // 添加时间范围条件
    if (startTime.isValid()) {
        conditions << QString("test_time >= '%1'").arg(startTime.toString("yyyy-MM-dd HH:mm:ss"));
    }
    if (endTime.isValid()) {
        conditions << QString("test_time <= '%1'").arg(endTime.toString("yyyy-MM-dd HH:mm:ss"));
    }

    QString conditionStr = conditions.isEmpty() ? "" : " WHERE " + conditions.join(" AND ");
    
    // 计算偏移量
    int offset = (page - 1) * pageSize;
    
    QString queryString = "SELECT * FROM t_test_results" + conditionStr + " ORDER BY test_time DESC LIMIT ? OFFSET ?";
    
    // 调试：打印SQL查询语句
    qDebug() << "执行查询：" << queryString;
    qDebug() << "参数：limit=" << pageSize << " offset=" << offset;
    
    QSqlQuery query = executeQuery(queryString, {pageSize, offset});
    
    int resultCount = 0;
    while (query.next()) {
        QMap<QString, QVariant> record;
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            record[fieldName] = value;
        }
        result.append(record);
        resultCount++;
    }
    
    // 调试：打印查询结果数量
    qDebug() << "查询返回结果数量：" << resultCount;
    
    return result;
}

// 获取测试结果总数
int DatabaseManager::getTestResultCount(const QString& productId, const QString& operatorName, const QString& testResult, const QDateTime& startTime, const QDateTime& endTime)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return -1;
    }

    // 构建查询条件
    QStringList conditions;
    if (!productId.isEmpty()) {
        conditions << QString("product_id LIKE '%%1%'").arg(productId);
    }
    if (!operatorName.isEmpty()) {
        conditions << QString("operator_name LIKE '%%1%'").arg(operatorName);
    }
    if (!testResult.isEmpty()) {
        conditions << QString("test_result = '%1'").arg(testResult);
    }
    // 添加时间范围条件
    if (startTime.isValid()) {
        conditions << QString("test_time >= '%1'").arg(startTime.toString("yyyy-MM-dd HH:mm:ss"));
    }
    if (endTime.isValid()) {
        conditions << QString("test_time <= '%1'").arg(endTime.toString("yyyy-MM-dd HH:mm:ss"));
    }

    QString conditionStr = conditions.isEmpty() ? "" : " WHERE " + conditions.join(" AND ");
    
    QString queryString = "SELECT COUNT(*) FROM t_test_results" + conditionStr;
    
    // 调试：打印统计查询语句
    qDebug() << "执行统计查询：" << queryString;
    
    QSqlQuery query = executeQuery(queryString);
    
    if (query.next()) {
        int count = query.value(0).toInt();
        // 调试：打印统计结果
        qDebug() << "统计查询返回结果：" << count;
        return count;
    }
    
    return 0;
}

// 导出测试结果
QList<QMap<QString, QVariant>> DatabaseManager::exportTestResults(const QString& productId, const QString& operatorName, const QString& testResult, const QDateTime& startTime, const QDateTime& endTime)
{
    QList<QMap<QString, QVariant>> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    // 构建查询条件
    QStringList conditions;
    if (!productId.isEmpty()) {
        conditions << QString("product_id LIKE '%%1%'").arg(productId);
    }
    if (!operatorName.isEmpty()) {
        conditions << QString("operator_name LIKE '%%1%'").arg(operatorName);
    }
    if (!testResult.isEmpty()) {
        conditions << QString("test_result = '%1'").arg(testResult);
    }
    // 添加时间范围条件
    if (startTime.isValid()) {
        conditions << QString("test_time >= '%1'").arg(startTime.toString("yyyy-MM-dd HH:mm:ss"));
    }
    if (endTime.isValid()) {
        conditions << QString("test_time <= '%1'").arg(endTime.toString("yyyy-MM-dd HH:mm:ss"));
    }

    QString conditionStr = conditions.isEmpty() ? "" : " WHERE " + conditions.join(" AND ");
    
    QString queryString = "SELECT * FROM t_test_results" + conditionStr + " ORDER BY test_time DESC";
    
    QSqlQuery query = executeQuery(queryString);
    
    while (query.next()) {
        QMap<QString, QVariant> record;
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            record[fieldName] = value;
        }
        result.append(record);
    }
    
    return result;
}

// 获取测试结果统计（测试总数、合格数、不合格数）- 只统计当天数据
QMap<QString, int> DatabaseManager::getTestResultStatistics()
{
    QMap<QString, int> result;
    result["total"] = 0;
    result["pass"] = 0;
    result["fail"] = 0;
    
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }
    
    // 获取当天日期范围（00:00:00 到 23:59:59）
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    QString todayStart = today + " 00:00:00";
    QString todayEnd = today + " 23:59:59";
    
    // 查询当天测试总数
    QString totalQuery = QString("SELECT COUNT(*) FROM t_test_results WHERE test_time >= '%1' AND test_time <= '%2'")
                        .arg(todayStart).arg(todayEnd);
    QSqlQuery query = executeQuery(totalQuery);
    if (query.next()) {
        result["total"] = query.value(0).toInt();
    }
    
    // 查询当天合格数（test_result = '通过'）
    QString passQuery = QString("SELECT COUNT(*) FROM t_test_results WHERE test_result = '通过' AND test_time >= '%1' AND test_time <= '%2'")
                       .arg(todayStart).arg(todayEnd);
    query = executeQuery(passQuery);
    if (query.next()) {
        result["pass"] = query.value(0).toInt();
    }
    
    // 查询当天不合格数（test_result = '不通过'）
    QString failQuery = QString("SELECT COUNT(*) FROM t_test_results WHERE test_result = '不通过' AND test_time >= '%1' AND test_time <= '%2'")
                       .arg(todayStart).arg(todayEnd);
    query = executeQuery(failQuery);
    if (query.next()) {
        result["fail"] = query.value(0).toInt();
    }
    
    return result;
}

// 保存用户权限
bool DatabaseManager::saveUserPermissions(int userId, const QString& permissionsJson)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QMap<QString, QVariant> userData;
    userData["permissions"] = permissionsJson;
    userData["updated_at"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    return updateRecord("t_users", userData, QString("id = %1").arg(userId));
}

// 获取用户权限
QString DatabaseManager::getUserPermissions(int userId)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return QString();
    }

    QString queryString = "SELECT permissions FROM t_users WHERE id = ?";
    QSqlQuery query = executeQuery(queryString, {userId});

    if (query.next()) {
        return query.value(0).toString();
    }

    return QString();
}

// 更新用户权限
bool DatabaseManager::updateUserPermissions(int userId, const QString& permissionsJson)
{
    return saveUserPermissions(userId, permissionsJson);
}

// 创建测试通道配置表
bool DatabaseManager::createTestChannelConfigTable()
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QString createTableQuery = "CREATE TABLE IF NOT EXISTS t_test_channel_config ( " 
                             "id INTEGER PRIMARY KEY AUTOINCREMENT, " 
                             "channel_number INTEGER NOT NULL UNIQUE, " 
                             "is_enabled INTEGER NOT NULL DEFAULT 0, " 
                             "program_number INTEGER NOT NULL DEFAULT 1, " 
                             "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, " 
                             "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

    QSqlQuery query(m_database);
    if (!query.exec(createTableQuery)) {
        m_lastError = query.lastError().text();
        qDebug() << "Test channel config table creation failed:" << m_lastError;
        return false;
    }

    // 初始化默认配置（如果表为空）
    if (getRecordCount("t_test_channel_config") == 0) {
        // 测试1通道：默认关闭，程序号1
        QMap<QString, QVariant> channel1;
        channel1["channel_number"] = 1;
        channel1["is_enabled"] = 0;
        channel1["program_number"] = 1;
        addRecord("t_test_channel_config", channel1);

        // 测试2通道：默认关闭，程序号2
        QMap<QString, QVariant> channel2;
        channel2["channel_number"] = 2;
        channel2["is_enabled"] = 0;
        channel2["program_number"] = 2;
        addRecord("t_test_channel_config", channel2);

        // 测试3通道：默认关闭，程序号3
        QMap<QString, QVariant> channel3;
        channel3["channel_number"] = 3;
        channel3["is_enabled"] = 0;
        channel3["program_number"] = 3;
        addRecord("t_test_channel_config", channel3);
    }

    return true;
}

// 保存测试通道配置
bool DatabaseManager::saveTestChannelConfig(int channelNumber, bool isEnabled, int programNumber)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QMap<QString, QVariant> config;
    config["channel_number"] = channelNumber;
    config["is_enabled"] = isEnabled ? 1 : 0;
    config["program_number"] = programNumber;

    return addRecord("t_test_channel_config", config);
}

bool DatabaseManager::updateTestChannelConfig(int channelNumber, bool isEnabled, int programNumber)
{
    qDebug() << "updateTestChannelConfig called: channel=" << channelNumber << ", isEnabled=" << isEnabled << ", programNumber=" << programNumber;
    
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        qDebug() << "updateTestChannelConfig failed: Database is not connected";
        return false;
    }

    qDebug() << "Database is open, databaseName=" << m_database.databaseName();

    QMap<QString, QVariant> config;
    config["is_enabled"] = isEnabled ? 1 : 0;
    config["program_number"] = programNumber;
    config["updated_at"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    bool result = updateRecord("t_test_channel_config", config, QString("channel_number = %1").arg(channelNumber));
    qDebug() << "updateTestChannelConfig result:" << result;
    if (!result) {
        qDebug() << "updateTestChannelConfig failed with error:" << m_lastError;
    }
    return result;
}

// 获取测试通道配置
QMap<QString, QVariant> DatabaseManager::getTestChannelConfig(int channelNumber)
{
    QMap<QString, QVariant> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    QString queryString = "SELECT * FROM t_test_channel_config WHERE channel_number = ?";
    QSqlQuery query = executeQuery(queryString, {channelNumber});

    if (query.next()) {
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            result[fieldName] = value;
        }
    }

    return result;
}

// 获取所有测试通道配置
QList<QMap<QString, QVariant>> DatabaseManager::getAllTestChannelConfigs()
{
    QList<QMap<QString, QVariant>> result;
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return result;
    }

    QString queryString = "SELECT * FROM t_test_channel_config ORDER BY channel_number";
    QSqlQuery query = executeQuery(queryString);

    while (query.next()) {
        QMap<QString, QVariant> record;
        for (int i = 0; i < query.record().count(); ++i) {
            QString fieldName = query.record().fieldName(i);
            QVariant value = query.value(i);
            record[fieldName] = value;
        }
        result.append(record);
    }

    return result;
}

// 判断是否存在测试通道配置
bool DatabaseManager::hasTestChannelConfig(int channelNumber)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not connected";
        return false;
    }

    QString queryString = "SELECT COUNT(*) FROM t_test_channel_config WHERE channel_number = ?";
    QSqlQuery query = executeQuery(queryString, {channelNumber});

    if (query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}
