#include "login.h"
#include "ui_login.h"
#include "databasemanager.h"
#include "logmanager.h"
#include <QMessageBox>
#include <QSettings>
#include <QDebug>
#include <QPainter>
#include <QRandomGenerator>
#include <QPixmap>
#include <QBrush>
#include <QPen>

Login::Login(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Login),
    isPasswordVisible(false)
{
    ui->setupUi(this);
    loadRememberedPassword();
}

Login::~Login()
{
    delete ui;
}

void Login::on_loginButton_clicked()
{
    QString username = ui->usernameLineEdit->text().trimmed();
    QString password = ui->passwordLineEdit->text().trimmed();
    
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "登录提示", "用户名和密码不能为空");
        return;
    }
    
    if (validateLogin(username, password)) {
        // 保存记住的密码
        saveRememberedPassword();
        
        // 获取用户角色
        DatabaseManager *dbManager = DatabaseManager::getInstance();
        QMap<QString, QVariant> user = dbManager->getUserByUsername(username);
        QString role = user["role"].toString();
        
        LOG_INFO(QString("用户 %1 登录成功，角色: %2").arg(username).arg(role), "登录");
        
        // 发送登录成功信号
        emit loginSuccess(username, role);
    } else {
        LOG_WARNING(QString("用户 %1 登录失败: 用户名或密码错误").arg(username), "登录");
        QMessageBox::warning(this, "登录提示", "用户名或密码错误");
    }
}

void Login::on_cancelButton_clicked()
{
    emit loginCancel();
    close();
}

void Login::on_togglePasswordButton_clicked()
{
    isPasswordVisible = !isPasswordVisible;
    
    if (isPasswordVisible) {
        ui->passwordLineEdit->setEchoMode(QLineEdit::Normal);
        ui->togglePasswordButton->setText("隐藏密码");
    } else {
        ui->passwordLineEdit->setEchoMode(QLineEdit::Password);
        ui->togglePasswordButton->setText("显示密码");
    }
}

void Login::loadRememberedPassword()
{
    QSettings settings("AirTightSystem", "Login");
    bool rememberPassword = settings.value("rememberPassword", false).toBool();
    QString username = settings.value("username", "").toString();
    QString password = settings.value("password", "").toString();
    
    ui->rememberPasswordCheckBox->setChecked(rememberPassword);
    ui->usernameLineEdit->setText(username);
    if (rememberPassword) {
        ui->passwordLineEdit->setText(password);
    }
}

void Login::saveRememberedPassword()
{
    QSettings settings("AirTightSystem", "Login");
    bool rememberPassword = ui->rememberPasswordCheckBox->isChecked();
    QString username = ui->usernameLineEdit->text().trimmed();
    QString password = ui->passwordLineEdit->text().trimmed();
    
    settings.setValue("rememberPassword", rememberPassword);
    settings.setValue("username", username);
    
    if (rememberPassword) {
        settings.setValue("password", password);
    } else {
        settings.remove("password");
    }
}

bool Login::validateLogin(const QString& username, const QString& password)
{
    DatabaseManager *dbManager = DatabaseManager::getInstance();
    QMap<QString, QVariant> user = dbManager->getUserByUsername(username);
    
    if (user.isEmpty()) {
        LOG_WARNING(QString("用户 %1 登录失败: 用户不存在").arg(username), "登录");
        return false;
    }
    
    if (!user["enabled"].toBool()) {
        LOG_WARNING(QString("用户 %1 登录失败: 用户已被禁用").arg(username), "登录");
        QMessageBox::warning(this, "登录提示", "用户已被禁用");
        return false;
    }
    
    QString hashedPassword = user["password"].toString();
    return dbManager->verifyPassword(password, hashedPassword);
}


