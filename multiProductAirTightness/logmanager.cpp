#include "logmanager.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>

// 初始化单例实例
LogManager* LogManager::m_instance = nullptr;

LogManager::LogManager(QObject *parent) : QObject(parent)
{
    m_logFile = nullptr;
    m_textStream = nullptr;
    m_logLevel = LogLevel::DEBUG;
    
    // 初始化配置对象
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/system.ini";
    m_settings = new QSettings(configPath, QSettings::IniFormat);
    
    // 加载日志设置
    loadLogSettings();
}

LogManager::~LogManager()
{
    closeLogFile();
    delete m_settings;
    m_instance = nullptr;
}

LogManager* LogManager::getInstance()
{
    if (m_instance == nullptr) {
        m_instance = new LogManager();
    }
    return m_instance;
}

bool LogManager::init()
{
    // 确保日志目录存在
    QDir logDir(m_logPath);
    if (!logDir.exists()) {
        if (!logDir.mkpath(m_logPath)) {
            qWarning() << "创建日志目录失败:" << m_logPath;
            return false;
        }
    }
    
    // 打开日志文件
    return openLogFile();
}

void LogManager::debug(const QString& message, const QString& module)
{
    if (m_logLevel <= LogLevel::DEBUG) {
        writeLog(LogLevel::DEBUG, message, module);
    }
}

void LogManager::info(const QString& message, const QString& module)
{
    if (m_logLevel <= LogLevel::INFO) {
        writeLog(LogLevel::INFO, message, module);
    }
}

void LogManager::warning(const QString& message, const QString& module)
{
    if (m_logLevel <= LogLevel::WARNING) {
        writeLog(LogLevel::WARNING, message, module);
    }
}

void LogManager::error(const QString& message, const QString& module)
{
    if (m_logLevel <= LogLevel::LOG_ERROR) {
        writeLog(LogLevel::LOG_ERROR, message, module);
    }
}

void LogManager::fatal(const QString& message, const QString& module)
{
    if (m_logLevel <= LogLevel::FATAL) {
        writeLog(LogLevel::FATAL, message, module);
    }
}

void LogManager::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

LogLevel LogManager::getLogLevel() const
{
    return m_logLevel;
}

void LogManager::updateLogPath()
{
    QMutexLocker locker(&m_mutex);
    
    // 重新加载日志设置
    loadLogSettings();
    
    // 关闭并重新打开日志文件
    closeLogFile();
    openLogFile();
}

void LogManager::writeLog(LogLevel level, const QString& message, const QString& module)
{
    QMutexLocker locker(&m_mutex);
    
    // 检查日志文件是否需要切换（按天分割）
    QString newLogFileName = getCurrentLogFileName();
    if (newLogFileName != m_currentLogFileName) {
        closeLogFile();
        openLogFile();
    }
    
    // 如果文件未打开，尝试重新打开
    if (m_logFile == nullptr || !m_logFile->isOpen()) {
        if (!openLogFile()) {
            qWarning() << "打开日志文件失败，使用qDebug替代:" << message;
            qDebug() << QString("[%1][%2][%3] %4").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
                        .arg(logLevelToString(level))
                        .arg(module)
                        .arg(message);
            return;
        }
    }
    
    // 格式化日志消息
    QString logMessage = QString("[%1][%2][%3] %4\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
                                                     .arg(logLevelToString(level))
                                                     .arg(module)
                                                     .arg(message);
    
    // 写入日志
    *m_textStream << logMessage;
    m_textStream->flush();
    
    // 同时输出到调试窗口
    qDebug() << logMessage.trimmed();
}

QString LogManager::getCurrentLogFileName()
{
    // 按天分割日志文件
    return QString("log_%1.log").arg(QDateTime::currentDateTime().toString("yyyyMMdd"));
}

bool LogManager::openLogFile()
{
    // 获取当前日志文件名
    m_currentLogFileName = getCurrentLogFileName();
    QString fullPath = m_logPath + "/" + m_currentLogFileName;
    
    // 创建日志文件
    m_logFile = new QFile(fullPath);
    if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "打开日志文件失败:" << fullPath;
        delete m_logFile;
        m_logFile = nullptr;
        return false;
    }
    
    // 创建文本流并明确设置为UTF-8编码
    m_textStream = new QTextStream(m_logFile);
    m_textStream->setEncoding(QStringConverter::Utf8);
    
    // 写入日志头
    *m_textStream << "\n=== 日志会话开始: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << " ===\n";
    m_textStream->flush();
    
    return true;
}

void LogManager::closeLogFile()
{
    if (m_textStream != nullptr) {
        delete m_textStream;
        m_textStream = nullptr;
    }
    
    if (m_logFile != nullptr) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }
}

QString LogManager::logLevelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARNING:
        return "WARNING";
    case LogLevel::LOG_ERROR:
        return "ERROR";
    case LogLevel::FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

LogLevel LogManager::stringToLogLevel(const QString& levelStr)
{
    QString lowerStr = levelStr.toLower();
    if (lowerStr == "debug") {
        return LogLevel::DEBUG;
    } else if (lowerStr == "info") {
        return LogLevel::INFO;
    } else if (lowerStr == "warning" || lowerStr == "warn") {
        return LogLevel::WARNING;
    } else if (lowerStr == "error") {
        return LogLevel::LOG_ERROR;
    } else if (lowerStr == "fatal") {
        return LogLevel::FATAL;
    } else {
        return LogLevel::DEBUG; // 默认级别
    }
}

void LogManager::loadLogSettings()
{
    m_settings->beginGroup("LogBackup");
    
    // 获取日志路径，默认值与SystemSetting保持一致
    m_logPath = m_settings->value("LogPath", "D:/AirTightnessData/Logs").toString();
    
    // 获取日志级别
    QString logLevelStr = m_settings->value("LogLevel", "DEBUG").toString();
    m_logLevel = stringToLogLevel(logLevelStr);
    
    m_settings->endGroup();
}
