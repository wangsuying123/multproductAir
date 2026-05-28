#include "testresultshow.h"
#include "ui_testresultshow.h"
#include "databasemanager.h"
#include "logmanager.h"
#include "realtimemonitor.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>

TestResultshow::TestResultshow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TestResultshow),
    currentPage(1),
    pageSize(10),
    totalCount(0),
    totalPages(0),
    m_autoRefreshEnabled(true),
    m_realtimeMonitor(nullptr),
    m_currentUserRole("")
{
    ui->setupUi(this);
    
    // 移除固定几何尺寸，确保页面能正确适应窗口大小变化
    this->setGeometry(QRect());
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 设置布局管理器的属性，确保它能正确适应窗口大小变化
    if (this->layout()) {
        this->layout()->setSizeConstraint(QLayout::SetNoConstraint);
        this->layout()->setContentsMargins(5, 5, 5, 5);
        this->layout()->setSpacing(5);
    }
    
    // 初始化表格
    initTable();
    
    // 初始化查询条件
    currentProductId = "";
    currentOperatorName = "";
    currentTestResult = "";
    
    // 初始化时间范围为当天
    QDateTime now = QDateTime::currentDateTime();
    // 当天的00:00:00
    currentStartTime = QDateTime(now.date(), QTime(0, 0, 0));
    // 当天的23:59:59
    currentEndTime = QDateTime(now.date(), QTime(23, 59, 59));
    
    // 更新UI中的时间选择控件
    ui->startTimeDateTimeEdit->setDateTime(currentStartTime);
    ui->endTimeDateTimeEdit->setDateTime(currentEndTime);
    
    // 刷新表格数据
    LOG_INFO(QString("开始查询测试结果，条件：产品ID=%1, 操作员=%2, 测试结果=%3, 时间范围=%4 至 %5").arg(currentProductId).arg(currentOperatorName).arg(currentTestResult).arg(currentStartTime.toString("yyyy-MM-dd HH:mm:ss")).arg(currentEndTime.toString("yyyy-MM-dd HH:mm:ss")), "测试结果");
    refreshTable();
    
    // 记录查询条件
    LOG_DEBUG(QString("查询条件：产品ID=%1, 操作员=%2, 测试结果=%3, 开始时间=%4, 结束时间=%5").arg(currentProductId).arg(currentOperatorName).arg(currentTestResult).arg(currentStartTime.toString("yyyy-MM-dd HH:mm:ss")).arg(currentEndTime.toString("yyyy-MM-dd HH:mm:ss")), "测试结果");
    
    // 调试：检查数据库连接状态和数据
    QSqlQuery query("SELECT COUNT(*) FROM t_test_results");
    if (query.exec()) {
        if (query.next()) {
            LOG_INFO(QString("数据库中测试结果表总记录数：%1").arg(query.value(0).toInt()), "测试结果");
        }
    } else {
        LOG_ERROR(QString("查询测试结果表记录数失败：%1").arg(query.lastError().text()), "测试结果");
    }
    
    // 记录最近的几条测试结果
    query.exec("SELECT * FROM t_test_results ORDER BY test_time DESC LIMIT 5");
    int recordIndex = 0;
    while (query.next()) {
        LOG_DEBUG(QString("最近测试结果记录 %1: 产品ID=%2, 结果=%3, 时间=%4").arg(recordIndex++).arg(query.value("product_id").toString()).arg(query.value("test_result").toString()).arg(query.value("test_time").toString()), "测试结果");
    }
}

TestResultshow::~TestResultshow()
{
    delete ui;
}

// 查询按钮点击事件
void TestResultshow::on_queryButton_clicked()
{
    // 获取查询条件
    currentProductId = ui->productIdLineEdit->text().trimmed();
    currentOperatorName = ui->operatorLineEdit->text().trimmed();
    currentTestResult = ui->testResultComboBox->currentText().trimmed();
    
    // 获取时间范围条件
    currentStartTime = ui->startTimeDateTimeEdit->dateTime();
    currentEndTime = ui->endTimeDateTimeEdit->dateTime();
    
    // 如果选择的是"全部"，则清空测试结果条件
    if (currentTestResult == "全部") {
        currentTestResult = "";
    }
    
    // 重置到第一页
    currentPage = 1;
    
    // 刷新表格数据
    refreshTable();
}

// 导出按钮点击事件
void TestResultshow::on_exportButton_clicked()
{
    // 获取当前查询条件
    QString productId = ui->productIdLineEdit->text().trimmed();
    QString operatorName = ui->operatorLineEdit->text().trimmed();
    QString testResult = ui->testResultComboBox->currentText().trimmed();
    
    // 获取当前时间范围条件
    QDateTime startTime = ui->startTimeDateTimeEdit->dateTime();
    QDateTime endTime = ui->endTimeDateTimeEdit->dateTime();
    
    // 如果选择的是"全部"，则清空测试结果条件
    if (testResult == "全部") {
        testResult = "";
    }
    
    // 导出测试结果
    LOG_INFO(QString("开始导出测试结果，条件：产品ID=%1, 操作员=%2, 测试结果=%3, 时间范围=%4 至 %5").arg(productId).arg(operatorName).arg(testResult).arg(startTime.toString("yyyy-MM-dd HH:mm:ss")).arg(endTime.toString("yyyy-MM-dd HH:mm:ss")), "测试结果");
    QList<QMap<QString, QVariant>> results = DatabaseManager::getInstance()->exportTestResults(productId, operatorName, testResult, startTime, endTime);
    
    if (results.isEmpty()) {
        LOG_WARNING("没有数据可以导出", "测试结果");
        QMessageBox::information(this, "提示", "没有数据可以导出！");
        return;
    }
    
    // 导出为Excel文件
    exportToExcel(results);
    LOG_INFO(QString("成功导出 %1 条测试结果").arg(results.size()), "测试结果");
}

// 首页按钮点击事件
void TestResultshow::on_firstPageButton_clicked()
{
    if (currentPage != 1) {
        currentPage = 1;
        refreshTable();
    }
}

// 上一页按钮点击事件
void TestResultshow::on_pageUpButton_clicked()
{
    if (currentPage > 1) {
        currentPage--;
        refreshTable();
    }
}

// 下一页按钮点击事件
void TestResultshow::on_pageDownButton_clicked()
{
    if (currentPage < totalPages) {
        currentPage++;
        refreshTable();
    }
}

// 末页按钮点击事件
void TestResultshow::on_lastPageButton_clicked()
{
    if (currentPage != totalPages && totalPages > 0) {
        currentPage = totalPages;
        refreshTable();
    }
}

// 测试结果下拉框变化事件
void TestResultshow::on_testResultComboBox_currentIndexChanged(const QString &)
{
    // 不需要额外处理，查询时会获取当前值
}

// 刷新表格数据
void TestResultshow::refreshTable()
{
    // 清空表格
    ui->resultTableWidget->setRowCount(0);
    
    // 获取测试结果数据
    QList<QMap<QString, QVariant>> results = DatabaseManager::getInstance()->getTestResults(
        currentProductId, currentOperatorName, currentTestResult, currentStartTime, currentEndTime, currentPage, pageSize);
    
    // 获取总记录数
    totalCount = DatabaseManager::getInstance()->getTestResultCount(
        currentProductId, currentOperatorName, currentTestResult, currentStartTime, currentEndTime);
    
    // 计算总页数
    totalPages = (totalCount + pageSize - 1) / pageSize;
    
    // 更新分页信息
    updatePagination();
    
    // 填充表格数据
    int row = 0;
    for (const QMap<QString, QVariant> &result : results) {
        ui->resultTableWidget->insertRow(row);
        
        // 序号（当前页的行号）
        int globalIndex = (currentPage - 1) * pageSize + row + 1;
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(globalIndex));
        indexItem->setTextAlignment(Qt::AlignCenter);
        ui->resultTableWidget->setItem(row, 0, indexItem);
        
        // 程序号
        QTableWidgetItem *serialItem = new QTableWidgetItem(result["serial_number"].toString());
        serialItem->setTextAlignment(Qt::AlignCenter);
        ui->resultTableWidget->setItem(row, 1, serialItem);
        
        // 产品编号
        QTableWidgetItem *productIdItem = new QTableWidgetItem(result["product_id"].toString());
        ui->resultTableWidget->setItem(row, 2, productIdItem);
        
        // 测试时间
        QTableWidgetItem *testTimeItem = new QTableWidgetItem(result["test_time"].toString());
        ui->resultTableWidget->setItem(row, 3, testTimeItem);
        
        // 压力值
        QTableWidgetItem *pressureItem = new QTableWidgetItem(QString::number(result["pressure_value"].toDouble(), 'f', 2) + " " + result["pressure_unit"].toString());
        ui->resultTableWidget->setItem(row, 4, pressureItem);
        
        // 泄露值
        QTableWidgetItem *leakItem = new QTableWidgetItem(QString::number(result["leak_value"].toDouble(), 'f', 2) + " " + result["leak_unit"].toString());
        ui->resultTableWidget->setItem(row, 5, leakItem);
        
        // 测试结果
        QTableWidgetItem *resultItem = new QTableWidgetItem(result["test_result"].toString());
        // 根据测试结果设置不同颜色
        if (result["test_result"].toString() == "通过") {
            resultItem->setForeground(Qt::green);
        } else if (result["test_result"].toString() == "不通过") {
            resultItem->setForeground(Qt::red);
        }
        ui->resultTableWidget->setItem(row, 6, resultItem);
        
        // 报警代码
        QTableWidgetItem *alarmCodeItem = new QTableWidgetItem(result["alarm_code"].toString());
        ui->resultTableWidget->setItem(row, 7, alarmCodeItem);
        
        // 操作员
        QTableWidgetItem *operatorItem = new QTableWidgetItem(result["operator_name"].toString());
        ui->resultTableWidget->setItem(row, 8, operatorItem);
        
        row++;
    }
}

// 导出为Excel文件
void TestResultshow::exportToExcel(const QList<QMap<QString, QVariant>> &results)
{
    // 打开文件保存对话框
    QString fileName = QFileDialog::getSaveFileName(
        this, "导出测试结果", 
        QString("测试结果_%1.xls").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")), 
        "Excel文件 (*.xls *.xlsx);;所有文件 (*.*)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(QString("无法打开文件进行写入：%1").arg(fileName), "测试结果");
        QMessageBox::critical(this, "错误", "无法打开文件进行写入！");
        return;
    }
    
    QTextStream out(&file);
    
    // 写入表头
    out << "序号\t程序号\t产品编号\t测试时间\t测试压力\t泄漏值\t测试结果\t报警代码\t操作员\n";
    
    // 写入数据
    int exportIndex = 1;
    for (const QMap<QString, QVariant> &result : results) {
        out << exportIndex++ << "\t";
        out << result["serial_number"].toString() << "\t";
        out << result["product_id"].toString() << "\t";
        out << result["test_time"].toString() << "\t";
        out << QString::number(result["pressure_value"].toDouble(), 'f', 2) << " " << result["pressure_unit"].toString() << "\t";
        out << QString::number(result["leak_value"].toDouble(), 'f', 2) << " " << result["leak_unit"].toString() << "\t";
        out << result["test_result"].toString() << "\t";
        out << result["alarm_code"].toString() << "\t";
        out << result["operator_name"].toString() << "\n";
    }
    
    file.close();
    
    QMessageBox::information(this, "提示", "测试结果导出成功！");
}

// 初始化表格
void TestResultshow::initTable()
{
    // 设置表格列数（序号 + 程序号 + 产品编号 + 其他6列 = 9列）
    ui->resultTableWidget->setColumnCount(9);
    
    // 设置表头
    QStringList headers;
    headers << "序号" << "程序号" << "产品编号" << "测试时间" << "测试压力" << "泄漏值" << "测试结果" << "报警代码" << "操作员";
    ui->resultTableWidget->setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    ui->resultTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers); // 禁止编辑
    ui->resultTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows); // 整行选择
    ui->resultTableWidget->setSelectionMode(QAbstractItemView::SingleSelection); // 单选
    ui->resultTableWidget->setAlternatingRowColors(true); // 交替行颜色
    ui->resultTableWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 禁用垂直滚动条
    ui->resultTableWidget->verticalHeader()->setDefaultSectionSize(32); // 行高32px，10行共320px
    ui->resultTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // 自适应列宽
    // 序号列（列0）固定宽度
    ui->resultTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->resultTableWidget->setColumnWidth(0, 50);
    // 测试时间列（列3）固定宽度，确保完整显示
    ui->resultTableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    ui->resultTableWidget->setColumnWidth(3, 180);
}

// 更新分页信息
void TestResultshow::updatePagination()
{
    // 更新总记录数和总页数显示
    ui->totalCountLabel->setText(QString("共 %1 条").arg(totalCount));
    ui->totalPagesLabel->setText(QString("共 %1 页").arg(totalPages));
    
    // 更新当前页码显示
    ui->currentPageLabel->setText(QString("第 %1 页").arg(currentPage));
    
    // 更新分页按钮状态
    ui->firstPageButton->setEnabled(currentPage > 1);
    ui->pageUpButton->setEnabled(currentPage > 1);
    ui->pageDownButton->setEnabled(currentPage < totalPages);
    ui->lastPageButton->setEnabled(currentPage < totalPages);
}

// 设置 RealtimeMonitor 实例，建立信号连接
void TestResultshow::setRealtimeMonitor(RealTimeMonitor* monitor)
{
    if (monitor) {
        m_realtimeMonitor = monitor;
        // 连接测试结果保存信号到自动刷新槽函数
        connect(m_realtimeMonitor, &RealTimeMonitor::testResultSaved,
                this, &TestResultshow::onTestResultSaved);
        LOG_INFO("已连接到 RealtimeMonitor 的 testResultSaved 信号", "测试结果");
    }
}

// 设置当前用户角色（用于权限控制）
void TestResultshow::setCurrentUserRole(const QString& role)
{
    m_currentUserRole = role;
    LOG_INFO(QString("设置用户角色：%1").arg(role), "测试结果");
}

// 响应测试结果保存信号，自动刷新表格
void TestResultshow::onTestResultSaved(const QMap<QString, QVariant>& testResult)
{
    Q_UNUSED(testResult);
    
    // 检查自动刷新是否启用
    if (!m_autoRefreshEnabled) {
        LOG_DEBUG("自动刷新已禁用，跳过刷新", "测试结果");
        return;
    }
    
    // 重置到第一页以显示最新数据
    currentPage = 1;
    
    // 刷新表格数据（保持当前查询条件不变）
    refreshTable();
    
    LOG_INFO("自动刷新：检测到新测试结果，已更新表格", "测试结果");
}

// 自动刷新复选框状态变化
void TestResultshow::on_autoRefreshCheckBox_stateChanged(int state)
{
    m_autoRefreshEnabled = (state == Qt::Checked);
    LOG_INFO(QString("自动刷新功能已%1").arg(m_autoRefreshEnabled ? "启用" : "禁用"), "测试结果");
}
