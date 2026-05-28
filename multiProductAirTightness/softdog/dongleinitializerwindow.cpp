#include "dongleinitializerwindow.h"
#include "donglemanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>

DongleInitializerWindow::DongleInitializerWindow(QWidget *parent)
    : QMainWindow(parent),
      m_dongleManager(nullptr)
{
    m_dongleManager = new DongleManager(this);
    setupUI();
    refreshStorageList();
}

DongleInitializerWindow::~DongleInitializerWindow()
{
}

void DongleInitializerWindow::setupUI()
{
    setWindowTitle("加密狗授权工具");
    setMinimumSize(600, 500);
    
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // ========== 标题 ==========
    QLabel *titleLabel = new QLabel("U盘加密狗授权工具", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #333;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // ========== U盘选择区域 ==========
    QGroupBox *storageGroup = new QGroupBox("选择U盘", this);
    QVBoxLayout *storageLayout = new QVBoxLayout(storageGroup);
    
    QHBoxLayout *selectLayout = new QHBoxLayout();
    m_storageComboBox = new QComboBox(this);
    m_storageComboBox->setMinimumWidth(350);
    m_refreshButton = new QPushButton("刷新", this);
    m_refreshButton->setFixedWidth(80);
    selectLayout->addWidget(m_storageComboBox);
    selectLayout->addWidget(m_refreshButton);
    storageLayout->addLayout(selectLayout);
    
    m_storageInfoLabel = new QLabel("请选择要授权的U盘", this);
    m_storageInfoLabel->setStyleSheet("color: #666; padding: 10px; background: #f5f5f5; border-radius: 5px;");
    m_storageInfoLabel->setWordWrap(true);
    storageLayout->addWidget(m_storageInfoLabel);
    
    mainLayout->addWidget(storageGroup);

    // ========== 授权时间设置 ==========
    m_timeGroupBox = new QGroupBox("授权时间设置", this);
    QVBoxLayout *timeLayout = new QVBoxLayout(m_timeGroupBox);
    
    m_permanentCheckBox = new QCheckBox("永久授权", this);
    m_permanentCheckBox->setChecked(true);
    timeLayout->addWidget(m_permanentCheckBox);
    
    QGridLayout *dateLayout = new QGridLayout();
    dateLayout->addWidget(new QLabel("开始时间:", this), 0, 0);
    m_startDateTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_startDateTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_startDateTimeEdit->setEnabled(false);
    dateLayout->addWidget(m_startDateTimeEdit, 0, 1);
    
    dateLayout->addWidget(new QLabel("结束时间:", this), 1, 0);
    m_endDateTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addYears(1), this);
    m_endDateTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_endDateTimeEdit->setEnabled(false);
    dateLayout->addWidget(m_endDateTimeEdit, 1, 1);
    
    timeLayout->addLayout(dateLayout);
    mainLayout->addWidget(m_timeGroupBox);
    
    // ========== 授权按钮 ==========
    m_authorizeButton = new QPushButton("授权U盘", this);
    m_authorizeButton->setFixedHeight(40);
    m_authorizeButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #4CAF50;"
        "  color: white;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border: none;"
        "  border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #45a049;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #3d8b40;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #cccccc;"
        "}"
    );
    mainLayout->addWidget(m_authorizeButton);
    
    // ========== 日志区域 ==========
    QGroupBox *logGroup = new QGroupBox("操作日志", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setMinimumHeight(150);
    logLayout->addWidget(m_logTextEdit);
    mainLayout->addWidget(logGroup);
    
    // ========== 信号连接 ==========
    connect(m_refreshButton, &QPushButton::clicked, this, &DongleInitializerWindow::onRefreshClicked);
    connect(m_authorizeButton, &QPushButton::clicked, this, &DongleInitializerWindow::onAuthorizeClicked);
    connect(m_storageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &DongleInitializerWindow::onStorageSelected);
    connect(m_permanentCheckBox, &QCheckBox::toggled, [this](bool checked) {
        m_startDateTimeEdit->setEnabled(!checked);
        m_endDateTimeEdit->setEnabled(!checked);
    });
    
    appendLog("授权工具已启动", "INFO");
}

void DongleInitializerWindow::refreshStorageList()
{
    m_storageComboBox->clear();
    m_storageList.clear();
    
    appendLog("正在扫描可用U盘...", "INFO");
    
    m_storageList = m_dongleManager->getAvailableUSBStorages();
    
    if (m_storageList.isEmpty()) {
        m_storageComboBox->addItem("未检测到可用U盘");
        m_authorizeButton->setEnabled(false);
        m_storageInfoLabel->setText("未检测到可用的U盘设备，请插入U盘后点击刷新");
        appendLog("未检测到可用U盘", "WARNING");
    } else {
        for (const QStorageInfo &storage : m_storageList) {
            qint64 totalBytes = storage.bytesTotal();
            double sizeGB = static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0);
            QString displayText = QString("%1 (%2) - %3 GB")
                .arg(storage.rootPath())
                .arg(storage.name().isEmpty() ? "无卷标" : storage.name())
                .arg(sizeGB, 0, 'f', 2);
            m_storageComboBox->addItem(displayText);
        }
        m_authorizeButton->setEnabled(true);
        appendLog(QString("检测到 %1 个可用存储设备").arg(m_storageList.size()), "INFO");
        
        if (m_storageComboBox->count() > 0) {
            onStorageSelected(0);
        }
    }
}

void DongleInitializerWindow::onRefreshClicked()
{
    refreshStorageList();
}

void DongleInitializerWindow::onStorageSelected(int index)
{
    if (index < 0 || index >= m_storageList.size()) {
        return;
    }
    
    const QStorageInfo &storage = m_storageList.at(index);
    qint64 totalBytes = storage.bytesTotal();
    qint64 freeBytes = storage.bytesFree();
    double totalGB = static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0);
    double freeGB = static_cast<double>(freeBytes) / (1024.0 * 1024.0 * 1024.0);
    
    QString dongleId = m_dongleManager->generateUniqueID(storage);
    
    QString info = QString(
        "设备路径: %1\n"
        "卷标: %2\n"
        "文件系统: %3\n"
        "总容量: %4 GB\n"
        "可用空间: %5 GB\n"
        "加密狗ID: %6"
    ).arg(storage.rootPath())
     .arg(storage.name().isEmpty() ? "无" : storage.name())
     .arg(QString(storage.fileSystemType()))
     .arg(totalGB, 0, 'f', 2)
     .arg(freeGB, 0, 'f', 2)
     .arg(dongleId.left(32) + "...");
    
    m_storageInfoLabel->setText(info);
}

void DongleInitializerWindow::onAuthorizeClicked()
{
    int index = m_storageComboBox->currentIndex();
    if (index < 0 || index >= m_storageList.size()) {
        QMessageBox::warning(this, "警告", "请先选择要授权的U盘！");
        return;
    }
    
    const QStorageInfo &storage = m_storageList.at(index);
    
    // 确认对话框
    QString confirmMsg = QString(
        "确定要授权以下U盘吗？\n\n"
        "设备: %1\n"
        "卷标: %2\n"
        "容量: %3 GB\n\n"
        "授权类型: %4"
    ).arg(storage.rootPath())
     .arg(storage.name().isEmpty() ? "无" : storage.name())
     .arg(static_cast<double>(storage.bytesTotal()) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2)
     .arg(m_permanentCheckBox->isChecked() ? "永久授权" : 
          QString("限时授权 (%1 至 %2)")
              .arg(m_startDateTimeEdit->dateTime().toString("yyyy-MM-dd"))
              .arg(m_endDateTimeEdit->dateTime().toString("yyyy-MM-dd")));
    
    if (QMessageBox::question(this, "确认授权", confirmMsg, 
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    
    appendLog(QString("开始授权U盘: %1").arg(storage.rootPath()), "INFO");
    
    QString programId = m_dongleManager->getProgramId();
    bool success = false;
    
    if (m_permanentCheckBox->isChecked()) {
        // 永久授权
        success = m_dongleManager->initializeDongle(storage, programId);
    } else {
        // 限时授权
        QDateTime startTime = m_startDateTimeEdit->dateTime();
        QDateTime endTime = m_endDateTimeEdit->dateTime();
        
        if (endTime <= startTime) {
            QMessageBox::warning(this, "警告", "结束时间必须晚于开始时间！");
            return;
        }
        
        success = m_dongleManager->initializeDongle(storage, programId, startTime, endTime);
    }
    
    if (success) {
        QString dongleId = m_dongleManager->generateUniqueID(storage);
        appendLog("========== 授权成功 ==========", "SUCCESS");
        appendLog(QString("U盘路径: %1").arg(storage.rootPath()), "SUCCESS");
        appendLog(QString("加密狗ID: %1").arg(dongleId), "SUCCESS");
        appendLog(QString("授权类型: %1").arg(m_permanentCheckBox->isChecked() ? "永久" : "限时"), "SUCCESS");
        appendLog("==============================", "SUCCESS");
        
        QMessageBox::information(this, "授权成功", 
            QString("U盘授权成功！\n\n"
                    "设备: %1\n"
                    "加密狗ID: %2\n\n"
                    "现在可以将程序复制到此U盘运行。")
            .arg(storage.rootPath())
            .arg(dongleId.left(32) + "..."));
    } else {
        appendLog("授权失败！请检查U盘是否可写", "ERROR");
        QMessageBox::critical(this, "授权失败", 
            "U盘授权失败！\n\n可能原因：\n1. U盘写保护\n2. U盘空间不足\n3. 权限不足");
    }
}

void DongleInitializerWindow::appendLog(const QString &message, const QString &type)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString color = "#333";
    
    if (type == "ERROR") {
        color = "#d32f2f";
    } else if (type == "WARNING") {
        color = "#f57c00";
    } else if (type == "SUCCESS") {
        color = "#388e3c";
    } else {
        color = "#1976d2";
    }
    
    QString html = QString("<span style='color: #999;'>[%1]</span> "
                          "<span style='color: %2;'>%3</span>")
                   .arg(timestamp).arg(color).arg(message);
    
    m_logTextEdit->append(html);
}
