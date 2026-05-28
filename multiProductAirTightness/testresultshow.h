#ifndef TESTRESULTshow_H
#define TESTRESULTshow_H

#include <QWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QMap>
#include <QVariant>
#include <QList>
#include <QDateTime>
#include <QPointer>

// 前向声明
class RealTimeMonitor;

namespace Ui {
class TestResultshow;
}

class TestResultshow : public QWidget
{
    Q_OBJECT

public:
    explicit TestResultshow(QWidget *parent = nullptr);
    ~TestResultshow();
    
    // 设置 RealtimeMonitor 实例，用于建立信号连接
    void setRealtimeMonitor(RealTimeMonitor* monitor);
    
    // 设置当前用户角色（用于权限控制）
    void setCurrentUserRole(const QString& role);

public slots:
    // 响应测试结果保存信号，自动刷新表格
    void onTestResultSaved(const QMap<QString, QVariant>& testResult);

private slots:
    // 查询按钮点击事件
    void on_queryButton_clicked();
    // 导出按钮点击事件
    void on_exportButton_clicked();
    // 分页切换事件
    void on_firstPageButton_clicked();
    void on_pageUpButton_clicked();
    void on_pageDownButton_clicked();
    void on_lastPageButton_clicked();
    // 测试结果下拉框变化事件
    void on_testResultComboBox_currentIndexChanged(const QString &arg1);
    // 自动刷新复选框状态变化
    void on_autoRefreshCheckBox_stateChanged(int state);
    // 刷新表格数据
    void refreshTable();
    // 导出为Excel文件
    void exportToExcel(const QList<QMap<QString, QVariant>> &results);
    // 初始化表格
    void initTable();
    // 更新分页信息
    void updatePagination();

private:
    Ui::TestResultshow *ui;
    
    // 当前页码
    int currentPage;
    // 每页显示条数
    int pageSize;
    // 总记录数
    int totalCount;
    // 总页数
    int totalPages;
    
    // 查询条件
    QString currentProductId;
    QString currentOperatorName;
    QString currentTestResult;
    QDateTime currentStartTime;
    QDateTime currentEndTime;
    
    // 自动刷新开关状态
    bool m_autoRefreshEnabled;
    // RealtimeMonitor 指针（用于信号连接）
    QPointer<RealTimeMonitor> m_realtimeMonitor;
    // 当前用户角色
    QString m_currentUserRole;
};

#endif // TESTRESULTshow_H