#include "chartdialog.h"
#include <QVBoxLayout>
#include <QPainter>

ChartDialog::ChartDialog(QWidget *parent)
    : QDialog(parent),
      chart(nullptr),
      chartView(nullptr),
      leakSeries(nullptr),
      pressureSeries(nullptr),
      axisX(nullptr),
      axisYLeft(nullptr),
      axisYRight(nullptr)
{
    setWindowTitle("实时数据折线图 - 全屏查看");
    setMinimumSize(1000, 700);
    setWindowFlags(windowFlags() | Qt::Window);

    // 对话框自己的独立序列
    leakSeries = new QLineSeries();
    pressureSeries = new QLineSeries();
    leakSeries->setName("泄露值");
    leakSeries->setColor(QColor(255, 70, 131));
    pressureSeries->setName("压力值");
    pressureSeries->setColor(QColor(50, 150, 255));

    chart = new QChart();
    chart->setTitle("实时数据监测 - 全屏模式");
    chart->setBackgroundBrush(QBrush(QColor(10, 25, 41)));
    chart->setTitleBrush(QBrush(QColor(100, 210, 255)));
    chart->setTitleFont(QFont("Segoe UI", 22, QFont::Bold));
    chart->legend()->hide();
    chart->addSeries(leakSeries);
    chart->addSeries(pressureSeries);

    axisX = new QValueAxis();
    axisX->setRange(0, 50);
    axisX->setLabelFormat("%d");
    axisX->setTitleText("时间 (s)");
    axisX->setTitleFont(QFont("Microsoft YaHei UI", 14, QFont::Bold));
    axisX->setLabelsFont(QFont("Microsoft YaHei UI", 13));

    axisYLeft = new QValueAxis();
    axisYLeft->setRange(0, 200);
    axisYLeft->setLabelFormat("%.1f");
    axisYLeft->setTitleText("压力值");
    axisYLeft->setLinePenColor(pressureSeries->color());
    axisYLeft->setLabelsColor(pressureSeries->color());
    axisYLeft->setTitleBrush(pressureSeries->color());
    axisYLeft->setTitleFont(QFont("Microsoft YaHei UI", 14, QFont::Bold));
    axisYLeft->setLabelsFont(QFont("Microsoft YaHei UI", 13));

    axisYRight = new QValueAxis();
    axisYRight->setRange(0, 10);
    axisYRight->setLabelFormat("%.2f");
    axisYRight->setTitleText("泄漏值");
    axisYRight->setLinePenColor(leakSeries->color());
    axisYRight->setLabelsColor(leakSeries->color());
    axisYRight->setTitleBrush(leakSeries->color());
    axisYRight->setTitleFont(QFont("Microsoft YaHei UI", 14, QFont::Bold));
    axisYRight->setLabelsFont(QFont("Microsoft YaHei UI", 13));

    chart->addAxis(axisX,      Qt::AlignBottom);
    chart->addAxis(axisYLeft,  Qt::AlignLeft);
    chart->addAxis(axisYRight, Qt::AlignRight);

    pressureSeries->attachAxis(axisX);
    pressureSeries->attachAxis(axisYLeft);
    leakSeries->attachAxis(axisX);
    leakSeries->attachAxis(axisYRight);

    chartView = new QChartView(chart, this);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setRenderHint(QPainter::SmoothPixmapTransform);
    chartView->setStyleSheet(
        "QChartView {"
        "    background-color: #0a1929;"
        "    border: 2px solid #64d2ff;"
        "    border-radius: 8px;"
        "}"
    );

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(chartView);
    setLayout(layout);

    setStyleSheet(
        "QDialog { background-color: #0a1929; border: 2px solid #1e5a96; border-radius: 8px; }"
        "QLabel  { color: #f8fafc; }"
    );
}

ChartDialog::~ChartDialog()
{
    // chart 拥有 series 和 axes，chartView 拥有 chart，
    // Qt 父子关系会自动释放，无需手动 delete
}

void ChartDialog::initWithHistory(const QList<QPointF> &pressurePoints,
                                   const QList<QPointF> &leakPoints,
                                   double axisXMin, double axisXMax,
                                   double axisYLeftMin, double axisYLeftMax,
                                   double axisYRightMin, double axisYRightMax)
{
    // 用主窗口的历史数据填充
    pressureSeries->replace(pressurePoints);
    leakSeries->replace(leakPoints);

    axisX->setRange(axisXMin, axisXMax);
    axisYLeft->setRange(axisYLeftMin, axisYLeftMax);
    axisYRight->setRange(axisYRightMin, axisYRightMax);
}

void ChartDialog::appendData(double xIndex, double pressure, double leak)
{
    pressureSeries->append(xIndex, pressure);
    leakSeries->append(xIndex, leak);

    // 跟随主窗口的滚动窗口逻辑：只显示最近 50 个点
    int count = pressureSeries->count();
    if (count > 50) {
        axisX->setRange(xIndex - 50, xIndex);
    } else {
        axisX->setRange(0, qMax(50.0, xIndex));
    }
}
