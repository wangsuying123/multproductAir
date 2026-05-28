#include "splashscreen.h"
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QFont>
#include <QScreen>
#include <QGuiApplication>
#include <QPixmap>
#include <QApplication>

SplashScreen::SplashScreen(QWidget *parent)
    : QWidget(parent), m_hasError(false)
{
    // 设置窗口属性 - 全屏无边框
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::SplashScreen);
    setAttribute(Qt::WA_TranslucentBackground, false);
    
    // 获取屏幕尺寸并设置全屏
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    setFixedSize(screenGeometry.width(), screenGeometry.height());
    move(0, 0);
    
    setupUI();
    setupErrorUI();
    setupAnimations();
}

SplashScreen::~SplashScreen()
{
}

void SplashScreen::setupUI()
{
    int centerX = width() / 2;
    int centerY = height() / 2;
    
    // Logo图标
    logoLabel = new QLabel(this);
    logoLabel->setGeometry(centerX - 100, centerY - 220, 200, 140);
    logoLabel->setAlignment(Qt::AlignCenter);
    
    // 加载logo图片
    QPixmap logoPixmap(":/assets/logo/logo.png");
    if (!logoPixmap.isNull()) {
        logoLabel->setPixmap(logoPixmap.scaled(180, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logoLabel->setStyleSheet("background: transparent;");
    } else {
        // 如果logo加载失败，显示赛博朋克风格备用文字
        logoLabel->setStyleSheet(
            "QLabel {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #00e676, stop:0.5 #64d2ff, stop:1 #ba68c8);"
            "   border-radius: 70px;"
            "   border: 3px solid #64d2ff;"
            "   font-size: 48px;"
            "   font-weight: bold;"
            "   color: #0a1929;"
            "}"
        );
        logoLabel->setText("智芯");
    }
    
    // 主标题 - 赛博朋克风格
    titleLabel = new QLabel("气密性检测系统", this);
    titleLabel->setGeometry(0, centerY - 50, width(), 70);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 52px;"
        "   font-weight: 800;"
        "   color: #64d2ff;"
        "   font-family: 'Microsoft YaHei UI', 'SimHei', sans-serif;"
        "   background: transparent;"
        "   letter-spacing: 8px;"
        "}"
    );

    // 副标题 - 英文
    subtitleLabel = new QLabel("Airtightness Detection System", this);
    subtitleLabel->setGeometry(0, centerY + 30, width(), 35);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 18px;"
        "   font-weight: 500;"
        "   color: #00e676;"
        "   font-family: 'Consolas', 'Segoe UI', monospace;"
        "   letter-spacing: 4px;"
        "   background: transparent;"
        "}"
    );
    
    // 加载消息
    messageLabel = new QLabel("正在初始化系统...", this);
    messageLabel->setGeometry(0, centerY + 120, width(), 30);
    messageLabel->setAlignment(Qt::AlignCenter);
    messageLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 16px;"
        "   color: #e2e8f0;"
        "   font-family: 'Microsoft YaHei UI', sans-serif;"
        "   background: transparent;"
        "}"
    );
    
    // 进度条 - 赛博朋克风格
    progressBar = new QProgressBar(this);
    progressBar->setGeometry(centerX - 250, centerY + 160, 500, 12);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);
    progressBar->setStyleSheet(
        "QProgressBar {"
        "   border: 2px solid #64d2ff;"
        "   border-radius: 6px;"
        "   background-color: rgba(10, 25, 41, 0.8);"
        "}"
        "QProgressBar::chunk {"
        "   border-radius: 4px;"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00e676, stop:0.5 #64d2ff, stop:1 #ba68c8);"
        "}"
    );
    
    // 版本号
    versionLabel = new QLabel("Version 1.0.0", this);
    versionLabel->setGeometry(0, centerY + 200, width(), 25);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 14px;"
        "   color: #64d2ff;"
        "   font-family: 'Consolas', monospace;"
        "   background: transparent;"
        "}"
    );
    
    // 版权信息 - 公司名称
    copyrightLabel = new QLabel("© 2025 东莞智芯数字科技有限公司 All Rights Reserved", this);
    copyrightLabel->setGeometry(0, height() - 60, width(), 25);
    copyrightLabel->setAlignment(Qt::AlignCenter);
    copyrightLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 14px;"
        "   color: #475569;"
        "   font-family: 'Microsoft YaHei UI', sans-serif;"
        "   background: transparent;"
        "}"
    );
    
    // 设置透明度效果
    opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(1.0);
    setGraphicsEffect(opacityEffect);
}

void SplashScreen::setupAnimations()
{
    fadeAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
    fadeAnimation->setDuration(500);
    fadeAnimation->setStartValue(1.0);
    fadeAnimation->setEndValue(0.0);
    fadeAnimation->setEasingCurve(QEasingCurve::OutQuad);
    
    connect(fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
        emit loadingFinished();
        close();
    });
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制赛博朋克深色背景
    QLinearGradient bgGradient(0, 0, width(), height());
    bgGradient.setColorAt(0, QColor(10, 25, 41));      // #0a1929 赛博暗青蓝
    bgGradient.setColorAt(0.5, QColor(17, 45, 78));    // #112d4e 青蓝深灰
    bgGradient.setColorAt(1, QColor(10, 25, 41));      // #0a1929
    
    painter.setBrush(bgGradient);
    painter.setPen(Qt::NoPen);
    painter.drawRect(rect());
    
    // 绘制科技网格线（水平）
    painter.setPen(QPen(QColor(100, 210, 255, 20), 1));
    for (int y = 0; y < height(); y += 50) {
        painter.drawLine(0, y, width(), y);
    }
    
    // 绘制科技网格线（垂直）
    for (int x = 0; x < width(); x += 50) {
        painter.drawLine(x, 0, x, height());
    }
    
    // 绘制中心发光效果
    QRadialGradient glowGradient(width() / 2, height() / 2, 400);
    glowGradient.setColorAt(0, QColor(100, 210, 255, 30));
    glowGradient.setColorAt(0.5, QColor(0, 230, 118, 15));
    glowGradient.setColorAt(1, QColor(0, 0, 0, 0));
    
    painter.setBrush(glowGradient);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(width() / 2 - 400, height() / 2 - 400, 800, 800);
    
    // 绘制顶部装饰线 - 霓虹渐变
    QLinearGradient lineGradient(0, 0, width(), 0);
    lineGradient.setColorAt(0, QColor(0, 230, 118));     // #00e676 荧光绿
    lineGradient.setColorAt(0.5, QColor(100, 210, 255)); // #64d2ff 亮青蓝
    lineGradient.setColorAt(1, QColor(186, 104, 200));   // #ba68c8 粉紫
    
    painter.setBrush(lineGradient);
    painter.drawRect(0, 0, width(), 4);
    
    // 绘制底部装饰线
    painter.drawRect(0, height() - 4, width(), 4);
    
    // 绘制左右边框装饰
    painter.setBrush(QColor(100, 210, 255, 100));
    painter.drawRect(0, 0, 2, height());
    painter.drawRect(width() - 2, 0, 2, height());
}

void SplashScreen::setProgress(int value)
{
    progressBar->setValue(value);
}

void SplashScreen::setMessage(const QString &message)
{
    messageLabel->setText(message);
}

void SplashScreen::finish()
{
    fadeAnimation->start();
}

void SplashScreen::setupErrorUI()
{
    int centerX = width() / 2;
    int centerY = height() / 2;
    
    // 错误图标 - 使用盾牌图标风格
    errorIconLabel = new QLabel(this);
    errorIconLabel->setGeometry(centerX - 50, centerY - 140, 100, 100);
    errorIconLabel->setAlignment(Qt::AlignCenter);
    errorIconLabel->setText("🔒");
    errorIconLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 72px;"
        "   color: #ff6b6b;"
        "   background: transparent;"
        "}"
    );
    errorIconLabel->hide();
    
    // 错误标题 - 赛博朋克霓虹风格
    errorTitleLabel = new QLabel(this);
    errorTitleLabel->setGeometry(0, centerY - 20, width(), 55);
    errorTitleLabel->setAlignment(Qt::AlignCenter);
    errorTitleLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 32px;"
        "   font-weight: 800;"
        "   color: #ff6b6b;"
        "   font-family: 'Microsoft YaHei UI', 'SimHei', sans-serif;"
        "   background: transparent;"
        "   letter-spacing: 4px;"
        "}"
    );
    errorTitleLabel->hide();
    
    // 错误详细信息 - 带边框的信息框
    errorMessageLabel = new QLabel(this);
    errorMessageLabel->setGeometry(centerX - 280, centerY + 50, 560, 90);
    errorMessageLabel->setAlignment(Qt::AlignCenter);
    errorMessageLabel->setWordWrap(true);
    errorMessageLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 15px;"
        "   color: #94a3b8;"
        "   font-family: 'Microsoft YaHei UI', sans-serif;"
        "   background: rgba(255, 107, 107, 0.08);"
        "   border: 1px solid rgba(255, 107, 107, 0.3);"
        "   border-radius: 10px;"
        "   padding: 15px;"
        "   line-height: 1.6;"
        "}"
    );
    errorMessageLabel->hide();
    
    // 退出按钮 - 赛博朋克风格
    exitButton = new QPushButton("退 出 程 序", this);
    exitButton->setGeometry(centerX - 100, centerY + 165, 200, 50);
    exitButton->setStyleSheet(
        "QPushButton {"
        "   font-size: 16px;"
        "   font-weight: bold;"
        "   color: #ffffff;"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff6b6b, stop:0.5 #ee5a5a, stop:1 #ff6b6b);"
        "   border: 2px solid #ff6b6b;"
        "   border-radius: 25px;"
        "   font-family: 'Microsoft YaHei UI', sans-serif;"
        "   letter-spacing: 3px;"
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff5252, stop:0.5 #ff1744, stop:1 #ff5252);"
        "   border: 2px solid #ff5252;"
        "}"
        "QPushButton:pressed {"
        "   background: #d32f2f;"
        "   border: 2px solid #d32f2f;"
        "}"
    );
    exitButton->setCursor(Qt::PointingHandCursor);
    exitButton->hide();
    
    connect(exitButton, &QPushButton::clicked, this, [this]() {
        emit exitRequested();
        QApplication::quit();
    });
}

void SplashScreen::showError(const QString &errorTitle, const QString &errorMessage)
{
    m_hasError = true;
    
    // 隐藏正常加载的UI元素
    messageLabel->hide();
    progressBar->hide();
    subtitleLabel->hide();
    
    // 显示错误UI元素
    errorIconLabel->show();
    errorTitleLabel->setText(errorTitle);
    errorTitleLabel->show();
    errorMessageLabel->setText(errorMessage);
    errorMessageLabel->show();
    exitButton->show();
    
    // 更新界面
    update();
}
