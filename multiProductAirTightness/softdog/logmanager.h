#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QSettings>
#include <QDir>
#include <QMutex>

// 日志级别枚举
enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARNING,
    LOG_ERROR,
    FATAL
};

class LogManager : public QObject
{
    Q_OBJECT

public:
    // 获取单例实例
    static LogManager* getInstance();

    // 初始化日志系统
    bool init();

    // 日志输出方法
    void debug(const QString& message, const QString& module = "");
    void info(const QString& message, const QString& module = "");
    void warning(const QString& message, const QString& module = "");
    void error(const QString& message, const QString& module = "");
    void fatal(const QString& message, const QString& module = "");

    // 设置日志级别
    void setLogLevel(LogLevel level);

    // 获取当前日志级别
    LogLevel getLogLevel() const;

    // 更新日志路径（当系统设置改变时调用）
    void updateLogPath();

private:
    // 私有构造函数（单例模式）
    explicit LogManager(QObject *parent = nullptr);
    ~LogManager();

    // 禁止拷贝和赋值
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    // 写入日志
    void writeLog(LogLevel level, const QString& message, const QString& module);

    // 获取当前日志文件名
    QString getCurrentLogFileName();

    // 打开日志文件
    bool openLogFile();

    // 关闭日志文件
    void closeLogFile();

    // 获取日志级别字符串
    QString logLevelToString(LogLevel level);

    // 从字符串获取日志级别
    LogLevel stringToLogLevel(const QString& levelStr);

    // 加载日志设置
    void loadLogSettings();

private:
    static LogManager* m_instance;  // 单例实例
    QMutex m_mutex;                 // 互斥锁，保证线程安全
    QFile* m_logFile;               // 日志文件
    QTextStream* m_textStream;      // 文本流
    LogLevel m_logLevel;            // 当前日志级别
    QString m_logPath;              // 日志存储路径
    QString m_currentLogFileName;   // 当前日志文件名
    QSettings* m_settings;          // 配置对象
};

// 全局日志宏，方便使用
#define LOG_DEBUG(msg, module) LogManager::getInstance()->debug(msg, module)
#define LOG_INFO(msg, module) LogManager::getInstance()->info(msg, module)
#define LOG_WARNING(msg, module) LogManager::getInstance()->warning(msg, module)
#define LOG_ERROR(msg, module) LogManager::getInstance()->error(msg, module)
#define LOG_FATAL(msg, module) LogManager::getInstance()->fatal(msg, module)

#endif // LOGMANAGER_H
