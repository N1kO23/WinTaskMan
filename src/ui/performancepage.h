#pragma once

#include <QList>
#include <QPair>
#include <QWidget>

#include <functional>

class HistoryGraph;
class QGroupBox;
class QLabel;
class QStackedWidget;
class UsageMeter;
struct SystemUsage;

// The Performance tab, laid out like the Windows 7 Task Manager's: CPU and memory meters next
// to their usage history, with boxes of memory and system figures underneath.
class PerformancePage : public QWidget
{
  Q_OBJECT

public:
  explicit PerformancePage(QWidget *parent = nullptr);

  void addSample(const SystemUsage &usage);
  void setPerCoreGraphsVisible(bool visible);
  void setKernelTimesVisible(bool visible);

private:
  using StatValue = std::function<QString(const SystemUsage &)>;

  struct Stat
  {
    QLabel *label;
    StatValue value;
  };

  QGroupBox *createStatsBox(const QString &title, const QList<QPair<QString, StatValue>> &rows);

  UsageMeter *m_cpuMeter = nullptr;
  QStackedWidget *m_cpuHistoryStack = nullptr;
  HistoryGraph *m_cpuHistory = nullptr;  // all CPUs in one graph
  HistoryGraph *m_coreHistory = nullptr; // one graph per CPU
  UsageMeter *m_memoryMeter = nullptr;
  HistoryGraph *m_memoryHistory = nullptr;
  QList<Stat> m_stats;
};
