#pragma once

#include "backgroundjob.h"

#include "system/procfs.h"
#include "system/trackers.h"

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QTimer>

class ApplicationsPage;
class NetworkingPage;
class PerformancePage;
class ProcessesPage;
class QAction;
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
  void updateTabMenus();
  void setAlwaysOnTop(bool onTop);

  void refresh();
  void refreshCurrentPage();
  void showUsage(const UsageSnapshot &snapshot);
  void setUpdateInterval(int intervalMs);
  void runNewTask();
  void showAbout();

  QTabWidget *m_tabs = nullptr;
  ApplicationsPage *m_applicationsPage = nullptr;
  ProcessesPage *m_processesPage = nullptr;
  PerformancePage *m_performancePage = nullptr;
  NetworkingPage *m_networkingPage = nullptr;
  QHash<QWidget *, QList<QAction *>> m_tabActions; // menu items shown only while that tab is open
  bool m_networkingAlwaysActive = false;
  QLabel *m_processCountLabel = nullptr;
  QLabel *m_cpuUsageLabel = nullptr;
  QLabel *m_memoryUsageLabel = nullptr;
  QTimer m_timer;
  BackgroundJob<UsageSnapshot> m_usageJob;
  SystemUsageTracker m_usageTracker;
};
