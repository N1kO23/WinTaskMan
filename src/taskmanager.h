#pragma once

#include <QMainWindow>
#include <QMap>
#include <QVector>

class QChart;
class QLineSeries;
class QStatusBar;
class QTabWidget;
class QTreeWidget;
class QTimer;
class QTreeWidgetItem;
class QChartView;
class QWidget;
class QGridLayout;
class QAction;
class QScrollArea;

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
  QChart *m_performanceChart = nullptr; // kept for compatibility but not used for per-graph charts
  QLineSeries *m_performanceSeries = nullptr;
  QLineSeries *m_memorySeries = nullptr;
  QVector<QLineSeries *> m_coreSeries;

  // New per-chart UI elements
  QChartView *m_cpuChartView = nullptr;
  QChartView *m_memoryChartView = nullptr;
  QVector<QChartView *> m_coreChartViews;
  QWidget *m_coreContainerWidget = nullptr;
  QGridLayout *m_coreGridLayout = nullptr;
  QScrollArea *m_coreScrollArea = nullptr;
  QAction *m_graphSummaryAction = nullptr;

  QMap<QString, QTreeWidgetItem *> m_appToItemMap;
  QMap<int, QTreeWidgetItem *> m_pidToItemMap;
  QMap<QString, QTreeWidgetItem *> m_serviceNameToItemMap;
  bool m_showAllProcesses = false;
};
