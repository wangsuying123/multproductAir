#include "programprotector.h"
#include "donglemanager.h"
#include "../logmanager.h"

#include <QCoreApplication>
#include <QMessageBox>
#include <QMutexLocker>
#include <QStorageInfo>
#include <QFileInfo>
#include <QDateTime>

ProgramProtector::ProgramProtector(QObject *parent)
    : QObject(parent),
      m_checkTimer(nullptr),
      m_dongleManager(nullptr),
      m_failureCount(0),
      m_maxFailures(3),      // 默认允许连续3次失败
      m_initialized(false),
      m_lastFailureReason("")
{
    m_checkTimer = new QTimer(this);
    connect(m_checkTimer, &QTimer::timeout, this, &ProgramProtector::onPeriodicCheck);
}

ProgramProtector::~ProgramProtector()
{
    stopPeriodicChecks();
    
    if (m_dongleManager) {
        delete m_dongleManager;
        m_dongleManager = nullptr;
    }
}

bool ProgramProtector::initialize()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    // 创建加密狗管理器
    m_dongleManager = new DongleManager(this);
    
    // 进行首次授权检查
    if (!checkDongleAuthorization()) {
        LOG_WARNING("初始化时授权检查失败，但允许继续（将在定期检查中重试）", "程序保护");
    }
    
    m_initialized = true;
    LOG_INFO("程序保护机制初始化成功", "程序保护");
    
    return true;
}

void ProgramProtector::startPeriodicChecks(int intervalMs)
{
    if (!m_initialized) {
        LOG_WARNING("程序保护未初始化，无法启动定期检查", "程序保护");
        return;
    }
    
    if (m_checkTimer->isActive()) {
        m_checkTimer->stop();
    }
    
    // 最小间隔10秒，避免过于频繁的检查
    if (intervalMs < 10000) {
        intervalMs = 10000;
    }
    
    m_checkTimer->start(intervalMs);
    LOG_INFO(QString("定期保护检查已启动，间隔：%1秒").arg(intervalMs / 1000), "程序保护");
}

void ProgramProtector::stopPeriodicChecks()
{
    if (m_checkTimer && m_checkTimer->isActive()) {
        m_checkTimer->stop();
        LOG_INFO("定期保护检查已停止", "程序保护");
    }
}

bool ProgramProtector::checkProgramIntegrity()
{
    return checkDongleAuthorization();
}

void ProgramProtector::setMaxFailures(int count)
{
    if (count > 0) {
        m_maxFailures = count;
    }
}

int ProgramProtector::getCurrentFailureCount() const
{
    return m_failureCount;
}

void ProgramProtector::onPeriodicCheck()
{
    QMutexLocker locker(&m_mutex);
    
    QString checkTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    LOG_DEBUG(QString("[%1] 执行定期授权检查...").arg(checkTime), "程序保护");
    
    bool authorized = false;
    QString failureReason;
    
    // 使用try-catch防止异常导致程序崩溃
    try {
        authorized = checkDongleAuthorization(failureReason);
    } catch (const std::exception &e) {
        failureReason = QString("授权检查异常：%1").arg(e.what());
        LOG_ERROR(failureReason, "程序保护");
        authorized = false;
    } catch (...) {
        failureReason = "授权检查发生未知异常";
        LOG_ERROR(failureReason, "程序保护");
        authorized = false;
    }
    
    if (authorized) {
        // 检查通过，重置失败计数
        if (m_failureCount > 0) {
            LOG_INFO(QString("[%1] 授权检查恢复正常，重置失败计数（之前：%2）").arg(checkTime).arg(m_failureCount), "程序保护");
        }
        m_failureCount = 0;
        m_lastFailureReason.clear();
        emit authorizationSuccess();
    } else {
        // 检查失败，增加失败计数
        m_failureCount++;
        m_lastFailureReason = failureReason;
        
        // 详细记录失败信息
        LOG_WARNING(QString("[%1] 授权检查失败 (%2/%3)").arg(checkTime).arg(m_failureCount).arg(m_maxFailures), "程序保护");
        LOG_WARNING(QString("失败原因：%1").arg(failureReason), "程序保护");
        
        emit authorizationFailed(QString("授权检查失败 (%1/%2): %3").arg(m_failureCount).arg(m_maxFailures).arg(failureReason));
        
        // 只有连续多次失败才终止程序
        if (m_failureCount >= m_maxFailures) {
            LOG_ERROR("========== 授权检查连续失败，程序即将终止 ==========", "程序保护");
            LOG_ERROR(QString("连续失败次数：%1").arg(m_failureCount), "程序保护");
            LOG_ERROR(QString("最后失败原因：%1").arg(failureReason), "程序保护");
            LOG_ERROR(QString("终止时间：%1").arg(checkTime), "程序保护");
            LOG_ERROR("=================================================", "程序保护");
            
            terminateProgram(failureReason);
        }
    }
}

bool ProgramProtector::checkDongleAuthorization(QString &failureReason)
{
    if (!m_dongleManager) {
        failureReason = "加密狗管理器未初始化";
        LOG_ERROR(failureReason, "程序保护");
        return false;
    }
    
    // ========== 第1步：获取程序路径和存储设备信息 ==========
    QString programPath = m_dongleManager->getCurrentProgramPath();
    QFileInfo programFile(programPath);
    QString programDir = programFile.absolutePath();
    
    // 刷新存储信息，确保获取最新状态
    QStorageInfo programStorage(programDir);
    programStorage.refresh();
    
    LOG_DEBUG(QString("程序路径：%1").arg(programPath), "程序保护");
    LOG_DEBUG(QString("程序所在存储设备：%1").arg(programStorage.rootPath()), "程序保护");
    
    // ========== 第2步：检查存储设备是否可用 ==========
    // 这是关键检查：如果U盘被拔出，存储设备会变得不可用
    if (!programStorage.isValid()) {
        failureReason = QString("存储设备无效 - U盘可能已被拔出 (路径: %1)").arg(programDir);
        LOG_ERROR(failureReason, "程序保护");
        return false;
    }
    
    if (!programStorage.isReady()) {
        failureReason = QString("存储设备暂时未就绪 (设备: %1)").arg(programStorage.rootPath());
        LOG_WARNING(failureReason, "程序保护");
        // 未就绪可能是临时性问题，返回true让容错机制处理
        return true;
    }
    
    // ========== 第3步：检查程序文件是否仍然存在 ==========
    // 如果U盘被拔出，程序文件将不可访问
    if (!programFile.exists()) {
        failureReason = QString("程序文件不存在 - U盘可能已被拔出 (文件: %1)").arg(programPath);
        LOG_ERROR(failureReason, "程序保护");
        return false;
    }
    
    // ========== 第4步：验证存储设备是否是授权的加密狗 ==========
    try {
        if (!m_dongleManager->isStorageAuthorized(programStorage)) {
            failureReason = QString("程序不在授权的加密狗上运行 (当前位置: %1)").arg(programStorage.rootPath());
            LOG_WARNING(failureReason, "程序保护");
            return false;
        }
    } catch (const std::exception &e) {
        failureReason = QString("检查存储设备授权时发生异常: %1").arg(e.what());
        LOG_WARNING(failureReason, "程序保护");
        return false;
    } catch (...) {
        failureReason = "检查存储设备授权时发生未知异常";
        LOG_WARNING(failureReason, "程序保护");
        return false;
    }
    
    // ========== 第5步：验证授权文件是否仍然存在且可读 ==========
    QString rootPath = programStorage.rootPath();
    if (!rootPath.endsWith("/") && !rootPath.endsWith("\\")) {
        rootPath += "/";
    }
    QString authFilePath = rootPath + ".program_auth";
    QFileInfo authFile(authFilePath);
    
    if (!authFile.exists()) {
        failureReason = QString("授权文件不存在 - 可能被删除或U盘已被拔出 (文件: %1)").arg(authFilePath);
        LOG_ERROR(failureReason, "程序保护");
        return false;
    }
    
    LOG_DEBUG("程序在授权的加密狗上运行，验证通过", "程序保护");
    failureReason.clear();
    return true;
}

// 保持旧接口兼容
bool ProgramProtector::checkDongleAuthorization()
{
    QString reason;
    return checkDongleAuthorization(reason);
}

void ProgramProtector::terminateProgram(const QString &reason)
{
    emit programTerminating();
    
    QString terminateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    
    // 详细记录终止信息到日志
    LOG_ERROR("╔══════════════════════════════════════════════════════════╗", "程序保护");
    LOG_ERROR("║              程序授权验证失败 - 即将终止                    ║", "程序保护");
    LOG_ERROR("╠══════════════════════════════════════════════════════════╣", "程序保护");
    LOG_ERROR(QString("║ 终止时间: %1").arg(terminateTime), "程序保护");
    LOG_ERROR(QString("║ 失败原因: %1").arg(reason), "程序保护");
    LOG_ERROR(QString("║ 连续失败: %1 次").arg(m_failureCount), "程序保护");
    LOG_ERROR("╠══════════════════════════════════════════════════════════╣", "程序保护");
    LOG_ERROR("║ 解决方案:                                                 ║", "程序保护");
    LOG_ERROR("║   1. 确保程序从授权的U盘运行                               ║", "程序保护");
    LOG_ERROR("║   2. 检查U盘是否正确插入                                   ║", "程序保护");
    LOG_ERROR("║   3. 检查授权文件(.program_auth)是否存在                   ║", "程序保护");
    LOG_ERROR("║   4. 如需重新授权，请使用授权工具                           ║", "程序保护");
    LOG_ERROR("╚══════════════════════════════════════════════════════════╝", "程序保护");
    
    // 显示详细的错误消息框
    QString detailMessage = QString(
        "程序授权检查失败，程序即将退出！\n\n"
        "【失败原因】\n%1\n\n"
        "【失败详情】\n"
        "• 连续失败次数: %2 次\n"
        "• 终止时间: %3\n\n"
        "【解决方案】\n"
        "1. 确保程序从授权的U盘运行\n"
        "2. 检查U盘是否正确插入\n"
        "3. 检查授权文件是否存在\n"
        "4. 如需重新授权，请联系管理员"
    ).arg(reason).arg(m_failureCount).arg(terminateTime);
    
    QMessageBox msgBox;
    msgBox.setWindowTitle("授权验证失败");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("程序授权检查失败！");
    msgBox.setDetailedText(detailMessage);
    msgBox.setInformativeText(QString("原因: %1\n\n程序将在关闭此对话框后退出。").arg(reason));
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
    
    // 退出程序
    QCoreApplication::exit(-1);
}
