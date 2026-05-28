#ifndef LOGIN_H
#define LOGIN_H

#include <QWidget>

namespace Ui {
class Login;
}

class Login : public QWidget
{
    Q_OBJECT

public:
    explicit Login(QWidget *parent = nullptr);
    ~Login();

signals:
    void loginSuccess(const QString& username, const QString& role);
    void loginCancel();

private slots:
    void on_loginButton_clicked();
    void on_cancelButton_clicked();
    void on_togglePasswordButton_clicked();

private:
    Ui::Login *ui;
    bool isPasswordVisible;
    void loadRememberedPassword();
    void saveRememberedPassword();
    bool validateLogin(const QString& username, const QString& password);
};

#endif // LOGIN_H
