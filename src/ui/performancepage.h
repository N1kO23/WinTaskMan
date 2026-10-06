#pragma once

#include <QList>
#include <QWidget>

class QGridLayout;
class QStackedWidget;
struct SystemUsage;

// The Performance tab: CPU usage history as one graph or one per core, and memory usage history.
class PerformancePage : public QWidget
{
  Q_OBJECT

public:
  explicit PerformancePage(QWidget *parent = nullptr);

  void addSample(const SystemUsage &usage);
  void setPerCoreGraphsVisible(bool visible);

private:
  class UsageGraph;

  void setCoreCount(int count);

  QStackedWidget *m_cpuStack = nullptr;
  UsageGraph *m_cpuGraph = nullptr;
  QGridLayout *m_coreGrid = nullptr;
  QList<UsageGraph *> m_coreGraphs;
  UsageGraph *m_memoryGraph = nullptr;
};
