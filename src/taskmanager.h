#pragma once

#include <QMainWindow>
#include <QMap>

class QChart;
class QLineSeries;
class QStatusBar;
class QTabWidget;
class QTreeWidget;
class QTimer;
class QTreeWidgetItem;

#include "systemdataprovider.h"

enum class UpdateSpeed
{
  High,
  Normal,
  Low,
  Paused
};

class TaskManager : public QMainWindow
{
  Q_OBJECT

public:
  explicit TaskManager(QWidget *parent = nullptr);

private:
  void createMenus();
  void createTabs();
  void createPerformanceChart();
  void refreshData();
  void updateActiveTab();
  void updateStatusBar();
  void updateGraphs();

  void updateApplications();
  void updateProcesses();
  void updateServices();

  void runNewTask();
  void refreshNow();
  void openHelp();
  void showAbout();
  void setUpdateSpeed(UpdateSpeed speed);

  void onTabChanged(int index);

private:
  SystemDataProvider m_dataProvider;
  SystemUsage m_usage;

  QTabWidget *m_tabWidget = nullptr;
  QStatusBar *m_statusBar = nullptr;
  QTimer *m_updateTimer = nullptr;
  QTreeWidget *m_applicationsTab = nullptr;
  QTreeWidget *m_processesTab = nullptr;
  QTreeWidget *m_servicesTab = nullptr;
  QChart *m_performanceChart = nullptr;
  QLineSeries *m_performanceSeries = nullptr;

  QMap<QString, QTreeWidgetItem *> m_appToItemMap;
  QMap<int, QTreeWidgetItem *> m_pidToItemMap;
  QMap<QString, QTreeWidgetItem *> m_serviceNameToItemMap;
  bool m_showAllProcesses = false;
};
