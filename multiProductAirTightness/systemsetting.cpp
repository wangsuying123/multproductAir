#include "systemsetting.h"
#include "ui_systemsetting.h"
#include "logmanager.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QCoreApplication>

SystemSetting::SystemSetting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SystemSetting)
{
    ui->setupUi(this);
    
    // 移除固定几何尺寸，确保页面能正确适应窗口大小变化
    this->setGeometry(QRect());
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 设置布局管理器的属性，确保它能正确适应窗口大小变化
    if (this->layout()) {
        this->layout()->setSizeConstraint(QLayout::SetNoConstraint);
        this->layout()->setContentsMargins(5, 5, 5, 5);
        this->layout()->setSpacing(5);
    }
    
    // 初始化配置（使用程序所在目录，与数据库路径策略一致）
    settings = new QSettings(QCoreApplication::applicationDirPath() + "/system.ini", QSettings::IniFormat);
    
    // 初始化定时器
    autoBackupTimer = new QTimer(this);
    autoLogBackupTimer = new QTimer(this);
    
    // 连接信号槽
    connect(autoBackupTimer, &QTimer::timeout, this, [=]() {
        if (ui->enableAutoBackupCheckBox->isChecked()) {
            performBackup(ui->dataStoragePathEdit->text(), ui->backupLocationEdit->text());
            scheduleNextBackup();
        }
    });
    
    connect(autoLogBackupTimer, &QTimer::timeout, this, [=]() {
        if (ui->enableLogBackupCheckBox->isChecked()) {
            performLogBackup(ui->logPathEdit->text(), ui->logBackupPathEdit->text());
            scheduleNextLogBackup();
        }
    });
    
    // 加载设置
    loadSettings();
}

SystemSetting::~SystemSetting()
{
    delete autoBackupTimer;
    delete autoLogBackupTimer;
    delete settings;
    delete ui;
}

// 获取是否使用外部机台的设置
bool SystemSetting::isUseExternalMachine() const
{
    QSettings s(QCoreApplication::applicationDirPath() + "/system.ini", QSettings::IniFormat);
    s.beginGroup("AutoStart");
    bool useExternal = s.value("UseExternalMachine", false).toBool();
    s.endGroup();
    return useExternal;
}

void SystemSetting::on_cancelButton_clicked()
{
    loadSettings();
    emit canceled();
}

void SystemSetting::on_saveButton_clicked()
{
    // 保存设置到配置文件
    saveSettings();
    LOG_INFO("系统设置已保存", "系统设置");
    QMessageBox::information(this, "保存成功", "系统设置已保存");
    
    // 通知日志管理器更新日志路径
    LogManager* logManager = LogManager::getInstance();
    logManager->updateLogPath();
}

void SystemSetting::on_selectDataPathButton_clicked()
{
    // 选择数据存储路径
    QString dir = QFileDialog::getExistingDirectory(this, "选择数据存储路径", ui->dataStoragePathEdit->text());
    if (!dir.isEmpty()) {
        LOG_INFO(QString("数据存储路径变更: %1 -> %2").arg(ui->dataStoragePathEdit->text()).arg(dir), "系统设置");
        ui->dataStoragePathEdit->setText(dir);
    }
}

void SystemSetting::on_selectBackupPathButton_clicked()
{
    // 选择备份存储位置
    QString dir = QFileDialog::getExistingDirectory(this, "选择备份存储位置", ui->backupLocationEdit->text());
    if (!dir.isEmpty()) {
        LOG_INFO(QString("备份存储路径变更: %1 -> %2").arg(ui->backupLocationEdit->text()).arg(dir), "系统设置");
        ui->backupLocationEdit->setText(dir);
    }
}

void SystemSetting::on_manualBackupButton_clicked()
{
    // 执行手动备份
    LOG_INFO("开始执行手动数据备份", "系统设置");
    if (performBackup(ui->dataStoragePathEdit->text(), ui->backupLocationEdit->text())) {
        // 更新最后备份时间
        ui->lastBackupLabel->setText("最后备份时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        LOG_INFO("手动数据备份成功", "系统设置");
        QMessageBox::information(this, "备份成功", "数据已成功备份");
    } else {
        LOG_ERROR("手动数据备份失败", "系统设置");
        QMessageBox::critical(this, "备份失败", "数据备份失败，请检查路径设置");
    }
}

void SystemSetting::on_selectLogPathButton_clicked()
{
    // 选择日志存储路径
    QString dir = QFileDialog::getExistingDirectory(this, "选择日志存储路径", ui->logPathEdit->text());
    if (!dir.isEmpty()) {
        LOG_INFO(QString("日志存储路径变更: %1 -> %2").arg(ui->logPathEdit->text()).arg(dir), "系统设置");
        ui->logPathEdit->setText(dir);
    }
}

void SystemSetting::on_selectLogBackupPathButton_clicked()
{
    // 选择日志备份路径
    QString dir = QFileDialog::getExistingDirectory(this, "选择日志备份路径", ui->logBackupPathEdit->text());
    if (!dir.isEmpty()) {
        LOG_INFO(QString("日志备份路径变更: %1 -> %2").arg(ui->logBackupPathEdit->text()).arg(dir), "系统设置");
        ui->logBackupPathEdit->setText(dir);
    }
}

void SystemSetting::on_manualLogBackupButton_clicked()
{
    // 执行手动日志备份
    LOG_INFO("开始执行手动日志备份", "系统设置");
    if (performLogBackup(ui->logPathEdit->text(), ui->logBackupPathEdit->text())) {
        // 更新最后日志备份时间
        ui->lastLogBackupLabel->setText("最后日志备份时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        LOG_INFO("手动日志备份成功", "系统设置");
        QMessageBox::information(this, "备份成功", "日志已成功备份");
    } else {
        LOG_ERROR("手动日志备份失败", "系统设置");
        QMessageBox::critical(this, "备份失败", "日志备份失败，请检查路径设置");
    }
}

void SystemSetting::on_enableAutoStartCheckBox_stateChanged(int arg1)
{
    // 设置开机自启动
    bool enable = (arg1 == Qt::Checked);
    LOG_INFO(QString("开机自启动设置: %1").arg(enable ? "启用" : "禁用"), "系统设置");
    if (!setAutoStart(enable)) {
        LOG_ERROR("开机自启动设置失败", "系统设置");
        QMessageBox::warning(this, "设置失败", "开机自启动设置失败，请检查权限");
        ui->enableAutoStartCheckBox->setChecked(!enable);
    }
}

bool SystemSetting::performBackup(const QString &sourceDir, const QString &destDir)
{
    if (sourceDir.isEmpty() || destDir.isEmpty()) {
        return false;
    }
    
    // 创建源目录和目标目录
    QDir source(sourceDir);
    QDir dest(destDir);
    
    if (!source.exists()) {
        source.mkpath(".");
    }
    
    if (!dest.exists()) {
        dest.mkpath(".");
    }
    
    // 创建带时间戳的备份目录
    QString backupDirName = "backup_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString backupDir = destDir + "/" + backupDirName;
    
    if (!QDir().mkpath(backupDir)) {
        return false;
    }
    
    // 复制文件
    QStringList filters;
    filters << "*";
    QFileInfoList fileList = source.entryInfoList(filters, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    
    foreach (QFileInfo fileInfo, fileList) {
        QString srcPath = fileInfo.absoluteFilePath();
        QString destPath = backupDir + "/" + fileInfo.fileName();
        
        if (fileInfo.isDir()) {
            QDir().mkpath(destPath);
            if (!performBackup(srcPath, destPath)) {
                return false;
            }
        } else {
            if (!QFile::copy(srcPath, destPath)) {
                return false;
            }
        }
    }
    
    // 清理旧备份
    cleanOldBackups(destDir, ui->backupRetentionEdit->text().toInt());
    
    return true;
}

bool SystemSetting::performLogBackup(const QString &sourceDir, const QString &destDir)
{
    if (sourceDir.isEmpty() || destDir.isEmpty()) {
        return false;
    }
    
    // 创建源目录和目标目录
    QDir source(sourceDir);
    QDir dest(destDir);
    
    if (!source.exists() || !dest.exists()) {
        return false;
    }
    
    // 创建带时间戳的日志备份目录
    QString backupDirName = "log_backup_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString backupDir = destDir + "/" + backupDirName;
    
    if (!QDir().mkpath(backupDir)) {
        return false;
    }
    
    // 复制日志文件
    QStringList filters;
    filters << "*.log";
    QFileInfoList fileList = source.entryInfoList(filters, QDir::Files);
    
    foreach (QFileInfo fileInfo, fileList) {
        QString srcPath = fileInfo.absoluteFilePath();
        QString destPath = backupDir + "/" + fileInfo.fileName();
        
        if (!QFile::copy(srcPath, destPath)) {
            return false;
        }
    }
    
    // 清理旧日志
    cleanOldLogs(sourceDir, ui->logRetentionEdit->text().toInt());
    
    return true;
}

void SystemSetting::scheduleNextBackup()
{
    // 根据选择的周期计算下次备份时间
    QDateTime now = QDateTime::currentDateTime();
    QDateTime nextBackup;
    
    QString period = ui->backupPeriodComboBox->currentText();
    if (period == "每天") {
        nextBackup = now.addDays(1);
        nextBackup.setTime(ui->backupTimeEdit->time());
    } else if (period == "每周") {
        nextBackup = now.addDays(7);
        nextBackup.setTime(ui->backupTimeEdit->time());
    } else if (period == "每月") {
        nextBackup = now.addMonths(1);
        nextBackup.setTime(ui->backupTimeEdit->time());
    }
    
    // 设置定时器
    autoBackupTimer->start(now.msecsTo(nextBackup));
}

void SystemSetting::scheduleNextLogBackup()
{
    // 根据选择的周期计算下次日志备份时间
    QDateTime now = QDateTime::currentDateTime();
    QDateTime nextBackup;
    
    QString period = ui->logBackupPeriodComboBox->currentText();
    if (period == "每天") {
        nextBackup = now.addDays(1);
    } else if (period == "每周") {
        nextBackup = now.addDays(7);
    } else if (period == "每月") {
        nextBackup = now.addMonths(1);
    }
    
    // 设置定时器
    autoLogBackupTimer->start(now.msecsTo(nextBackup));
}

void SystemSetting::cleanOldBackups(const QString &backupDir, int retentionDays)
{
    QDir dir(backupDir);
    if (!dir.exists()) {
        return;
    }
    
    // 获取所有备份目录
    QStringList filters;
    filters << "backup_*";
    QFileInfoList backupList = dir.entryInfoList(filters, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    
    // 计算保留时间阈值
    QDateTime threshold = QDateTime::currentDateTime().addDays(-retentionDays);
    
    // 删除旧备份
    foreach (QFileInfo backupInfo, backupList) {
        QString dirName = backupInfo.fileName();
        QString dateStr = dirName.mid(7, 8); // 获取日期部分 yyyyMMdd
        QDateTime backupDate = QDateTime::fromString(dateStr, "yyyyMMdd");
        
        if (backupDate < threshold) {
            QDir(backupInfo.absoluteFilePath()).removeRecursively();
        }
    }
}

void SystemSetting::cleanOldLogs(const QString &logDir, int retentionDays)
{
    QDir dir(logDir);
    if (!dir.exists()) {
        return;
    }
    
    // 获取所有日志文件
    QStringList filters;
    filters << "*.log";
    QFileInfoList logList = dir.entryInfoList(filters, QDir::Files, QDir::Time);
    
    // 计算保留时间阈值
    QDateTime threshold = QDateTime::currentDateTime().addDays(-retentionDays);
    
    // 删除旧日志
    foreach (QFileInfo logInfo, logList) {
        if (logInfo.birthTime() < threshold) {
            QFile::remove(logInfo.absoluteFilePath());
        }
    }
}

bool SystemSetting::setAutoStart(bool enable)
{
    // Windows系统的开机自启动设置
    QString appPath = QCoreApplication::applicationFilePath();
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    
    if (enable) {
        // 设置自启动
        reg.setValue("AirTightnessSystem", QVariant(appPath));
    } else {
        // 移除自启动
        reg.remove("AirTightnessSystem");
    }
    
    return true;
}

bool SystemSetting::isAutoStartEnabled()
{
    // 检查开机自启动是否已启用
    QString appPath = QCoreApplication::applicationFilePath();
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    QString value = reg.value("AirTightnessSystem").toString();
    
    return value == appPath;
}

void SystemSetting::loadSettings()
{
    // 加载数据存储与备份设置
    settings->beginGroup("DataBackup");
    ui->dataStoragePathEdit->setText(settings->value("DataPath", "D:/AirTightnessData/Storage").toString());
    ui->backupLocationEdit->setText(settings->value("BackupPath", "D:/AirTightnessData/Backup").toString());
    ui->backupPeriodComboBox->setCurrentText(settings->value("BackupPeriod", "每天").toString());
    ui->backupTimeEdit->setTime(QTime::fromString(settings->value("BackupTime", "00:00:00").toString()));
    ui->backupRetentionEdit->setText(settings->value("BackupRetention", "90").toString());
    ui->enableAutoBackupCheckBox->setChecked(settings->value("EnableAutoBackup", true).toBool());
    ui->lastBackupLabel->setText("最后备份时间: " + settings->value("LastBackupTime", "2025-05-20 14:30:25").toString());
    settings->endGroup();
    
    // 加载日志备份设置
    settings->beginGroup("LogBackup");
    ui->logPathEdit->setText(settings->value("LogPath", "D:/AirTightnessData/Logs").toString());
    ui->logBackupPathEdit->setText(settings->value("LogBackupPath", "D:/AirTightnessData/LogBackup").toString());
    ui->logLevelComboBox->setCurrentText(settings->value("LogLevel", "DEBUG").toString());
    ui->logSplitComboBox->setCurrentText(settings->value("LogSplit", "按天分割").toString());
    ui->logRetentionEdit->setText(settings->value("LogRetention", "30").toString());
    ui->logBackupPeriodComboBox->setCurrentText(settings->value("LogBackupPeriod", "每天").toString());
    ui->enableLogBackupCheckBox->setChecked(settings->value("EnableLogBackup", true).toBool());
    ui->lastLogBackupLabel->setText("最后日志备份时间: " + settings->value("LastLogBackupTime", "2025-05-20 10:15:30").toString());
    settings->endGroup();
    
    // 加载开机自启动设置
    ui->enableAutoStartCheckBox->setChecked(isAutoStartEnabled());
    settings->beginGroup("AutoStart");
    ui->minimizeToTrayCheckBox->setChecked(settings->value("MinimizeToTray", false).toBool());
    ui->useExternalMachineCheckBox->setChecked(settings->value("UseExternalMachine", false).toBool());
    ui->autoConnectCheckBox->setChecked(settings->value("AutoConnect", false).toBool());
    ui->autoStartRecordCheckBox->setChecked(settings->value("AutoStartRecord", false).toBool());
    settings->endGroup();
    
    // 启动定时器
    if (ui->enableAutoBackupCheckBox->isChecked()) {
        scheduleNextBackup();
    }
    
    if (ui->enableLogBackupCheckBox->isChecked()) {
        scheduleNextLogBackup();
    }
}

void SystemSetting::saveSettings()
{
    // 保存数据存储与备份设置
    settings->beginGroup("DataBackup");
    settings->setValue("DataPath", ui->dataStoragePathEdit->text());
    settings->setValue("BackupPath", ui->backupLocationEdit->text());
    settings->setValue("BackupPeriod", ui->backupPeriodComboBox->currentText());
    settings->setValue("BackupTime", ui->backupTimeEdit->time().toString());
    settings->setValue("BackupRetention", ui->backupRetentionEdit->text());
    settings->setValue("EnableAutoBackup", ui->enableAutoBackupCheckBox->isChecked());
    settings->endGroup();
    
    // 保存日志备份设置
    settings->beginGroup("LogBackup");
    settings->setValue("LogPath", ui->logPathEdit->text());
    settings->setValue("LogBackupPath", ui->logBackupPathEdit->text());
    settings->setValue("LogLevel", ui->logLevelComboBox->currentText());
    settings->setValue("LogSplit", ui->logSplitComboBox->currentText());
    settings->setValue("LogRetention", ui->logRetentionEdit->text());
    settings->setValue("LogBackupPeriod", ui->logBackupPeriodComboBox->currentText());
    settings->setValue("EnableLogBackup", ui->enableLogBackupCheckBox->isChecked());
    settings->endGroup();
    
    // 保存开机自启动设置
    settings->beginGroup("AutoStart");
    settings->setValue("MinimizeToTray", ui->minimizeToTrayCheckBox->isChecked());
    settings->setValue("UseExternalMachine", ui->useExternalMachineCheckBox->isChecked());
    settings->setValue("AutoConnect", ui->autoConnectCheckBox->isChecked());
    settings->setValue("AutoStartRecord", ui->autoStartRecordCheckBox->isChecked());
    settings->endGroup();
    
    // 重启定时器
    autoBackupTimer->stop();
    autoLogBackupTimer->stop();
    
    if (ui->enableAutoBackupCheckBox->isChecked()) {
        scheduleNextBackup();
    }
    
    if (ui->enableLogBackupCheckBox->isChecked()) {
        scheduleNextLogBackup();
    }
    
    // 确保数据写入磁盘
    settings->sync();
}