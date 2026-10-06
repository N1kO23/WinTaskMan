#include "performancepage.h"

#include "system/trackers.h"

#include <QChart>
#include <QChartView>
#include <QGraphicsLayout>
#include <QGridLayout>
#include <QLineSeries>
#include <QScrollArea>
#include <QStackedWidget>
#include <QValueAxis>
#include <QVBoxLayout>

namespace
{
constexpr int kHistoryLength = 60; // samples shown per graph
constexpr int kCoreGridColumns = 4;
} // namespace

// A scrolling 0-100% history graph in the classic task manager style.
class PerformancePage::UsageGraph : public QChartView
{
public:
  enum class Size
  {
    Large,   // axis labels and a thick line
    Compact, // only the plot, small enough for the per-core grid
  };

  UsageGraph(const QColor &lineColor, const QColor &gridColor, Size size, QWidget *parent = nullptr)
      : QChartView(new QChart, parent), m_series(new QLineSeries)
  {
    m_series->setPen(QPen(lineColor, size == Size::Large ? 2 : 1));

    QChart *graph = chart();
    graph->addSeries(m_series);
    graph->legend()->hide();
    graph->setBackgroundBrush(Qt::black);
    graph->setPlotAreaBackgroundBrush(Qt::black);
    graph->setPlotAreaBackgroundVisible(true);
    if (size == Size::Compact)
    {
      graph->setMargins(QMargins());
      graph->layout()->setContentsMargins(0, 0, 0, 0);
      graph->setBackgroundRoundness(0);
    }

    auto *axisX = new QValueAxis;
    axisX->setRange(0, kHistoryLength);
    auto *axisY = new QValueAxis;
    axisY->setRange(0, 100);
    for (QValueAxis *axis : {axisX, axisY})
    {
      axis->setGridLinePen(QPen(gridColor));
      axis->setLabelsVisible(size == Size::Large);
      axis->setLineVisible(size == Size::Large);
    }
    graph->addAxis(axisX, Qt::AlignBottom);
    graph->addAxis(axisY, Qt::AlignLeft);
    m_series->attachAxis(axisX);
    m_series->attachAxis(axisY);

    setRenderHint(QPainter::Antialiasing);
  }

  void addValue(double percent)
  {
    m_values.append(percent);
    if (m_values.size() > kHistoryLength)
      m_values.removeFirst();

    // The newest value sits at the right edge and older ones scroll off to the left.
    QList<QPointF> points;
    points.reserve(m_values.size());
    const int firstX = kHistoryLength - m_values.size() + 1;
    for (int i = 0; i < m_values.size(); ++i)
      points.append(QPointF(firstX + i, m_values[i]));
    m_series->replace(points);
  }

private:
  QLineSeries *m_series;
  QList<double> m_values;
};

PerformancePage::PerformancePage(QWidget *parent)
    : QWidget(parent),
      m_cpuGraph(new UsageGraph(Qt::green, Qt::darkGreen, UsageGraph::Size::Large, this)),
      m_memoryGraph(new UsageGraph(Qt::blue, Qt::darkBlue, UsageGraph::Size::Large, this))
{
  auto *coreContainer = new QWidget;
  m_coreGrid = new QGridLayout(coreContainer);
  m_coreGrid->setSpacing(6);
  m_coreGrid->setContentsMargins(0, 0, 0, 0);

  auto *coreScrollArea = new QScrollArea;
  coreScrollArea->setWidgetResizable(true);
  coreScrollArea->setWidget(coreContainer);
  coreScrollArea->setMinimumHeight(180);

  m_cpuStack = new QStackedWidget(this);
  m_cpuStack->addWidget(m_cpuGraph);
  m_cpuStack->addWidget(coreScrollArea);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 10, 10);
  layout->setSpacing(8);
  layout->addWidget(m_cpuStack, 1);
  layout->addWidget(m_memoryGraph, 1);
}

void PerformancePage::addSample(const SystemUsage &usage)
{
  if (usage.corePercents.size() != m_coreGraphs.size())
    setCoreCount(usage.corePercents.size());

  m_cpuGraph->addValue(usage.cpuPercent);
  for (int core = 0; core < m_coreGraphs.size(); ++core)
    m_coreGraphs[core]->addValue(usage.corePercents[core]);
  m_memoryGraph->addValue(usage.memoryPercent);
}

void PerformancePage::setPerCoreGraphsVisible(bool visible)
{
  m_cpuStack->setCurrentIndex(visible ? 1 : 0);
}

void PerformancePage::setCoreCount(int count)
{
  qDeleteAll(m_coreGraphs);
  m_coreGraphs.clear();

  for (int core = 0; core < count; ++core)
  {
    auto *graph = new UsageGraph(QColor::fromHsv(core * 40 % 360, 200, 200), Qt::darkGreen, UsageGraph::Size::Compact);
    graph->setMinimumHeight(80);
    m_coreGrid->addWidget(graph, core / kCoreGridColumns, core % kCoreGridColumns);
    m_coreGraphs.append(graph);
  }
}
