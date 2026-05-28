#ifndef CHARTDIALOG_H
#define CHARTDIALOG_H

#include <QDialog>
#include <QtCharts>

class ChartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChartDialog(QWidget *parent = nullptr);
    ~ChartDialog();

    // 打开时用主窗口现有历史数据初始化
    void initWithHistory(const QList<QPointF> &pressurePoints,
                         const QList<QPointF> &leakPoints,
                         double axisXMin, double axisXMax,
                         double axisYLeftMin, double axisYLeftMax,
                         double axisYRightMin, double axisYRightMax);

    // 主窗口每次追加新数据点时调用，实时同步
    void appendData(double xIndex, double pressure, double leak);

private:
    QChart       *chart;
    QChartView   *chartView;
    QLineSeries  *leakSeries;
    QLineSeries  *pressureSeries;
    QValueAxis   *axisX;
    QValueAxis   *axisYLeft;
    QValueAxis   *axisYRight;
};

#endif // CHARTDIALOG_H
