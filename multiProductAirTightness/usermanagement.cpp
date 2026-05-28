#include "usermanagement.h"
#include "ui_usermanagement.h"
#include "ui_userdialog.h"
#include <QTableWidgetItem>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMessageBox>
#include "databasemanager.h"

// ============ UserDialog 实现 ============

UserDialog::UserDialog(QWidget *parent, int userId)
    : QDialog(parent),
      ui(new Ui::UserDialog),
      currentUserId(userId),
      dbManager(DatabaseManager::getInstance())
{
    ui->setupUi(this);
    
    // 连接保存按钮的点击事件
    connect(ui->saveButton, &QPushButton::clicked, this, &UserDialog::onSaveButtonClicked);
    
    if (userId > 0) {
        setWindowTitle("编辑用户");
        // 修改提示信息为编辑模式
        ui->hintLabel->setText("📝 编辑用户信息（标记 * 为必填项，密码留空则不修改）");
        loadUserData(userId);
    } else {
        setWindowTitle("新增用户");
        ui->hintLabel->setText("📝 请填写以下信息（标记 * 为必填项）");
    }
}

UserDialog::~UserDialog()
{
    delete ui;
}

void UserDialog::loadUserData(int userId)
{
    QMap<QString, QVariant> user = dbManager->getUserById(userId);
    if (!user.isEmpty()) {
        ui->nameLineEdit->setText(user["username"].toString());
        ui->employeeIdLineEdit->setText(user["employee_id"].toString());
        ui->roleComboBox->setCurrentText(user["role"].toString());
        bool enabled = user["enabled"].toBool();
        ui->enableRadioButton->setChecked(enabled);
        ui->disableRadioButton->setChecked(!enabled);
        // 编辑时密码可以为空(不修改密码)
        ui->passwordLineEdit->setPlaceholderText("留空则不修改密码");
    }
}

UserDialog::UserData UserDialog::getUserData() const
{
    UserData data;
    data.username = ui->nameLineEdit->text();
    data.employeeId = ui->employeeIdLineEdit->text();
    data.password = ui->passwordLineEdit->text();
    data.role = ui->roleComboBox->currentText();
    data.enabled = ui->enableRadioButton->isChecked();
    return data;
}

bool UserDialog::validateInput()
{
    // 验证用户名
    if (ui->nameLineEdit->text().trimmed().isEmpty()) {
        ui->hintLabel->setText("❌ 错误：用户名不能为空！");
        ui->hintLabel->setStyleSheet(
            "QLabel {"
            "    color: #ff4d4f;"
            "    font-size: 15px;"
            "    font-weight: 700;"
            "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "                                stop:0 #fff1f0, stop:1 #ffccc7);"
            "    border: 2px solid #ff4d4f;"
            "    border-radius: 8px;"
            "    padding: 12px 16px;"
            "}"
        );
        ui->nameLineEdit->setFocus();
        return false;
    }
    
    // 验证密码（仅在新增用户时）
    if (currentUserId <= 0 && ui->passwordLineEdit->text().trimmed().isEmpty()) {
        ui->hintLabel->setText("❌ 错误：密码不能为空！");
        ui->hintLabel->setStyleSheet(
            "QLabel {"
            "    color: #ff4d4f;"
            "    font-size: 15px;"
            "    font-weight: 700;"
            "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "                                stop:0 #fff1f0, stop:1 #ffccc7);"
            "    border: 2px solid #ff4d4f;"
            "    border-radius: 8px;"
            "    padding: 12px 16px;"
            "}"
        );
        ui->passwordLineEdit->setFocus();
        return false;
    }
    
    return true;
}

void UserDialog::onSaveButtonClicked()
{
    if (validateInput()) {
        accept();  // 验证通过，关闭对话框
    }
}

// ============ UserManagement 实现 ============

UserManagement::UserManagement(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UserManagement),
    dbManager(DatabaseManager::getInstance()),
    currentPage(1),
    pageSize(4),
    totalRecords(0),
    totalPages(0)
{
    ui->setupUi(this);
    
    initUserTable();
    refreshUserList();
}

UserManagement::~UserManagement()
{
    delete ui;
}

void UserManagement::initUserTable()
{
    // 禁用水平滚动条
    ui->userTableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // 设置表头字体
    QFont headerFont("Microsoft YaHei UI", 14, QFont::Bold);
    ui->userTableWidget->horizontalHeader()->setFont(headerFont);

    // 使用自适应列宽模式
    QHeaderView *header = ui->userTableWidget->horizontalHeader();
    
    // ID列 - 固定宽度
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->userTableWidget->setColumnWidth(0, 60);
    
    // 用户名列 - 拉伸
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    
    // 工号列 - 拉伸
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    
    // 角色列 - 拉伸
    header->setSectionResizeMode(3, QHeaderView::Stretch);
    
    // 状态列 - 拉伸
    header->setSectionResizeMode(4, QHeaderView::Stretch);
    
    // 操作列 - 固定宽度，确保按钮始终可见
    header->setSectionResizeMode(5, QHeaderView::Fixed);
    ui->userTableWidget->setColumnWidth(5, 150);
}

void UserManagement::refreshUserList()
{
    // 从数据库获取所有用户
    allUsers = dbManager->getAllUsers();
    totalRecords = allUsers.size();
    totalPages = (totalRecords + pageSize - 1) / pageSize;  // 向上取整
    
    if (totalPages == 0) {
        totalPages = 1;
    }
    
    // 确保当前页在有效范围内
    if (currentPage > totalPages) {
        currentPage = totalPages;
    }
    if (currentPage < 1) {
        currentPage = 1;
    }
    
    // 加载当前页数据
    loadPage(currentPage);
    
    // 更新分页信息
    updatePagination();
}

void UserManagement::loadPage(int page)
{
    // 清空表格
    ui->userTableWidget->setRowCount(0);
    
    // 计算当前页的数据范围
    int startIndex = (page - 1) * pageSize;
    int endIndex = qMin(startIndex + pageSize, totalRecords);
    
    // 设置表格行数
    int rowCount = endIndex - startIndex;
    ui->userTableWidget->setRowCount(rowCount);

    // 统一字体
    QFont cellFont("Microsoft YaHei UI", 14);

    // 填充当前页数据
    for (int i = 0; i < rowCount; ++i) {
        const QMap<QString, QVariant>& user = allUsers[startIndex + i];
        int userId = user["id"].toInt();
        
        // ID
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(userId));
        idItem->setTextAlignment(Qt::AlignCenter);
        idItem->setFont(cellFont);
        ui->userTableWidget->setItem(i, 0, idItem);
        
        // 用户名
        QTableWidgetItem *nameItem = new QTableWidgetItem(user["username"].toString());
        nameItem->setFont(cellFont);
        ui->userTableWidget->setItem(i, 1, nameItem);
        
        // 年龄(显示工号)
        QTableWidgetItem *ageItem = new QTableWidgetItem(user["employee_id"].toString());
        ageItem->setTextAlignment(Qt::AlignCenter);
        ageItem->setFont(cellFont);
        ui->userTableWidget->setItem(i, 2, ageItem);
        
        // 手机号(显示角色)
        QTableWidgetItem *phoneItem = new QTableWidgetItem(user["role"].toString());
        phoneItem->setFont(cellFont);
        ui->userTableWidget->setItem(i, 3, phoneItem);
        
        // 邮箱(显示状态)
        QString status = user["enabled"].toBool() ? "启用" : "禁用";
        QTableWidgetItem *emailItem = new QTableWidgetItem(status);
        emailItem->setFont(cellFont);
        ui->userTableWidget->setItem(i, 4, emailItem);
        
        // 操作列 - 设置一个透明的空item，确保单元格背景不会遮挡按钮
        QTableWidgetItem *actionItem = new QTableWidgetItem();
        actionItem->setFlags(Qt::NoItemFlags);  // 禁用所有交互
        actionItem->setBackground(QBrush(Qt::transparent));  // 设置透明背景
        ui->userTableWidget->setItem(i, 5, actionItem);
        
        // 操作按钮
        createActionButtons(i, userId);
    }
}

void UserManagement::updatePagination()
{
    // 更新页面信息标签
    ui->pageInfoLabel->setText(QString("共 %1 条记录").arg(totalRecords));
    ui->currentPageLabel->setText(QString("第 %1 页 / 共 %2 页").arg(currentPage).arg(totalPages));
    
    // 更新按钮状态
    updatePageButtons();
}

void UserManagement::updatePageButtons()
{
    // 首页和上一页按钮
    ui->firstPageButton->setEnabled(currentPage > 1);
    ui->prevPageButton->setEnabled(currentPage > 1);
    
    // 下一页和末页按钮
    ui->nextPageButton->setEnabled(currentPage < totalPages);
    ui->lastPageButton->setEnabled(currentPage < totalPages);
}

void UserManagement::on_firstPageButton_clicked()
{
    if (currentPage != 1) {
        currentPage = 1;
        loadPage(currentPage);
        updatePagination();
    }
}

void UserManagement::on_prevPageButton_clicked()
{
    if (currentPage > 1) {
        currentPage--;
        loadPage(currentPage);
        updatePagination();
    }
}

void UserManagement::on_nextPageButton_clicked()
{
    if (currentPage < totalPages) {
        currentPage++;
        loadPage(currentPage);
        updatePagination();
    }
}

void UserManagement::on_lastPageButton_clicked()
{
    if (currentPage != totalPages) {
        currentPage = totalPages;
        loadPage(currentPage);
        updatePagination();
    }
}

void UserManagement::createActionButtons(int row, int userId)
{
    QWidget *widget = new QWidget();
    widget->setStyleSheet("background: transparent;");
    
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(5, 3, 5, 3);
    layout->setSpacing(8);
    
    // 编辑按钮 - 使用最简单直接的方式
    QPushButton *editBtn = new QPushButton("编辑");
    editBtn->setProperty("userId", userId);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setFixedSize(60, 28);
    
    // 使用最基础的样式，不使用渐变
    editBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: rgb(255, 255, 255);"
        "    color: rgb(24, 144, 255);"
        "    border: 1px solid rgb(24, 144, 255);"
        "    border-radius: 3px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgb(230, 247, 255);"
        "}"
    );
    connect(editBtn, &QPushButton::clicked, this, &UserManagement::onEditButtonClicked);
    
    // 删除按钮
    QPushButton *deleteBtn = new QPushButton("删除");
    deleteBtn->setProperty("userId", userId);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setFixedSize(60, 28);
    
    deleteBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: rgb(255, 255, 255);"
        "    color: rgb(255, 77, 79);"
        "    border: 1px solid rgb(255, 77, 79);"
        "    border-radius: 3px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgb(255, 241, 240);"
        "}"
    );
    connect(deleteBtn, &QPushButton::clicked, this, &UserManagement::onDeleteButtonClicked);
    
    layout->addWidget(editBtn);
    layout->addWidget(deleteBtn);
    layout->addStretch();
    
    // 提升widget的层级
    widget->raise();
    
    ui->userTableWidget->setCellWidget(row, 5, widget);
}

void UserManagement::on_addUserPushButton_clicked()
{
    UserDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        UserDialog::UserData data = dialog.getUserData();
        
        // 验证输入
        if (data.username.isEmpty()) {
            QMessageBox::warning(this, "提示", "请输入用户名");
            return;
        }
        
        if (data.password.isEmpty()) {
            QMessageBox::warning(this, "提示", "请输入密码");
            return;
        }
        
        // 检查用户名是否已存在
        QMap<QString, QVariant> existingUser = dbManager->getUserByUsername(data.username);
        if (!existingUser.isEmpty()) {
            QMessageBox::warning(this, "提示", "用户名已存在");
            return;
        }
        
        // 添加用户
        if (dbManager->addUser(data.username, data.employeeId, data.password, data.role, data.enabled) > 0) {
            QMessageBox::information(this, "提示", "用户添加成功");
            refreshUserList();
            dbManager->addOperationLog("admin", "新增用户", "UserManagement", 
                QString("用户名=%1").arg(data.username), true);
        } else {
            QMessageBox::warning(this, "提示", "用户添加失败：" + dbManager->getLastError());
        }
    }
}

void UserManagement::onEditButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    
    int userId = btn->property("userId").toInt();
    
    UserDialog dialog(this, userId);
    if (dialog.exec() == QDialog::Accepted) {
        UserDialog::UserData data = dialog.getUserData();
        
        // 验证输入
        if (data.username.isEmpty()) {
            QMessageBox::warning(this, "提示", "请输入用户名");
            return;
        }
        
        // 更新用户
        if (dbManager->updateUser(userId, data.username, data.employeeId, data.password, data.role, data.enabled)) {
            QMessageBox::information(this, "提示", "用户更新成功");
            refreshUserList();
            dbManager->addOperationLog("admin", "编辑用户", "UserManagement", 
                QString("用户ID=%1, 用户名=%2").arg(userId).arg(data.username), true);
        } else {
            QMessageBox::warning(this, "提示", "用户更新失败：" + dbManager->getLastError());
        }
    }
}

void UserManagement::onDeleteButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    
    int userId = btn->property("userId").toInt();
    
    // 获取用户信息
    QMap<QString, QVariant> user = dbManager->getUserById(userId);
    if (user.isEmpty()) {
        QMessageBox::warning(this, "提示", "用户不存在");
        return;
    }
    
    QString username = user["username"].toString();
    
    // 防止删除 admin 用户
    if (username.toLower() == "admin") {
        QMessageBox::warning(this, "提示", "系统管理员账户 (admin) 不能被删除");
        return;
    }
    
    if (QMessageBox::question(this, "确认", 
        QString("确定要删除用户 \"%1\" 吗？").arg(username)) == QMessageBox::Yes) {
        
        if (dbManager->deleteUser(userId)) {
            QMessageBox::information(this, "提示", "用户删除成功");
            refreshUserList();
            dbManager->addOperationLog("admin", "删除用户", "UserManagement", 
                QString("用户ID=%1, 用户名=%2").arg(userId).arg(username), true);
        } else {
            QMessageBox::warning(this, "提示", "用户删除失败：" + dbManager->getLastError());
        }
    }
}
