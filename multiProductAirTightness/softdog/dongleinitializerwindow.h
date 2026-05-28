#ifndef DONGLEINITIALIZERWINDOW_H
#define DONGLEINITIALIZERWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <QStorageInfo>

class DongleManager;

class DongleInitializerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit DongleInitializerWindow(QWidget *parent = nullptr);
    ~DongleInitializerWindow();

private slots:
    void onRefreshClicked();
    void onAuthorizeClicked();
    void onStorageSelected(int index);

private:
    void setupUI();
    void refreshStorageList();
    void appendLog(const QString &message, const QString &type = "INFO");

    // UI组件
    QComboBox *m_storageComboBox;
    QPushButton *m_refreshButton;
    QPushButton *m_authorizeButton;
    QLabel *m_storageInfoLabel;
    QTextEdit *m_logTextEdit;
    QCheckBox *m_permanentCheckBox;
    QDateTimeEdit *m_startDateTimeEdit;
    QDateTimeEdit *m_endDateTimeEdit;
    QGroupBox *m_timeGroupBox;

    // 数据
    DongleManager *m_dongleManager;
    QList<QStorageInfo> m_storageList;
};

#endif // DONGLEINITIALIZERWINDOW_H
