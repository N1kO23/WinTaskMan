#pragma once

#include "backgroundjob.h"

#include "system/procfs.h"
#include "system/trackers.h"

#include <QMainWindow>
#include <QTimer>

class PerformancePage;
class QLabel;
class QTabWidget;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);

private:
  void createTabs();
  void createMenus();
  void createStatusBar();

  void refresh();
  void refreshCurrentPage();
  void showUsage(const UsageSnapshot &snapshot);
  void setUpdateInterval(int intervalMs);
  void runNewTask();
  void showAbout();

  QTabWidget *m_tabs = nullptr;
  PerformancePage *m_performancePage = nullptr;
  QLabel *m_processCountLabel = nullptr;
  QLabel *m_cpuUsageLabel = nullptr;
  QLabel *m_memoryUsageLabel = nullptr;
  QTimer m_timer;
  BackgroundJob<UsageSnapshot> m_usageJob;
  SystemUsageTracker m_usageTracker;
};
