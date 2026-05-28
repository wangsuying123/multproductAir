#include "machinelockauthorizer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QApplication>
#include <QClipboard>
#include <QScreen>
#include <QScrollArea>
#include <QCoreApplication>
#include <QStandardPaths>

MachineLockAuthorizer::MachineLockAuthorizer(QWidget *parent)
    : QMainWindow(parent),
      m_lockManager(nullptr)
{
    m_lockManager = new MachineLockManager(this);
    setupUI();
    centerWindow();
}

MachineLockAuthorizer::~MachineLockAuthorizer()
{
}

void MachineLockAuthorizer::centerWindow()
{
    // 获取主屏幕
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) {
        return;
    }
    
    // 获取屏幕的可用几何区域（排除任务栏等）
    QRect screenGeometry = screen->availableGeometry();
    
    // 计算窗口应该显示的位置（屏幕中心）
    int x = (screenGeometry.width() - width()) / 2 + screenGeometry.x();
    int y = (screenGeometry.height() - height()) / 2 + screenGeometry.y();
    
    // 移动窗口到中心位置
    move(x, y);
}

void MachineLockAuthorizer::setupUI()
{
    setWindowTitle("机器锁定授权工具 - 硬件绑定版");
    
    // 确保窗口有标准的窗口装饰（最大化、最小化、关闭按钮）
    setWindowFlags(Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose, false);
    
    setMinimumSize(800, 600);
    resize(850, 700);
    
    // 创建滚动区域
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setCentralWidget(scrollArea);
    
    // 创建内容容器
    QWidget *contentWidget = new QWidget();
    scrollArea->setWidget(contentWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(25, 25, 25, 25);
    
    // ========== 标题 ==========
    QLabel *titleLabel = new QLabel("机器锁定授权工具（硬件绑定版）", this);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #333; padding: 10px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    QLabel *subtitleLabel = new QLabel("程序将绑定到特定计算机，复制到其他电脑无法使用", this);
    subtitleLabel->setStyleSheet("font-size: 13px; color: #666; padding-bottom: 10px;");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subtitleLabel);
    
    // ========== 步骤1：获取机器指纹 ==========
    QGroupBox *step1Group = new QGroupBox("步骤1：获取目标计算机的机器指纹", this);
    step1Group->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; padding-top: 15px; }");
    QVBoxLayout *step1Layout = new QVBoxLayout(step1Group);
    step1Layout->setSpacing(12);
    
    QLabel *step1Hint = new QLabel(
        "在需要授权的计算机上运行此工具，点击下方按钮获取该机器的硬件指纹。\n"
        "机器指纹基于CPU、主板、硬盘等硬件特征生成，每台电脑都是唯一的。",
        this
    );
    step1Hint->setStyleSheet("color: #666; padding: 8px; font-size: 12px;");
    step1Hint->setWordWrap(true);
    step1Layout->addWidget(step1Hint);
    
    m_getFingerprintButton = new QPushButton("获取当前机器指纹", this);
    m_getFingerprintButton->setFixedHeight(45);
    m_getFingerprintButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #2196F3;"
        "  color: white;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border: none;"
        "  border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #1976D2;"
        "}"
    );
    step1Layout->addWidget(m_getFingerprintButton);
    
    m_fingerprintTextEdit = new QTextEdit(this);
    m_fingerprintTextEdit->setReadOnly(true);
    m_fingerprintTextEdit->setMinimumHeight(110);
    m_fingerprintTextEdit->setMaximumHeight(110);
    m_fingerprintTextEdit->setStyleSheet("font-size: 12px; padding: 8px;");
    m_fingerprintTextEdit->setPlaceholderText("机器指纹将显示在这里...");
    step1Layout->addWidget(m_fingerprintTextEdit);
    
    mainLayout->addWidget(step1Group);
    
    // ========== 步骤2：生成授权文件 ==========
    QGroupBox *step2Group = new QGroupBox("步骤2：生成授权文件", this);
    step2Group->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; padding-top: 15px; }");
    QVBoxLayout *step2Layout = new QVBoxLayout(step2Group);
    step2Layout->setSpacing(12);
    
    QLabel *step2Hint = new QLabel(
        "获取机器指纹后，生成授权文件并将其复制到目标计算机的程序目录。",
        this
    );
    step2Hint->setStyleSheet("color: #666; padding: 8px; font-size: 12px;");
    step2Hint->setWordWrap(true);
    step2Layout->addWidget(step2Hint);
    
    // 输出路径
    QHBoxLayout *pathLayout = new QHBoxLayout();
    QLabel *pathLabel = new QLabel("授权文件保存位置:", this);
    pathLabel->setStyleSheet("font-size: 12px;");
    pathLayout->addWidget(pathLabel);
    m_outputPathEdit = new QLineEdit(this);
    m_outputPathEdit->setPlaceholderText("选择保存位置...");
    m_outputPathEdit->setMinimumHeight(32);
    m_outputPathEdit->setStyleSheet("font-size: 12px; padding: 5px;");
    pathLayout->addWidget(m_outputPathEdit);
    m_browseButton = new QPushButton("浏览...", this);
    m_browseButton->setFixedSize(90, 32);
    m_browseButton->setStyleSheet("font-size: 12px;");
    pathLayout->addWidget(m_browseButton);
    step2Layout->addLayout(pathLayout);
    
    // 时间设置
    m_timeGroupBox = new QGroupBox("授权时间设置", this);
    m_timeGroupBox->setStyleSheet("QGroupBox { font-size: 12px; padding-top: 12px; }");
    QVBoxLayout *timeLayout = new QVBoxLayout(m_timeGroupBox);
    timeLayout->setSpacing(10);
    
    m_permanentCheckBox = new QCheckBox("永久授权", this);
    m_permanentCheckBox->setChecked(true);
    m_permanentCheckBox->setStyleSheet("font-size: 12px;");
    timeLayout->addWidget(m_permanentCheckBox);
    
    QGridLayout *dateLayout = new QGridLayout();
    dateLayout->setVerticalSpacing(10);
    dateLayout->setHorizontalSpacing(10);
    QLabel *startLabel = new QLabel("开始时间:", this);
    startLabel->setStyleSheet("font-size: 12px;");
    dateLayout->addWidget(startLabel, 0, 0);
    m_startDateTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_startDateTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_startDateTimeEdit->setMinimumHeight(32);
    m_startDateTimeEdit->setStyleSheet("font-size: 12px;");
    m_startDateTimeEdit->setEnabled(false);
    dateLayout->addWidget(m_startDateTimeEdit, 0, 1);
    
    QLabel *endLabel = new QLabel("结束时间:", this);
    endLabel->setStyleSheet("font-size: 12px;");
    dateLayout->addWidget(endLabel, 1, 0);
    m_endDateTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addYears(1), this);
    m_endDateTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_endDateTimeEdit->setMinimumHeight(32);
    m_endDateTimeEdit->setStyleSheet("font-size: 12px;");
    m_endDateTimeEdit->setEnabled(false);
    dateLayout->addWidget(m_endDateTimeEdit, 1, 1);
    
    timeLayout->addLayout(dateLayout);
    step2Layout->addWidget(m_timeGroupBox);
    
    m_generateLicenseButton = new QPushButton("生成授权文件", this);
    m_generateLicenseButton->setFixedHeight(45);
    m_generateLicenseButton->setStyleSheet(
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
    );
    step2Layout->addWidget(m_generateLicenseButton);
    
    mainLayout->addWidget(step2Group);
    
    // ========== 步骤3：验证授权 ==========
    QGroupBox *step3Group = new QGroupBox("步骤3：验证授权（可选）", this);
    step3Group->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; padding-top: 15px; }");
    QVBoxLayout *step3Layout = new QVBoxLayout(step3Group);
    step3Layout->setSpacing(12);
    
    QLabel *step3Hint = new QLabel(
        "在目标计算机上运行此工具，验证授权文件是否有效。",
        this
    );
    step3Hint->setStyleSheet("color: #666; padding: 8px; font-size: 12px;");
    step3Hint->setWordWrap(true);
    step3Layout->addWidget(step3Hint);
    
    m_verifyLicenseButton = new QPushButton("验证当前机器授权", this);
    m_verifyLicenseButton->setFixedHeight(45);
    m_verifyLicenseButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #FF9800;"
        "  color: white;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border: none;"
        "  border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #F57C00;"
        "}"
    );
    step3Layout->addWidget(m_verifyLicenseButton);
    
    mainLayout->addWidget(step3Group);
    
    // ========== 日志区域 ==========
    QGroupBox *logGroup = new QGroupBox("操作日志", this);
    logGroup->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; padding-top: 15px; }");
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setMinimumHeight(150);
    m_logTextEdit->setStyleSheet("font-size: 12px; padding: 5px;");
    logLayout->addWidget(m_logTextEdit);
    mainLayout->addWidget(logGroup);
    
    // ========== 信号连接 ==========
    connect(m_getFingerprintButton, &QPushButton::clicked, this, &MachineLockAuthorizer::onGetFingerprintClicked);
    connect(m_generateLicenseButton, &QPushButton::clicked, this, &MachineLockAuthorizer::onGenerateLicenseClicked);
    connect(m_verifyLicenseButton, &QPushButton::clicked, this, &MachineLockAuthorizer::onVerifyLicenseClicked);
    connect(m_browseButton, &QPushButton::clicked, [this]() {
        // 默认保存到桌面，文件名为 .machine_license
        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString defaultPath = desktopPath + "/.machine_license";
        
        QString filePath = QFileDialog::getSaveFileName(
            this, 
            "选择保存位置", 
            defaultPath,
            "授权文件 (.machine_license);;所有文件 (*.*)"
        );
        if (!filePath.isEmpty()) {
            m_outputPathEdit->setText(filePath);
        }
    });
    connect(m_permanentCheckBox, &QCheckBox::toggled, [this](bool checked) {
        m_startDateTimeEdit->setEnabled(!checked);
        m_endDateTimeEdit->setEnabled(!checked);
    });
    
    appendLog("授权工具已启动", "INFO");
    appendLog("提示：此工具用于生成机器绑定的授权文件", "INFO");
}

void MachineLockAuthorizer::onGetFingerprintClicked()
{
    appendLog("正在获取当前机器指纹...", "INFO");
    
    QString fingerprint = m_lockManager->getCurrentMachineFingerprint();
    
    if (fingerprint.isEmpty()) {
        appendLog("获取机器指纹失败！", "ERROR");
        QMessageBox::critical(this, "错误", "无法获取机器指纹！");
        return;
    }
    
    // 显示指纹
    m_fingerprintTextEdit->clear();
    m_fingerprintTextEdit->append("机器指纹：");
    m_fingerprintTextEdit->append(fingerprint);
    m_fingerprintTextEdit->append("");
    m_fingerprintTextEdit->append("（已自动复制到剪贴板）");
    
    // 复制到剪贴板
    QApplication::clipboard()->setText(fingerprint);
    
    appendLog("========== 机器指纹获取成功 ==========", "SUCCESS");
    appendLog("指纹: " + fingerprint, "SUCCESS");
    appendLog("已复制到剪贴板", "SUCCESS");
    appendLog("======================================", "SUCCESS");
}

void MachineLockAuthorizer::onGenerateLicenseClicked()
{
    QString outputPath = m_outputPathEdit->text().trimmed();
    
    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择授权文件保存位置！");
        return;
    }
    
    // 确认对话框
    QString confirmMsg = "确定要生成授权文件吗？\n\n";
    confirmMsg += "保存位置: " + outputPath + "\n";
    confirmMsg += "授权类型: " + QString(m_permanentCheckBox->isChecked() ? "永久授权" : "限时授权");
    
    if (QMessageBox::question(this, "确认", confirmMsg, 
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    
    appendLog("开始生成授权文件...", "INFO");
    
    QDateTime startTime, endTime;
    if (!m_permanentCheckBox->isChecked()) {
        startTime = m_startDateTimeEdit->dateTime();
        endTime = m_endDateTimeEdit->dateTime();
        
        if (endTime <= startTime) {
            QMessageBox::warning(this, "警告", "结束时间必须晚于开始时间！");
            return;
        }
    }
    
    bool success = m_lockManager->generateLicenseFile(outputPath, startTime, endTime);
    
    if (success) {
        appendLog("========== 授权文件生成成功 ==========", "SUCCESS");
        appendLog("文件位置: " + outputPath, "SUCCESS");
        appendLog("授权类型: " + QString(m_permanentCheckBox->isChecked() ? "永久" : "限时"), "SUCCESS");
        appendLog("======================================", "SUCCESS");
        
        QString infoMsg = "授权文件生成成功！\n\n";
        infoMsg += "文件位置: " + outputPath + "\n\n";
        infoMsg += "【重要提示】\n";
        infoMsg += "1. 将此文件复制到目标计算机的程序目录\n";
        infoMsg += "2. 必须重命名为: .machine_license\n";
        infoMsg += "   （注意：没有 .lic 后缀）\n";
        infoMsg += "3. 程序启动时会自动验证此文件";
        
        QMessageBox::information(this, "成功", infoMsg);
    } else {
        appendLog("授权文件生成失败！", "ERROR");
        QMessageBox::critical(this, "失败", "授权文件生成失败！");
    }
}

void MachineLockAuthorizer::onVerifyLicenseClicked()
{
    // 先让用户选择授权文件
    QString licenseFilePath = QFileDialog::getOpenFileName(
        this, 
        "选择授权文件", 
        QCoreApplication::applicationDirPath(),
        "授权文件 (*.lic *.machine_license);;所有文件 (*.*)"
    );
    
    if (licenseFilePath.isEmpty()) {
        appendLog("用户取消了文件选择", "WARNING");
        return;
    }
    
    appendLog("正在验证授权文件: " + licenseFilePath, "INFO");
    
    // 临时加载并验证指定的授权文件
    if (!m_lockManager->loadLicenseFile(licenseFilePath)) {
        appendLog("========== 授权文件读取失败 ==========", "ERROR");
        appendLog("可能原因：", "ERROR");
        appendLog("1. 文件格式不正确", "ERROR");
        appendLog("2. 文件已损坏", "ERROR");
        appendLog("3. 文件不是有效的授权文件", "ERROR");
        appendLog("===================================", "ERROR");
        
        QMessageBox::critical(this, "验证失败", 
            "授权文件读取失败！\n\n"
            "可能原因：\n"
            "1. 文件格式不正确\n"
            "2. 文件已损坏\n"
            "3. 文件不是有效的授权文件");
        return;
    }
    
    // 验证机器指纹
    QString currentFingerprint = m_lockManager->getCurrentMachineFingerprint();
    QString authorizedFingerprint = m_lockManager->getAuthorizedFingerprint();
    
    if (currentFingerprint != authorizedFingerprint) {
        appendLog("========== 授权验证失败 ==========", "ERROR");
        appendLog("机器指纹不匹配！", "ERROR");
        appendLog("当前机器: " + currentFingerprint, "ERROR");
        appendLog("授权机器: " + authorizedFingerprint, "ERROR");
        appendLog("===================================", "ERROR");
        
        QMessageBox::critical(this, "验证失败", 
            "机器指纹不匹配！\n\n"
            "此授权文件不适用于当前计算机。\n"
            "程序可能被复制到其他电脑。");
        return;
    }
    
    // 验证时间
    if (m_lockManager->isLicenseExpired()) {
        appendLog("========== 授权验证失败 ==========", "ERROR");
        appendLog("授权已过期！", "ERROR");
        appendLog("===================================", "ERROR");
        
        QMessageBox::critical(this, "验证失败", "授权已过期！");
        return;
    }
    
    // 验证成功
    QString info = m_lockManager->getLicenseInfo();
    appendLog("========== 授权验证通过 ==========", "SUCCESS");
    appendLog(info, "SUCCESS");
    appendLog("===================================", "SUCCESS");
    
    QMessageBox::information(this, "验证成功", "授权验证通过！\n\n" + info);
}

void MachineLockAuthorizer::appendLog(const QString &message, const QString &type)
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
