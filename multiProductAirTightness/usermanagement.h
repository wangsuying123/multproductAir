#ifndef USERMANAGEMENT_H
#define USERMANAGEMENT_H

#include <QWidget>
#include <QDialog>

class DatabaseManager;

namespace Ui {
class UserManagement;
class UserDialog;
}

// 用户信息对话框
class UserDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserDialog(QWidget *parent = nullptr, int userId = -1);
    ~UserDialog();
    
    struct UserData {
        QString username;
        QString employeeId;
        QString password;
        QString role;
        bool enabled;
    };
    
    UserData getUserData() const;

private slots:
    void onSaveButtonClicked();

private:
    Ui::UserDialog *ui;
    int currentUserId;
    DatabaseManager *dbManager;
    
    void loadUserData(int userId);
    bool validateInput();
};

// 用户管理主窗口
class UserManagement : public QWidget
{
    Q_OBJECT

public:
    explicit UserManagement(QWidget *parent = nullptr);
    ~UserManagement();

private slots:
    void on_addUserPushButton_clicked();
    void onEditButtonClicked();
    void onDeleteButtonClicked();
    void on_firstPageButton_clicked();
    void on_prevPageButton_clicked();
    void on_nextPageButton_clicked();
    void on_lastPageButton_clicked();

private:
    Ui::UserManagement *ui;
    DatabaseManager *dbManager;
    
    // 分页相关
    int currentPage;        // 当前页码（从1开始）
    int pageSize;           // 每页显示数量
    int totalRecords;       // 总记录数
    int totalPages;         // 总页数
    QList<QMap<QString, QVariant>> allUsers;  // 所有用户数据
    
    void initUserTable();
    void refreshUserList();
    void createActionButtons(int row, int userId);
    void updatePagination();
    void updatePageButtons();
    void loadPage(int page);
};

#endif // USERMANAGEMENT_H
