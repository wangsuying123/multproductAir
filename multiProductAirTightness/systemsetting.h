#ifndef SYSTEMSETTING_H
#define SYSTEMSETTING_H

#include <QWidget>
#include <QSettings>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace Ui {
class SystemSetting;
}

class SystemSetting : public QWidget
{
    Q_OBJECT

public:
    explicit SystemSetting(QWidget *parent = nullptr);
    ~SystemSetting();

    bool isUseExternalMachine() const;

signals:
    void canceled();

private slots:
    void on_cancelButton_clicked();
    void on_saveButton_clicked();
    void on_selectDataPathButton_clicked();
    void on_selectBackupPathButton_clicked();
    void on_manualBackupButton_clicked();
    void on_selectLogPathButton_clicked();
    void on_selectLogBackupPathButton_clicked();
    void on_manualLogBackupButton_clicked();
    void on_enableAutoStartCheckBox_stateChanged(int arg1);

private:
    Ui::SystemSetting *ui;
    
    // 配置相关
    QSettings *settings;
    
    // 自动备份定时器
    QTimer *autoBackupTimer;
    QTimer *autoLogBackupTimer;
    
    // 备份相关方法
    bool performBackup(const QString &sourceDir, const QString &destDir);
    bool performLogBackup(const QString &sourceDir, const QString &destDir);
    void scheduleNextBackup();
    void scheduleNextLogBackup();
    void cleanOldBackups(const QString &backupDir, int retentionDays);
    void cleanOldLogs(const QString &logDir, int retentionDays);
    
    // 开机自启动相关方法
    bool setAutoStart(bool enable);
    bool isAutoStartEnabled();
    
    // 配置文件相关方法
    void loadSettings();
    void saveSettings();
};

#endif // SYSTEMSETTING_H