#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QPushButton>

class SplashScreen : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreen(QWidget *parent = nullptr);
    ~SplashScreen();

    void setProgress(int value);
    void setMessage(const QString &message);
    void finish();
    
    // 显示错误信息（加密狗验证失败等）
    void showError(const QString &errorTitle, const QString &errorMessage);

signals:
    void loadingFinished();
    void exitRequested();  // 用户点击退出按钮

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *logoLabel;
    QLabel *titleLabel;
    QLabel *subtitleLabel;
    QLabel *messageLabel;
    QProgressBar *progressBar;
    QLabel *versionLabel;
    QLabel *copyrightLabel;
    QGraphicsOpacityEffect *opacityEffect;
    QPropertyAnimation *fadeAnimation;
    
    // 错误显示相关
    QLabel *errorIconLabel;
    QLabel *errorTitleLabel;
    QLabel *errorMessageLabel;
    QPushButton *exitButton;
    QPushButton *retryButton;
    bool m_hasError;
    
    void setupUI();
    void setupAnimations();
    void setupErrorUI();
};

#endif // SPLASHSCREEN_H
