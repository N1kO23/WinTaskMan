#include "taskmanager.h"
#include <QAction>
#include <QChart>
#include <QChartView>
#include <QDebug>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineSeries>
#include <QColor>
#include <QAbstractAxis>
#include <QMenuBar>
#include <QProcess>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QValueAxis>
#include <QCheckBox>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QLocale>
#include <QPointF>
#include <unistd.h>
#include <signal.h>

TaskManager::TaskManager(QWidget *parent)
    : QMainWindow(parent)
{
  setWindowTitle("Task Manager");
  setWindowIcon(QIcon(":/src/assets/icons/taskmgr.ico"));

  createMenus();
  createTabs();
  createPerformanceChart();

  m_statusBar = new QStatusBar(this);
  setStatusBar(m_statusBar);

  setCentralWidget(m_tabWidget);

  connect(m_tabWidget, &QTabWidget::currentChanged, this, &TaskManager::onTabChanged);

  m_updateTimer = new QTimer(this);
  connect(m_updateTimer, &QTimer::timeout, this, &TaskManager::refreshData);
  setUpdateSpeed(UpdateSpeed::Normal);

  refreshData();
}

void TaskManager::createMenus()
{
  QMenuBar *menuBar = new QMenuBar(this);

  QMenu *fileMenu = menuBar->addMenu("File");
  QMenu *viewMenu = menuBar->addMenu("View");
  QMenu *helpMenu = menuBar->addMenu("Help");

  fileMenu->addAction("Run new task", QKeySequence("Ctrl+N"), this, &TaskManager::runNewTask);
  fileMenu->addSeparator();
  fileMenu->addAction("Exit", this, &QMainWindow::close);

  viewMenu->addAction("Refresh now", QKeySequence("F5"), this, &TaskManager::refreshNow);
  QMenu *updateSpeedMenu = viewMenu->addMenu("Update speed");
  updateSpeedMenu->addAction("High", this, [this]()
                             { setUpdateSpeed(UpdateSpeed::High); });
  updateSpeedMenu->addAction("Normal", this, [this]()
                             { setUpdateSpeed(UpdateSpeed::Normal); });
  updateSpeedMenu->addAction("Low", this, [this]()
                             { setUpdateSpeed(UpdateSpeed::Low); });
  updateSpeedMenu->addAction("Paused", this, [this]()
                             { setUpdateSpeed(UpdateSpeed::Paused); });

  viewMenu->addSeparator();
  QAction *individualCore = viewMenu->addAction("Individual core usage");
  individualCore->setCheckable(true);
  m_graphSummaryAction = individualCore;
  connect(m_graphSummaryAction, &QAction::toggled, this, [this](bool checked)
          {
            if (m_coreScrollArea)
              m_coreScrollArea->setVisible(checked);
            if (m_cpuChartView)
              m_cpuChartView->setVisible(!checked); });
  viewMenu->addAction("Show history for all processes", this, []() {})->setCheckable(true);

  helpMenu->addAction("Help topics", this, &TaskManager::openHelp);
  helpMenu->addSeparator();
  helpMenu->addAction("About Task Manager", this, &TaskManager::showAbout);

  setMenuBar(menuBar);
}

void TaskManager::createTabs()
{
  m_tabWidget = new QTabWidget(this);
  m_tabWidget->setContentsMargins(12, 12, 12, 12);

  m_applicationsTab = new QTreeWidget(this);
  m_applicationsTab->setColumnCount(2);
  m_applicationsTab->setHeaderLabels({"Task", "Status"});
  m_applicationsTab->setRootIsDecorated(false);
  m_applicationsTab->setSortingEnabled(true);
  m_applicationsTab->setStyleSheet("QTreeWidget { border: 1px solid gray; font-size: 11px; }");
  m_tabWidget->addTab(m_applicationsTab, "Applications");

  QWidget *processesTabContainer = new QWidget(this);
  QVBoxLayout *processesLayout = new QVBoxLayout(processesTabContainer);
  processesLayout->setContentsMargins(12, 12, 10, 10);
  processesLayout->setSpacing(5);

  m_processesTab = new QTreeWidget(this);
  m_processesTab->setColumnCount(5);
  m_processesTab->setHeaderLabels({"Name", "PID", "User", "CPU", "Working Set (Memory)"});
  m_processesTab->setRootIsDecorated(false);
  m_processesTab->setSortingEnabled(true);
  m_processesTab->setStyleSheet("QTreeWidget { border: 1px solid gray; font-size: 11px; }");

  QHBoxLayout *controlsLayout = new QHBoxLayout();
  QCheckBox *toggleFilterButton = new QCheckBox("Show processes from all users", this);
  toggleFilterButton->setChecked(m_showAllProcesses);
  QPushButton *endProcessButton = new QPushButton("End Process", this);
  endProcessButton->setEnabled(false);

  controlsLayout->addWidget(toggleFilterButton);
  controlsLayout->addStretch();
  controlsLayout->addWidget(endProcessButton);

  processesLayout->addWidget(m_processesTab);
  processesLayout->addLayout(controlsLayout);
  processesTabContainer->setLayout(processesLayout);
  m_tabWidget->addTab(processesTabContainer, "Processes");

  connect(toggleFilterButton, &QCheckBox::toggled, this, [this](bool checked)
          {
        m_showAllProcesses = checked;
        updateProcesses(); });

  connect(m_processesTab, &QTreeWidget::itemSelectionChanged, this, [this, endProcessButton]()
          { endProcessButton->setEnabled(!m_processesTab->selectedItems().isEmpty()); });

  connect(endProcessButton, &QPushButton::clicked, this, [this]()
          {
        QTreeWidgetItem *selectedItem = m_processesTab->currentItem();
        if (!selectedItem)
        {
            QMessageBox::warning(this, "No Selection", "Please select a process to end.");
            return;
        }

        const int pid = selectedItem->data(1, Qt::UserRole).toInt();
        if (QMessageBox::question(this, "Confirm", "Are you sure you want to end this process?") == QMessageBox::Yes)
        {
            kill(pid, SIGTERM);
            updateProcesses();
        } });

  m_servicesTab = new QTreeWidget(this);
  m_servicesTab->setColumnCount(4);
  m_servicesTab->setHeaderLabels({"Name", "PID", "Description", "Status"});
  m_servicesTab->setRootIsDecorated(false);
  m_servicesTab->setSortingEnabled(true);
  m_servicesTab->setStyleSheet("QTreeWidget { border: 1px solid gray; font-size: 11px; }");
  m_tabWidget->addTab(m_servicesTab, "Services");

  QWidget *networkingTab = new QWidget(this);
  m_tabWidget->addTab(networkingTab, "Networking");

  QWidget *usersTab = new QWidget(this);
  m_tabWidget->addTab(usersTab, "Users");
}

void TaskManager::createPerformanceChart()
{
  // Create CPU chart (total)
  m_performanceSeries = new QLineSeries();
  QChart *cpuChart = new QChart();
  cpuChart->addSeries(m_performanceSeries);
  cpuChart->legend()->hide();
  cpuChart->setBackgroundBrush(QBrush(Qt::black));
  cpuChart->setPlotAreaBackgroundVisible(true);
  cpuChart->setPlotAreaBackgroundBrush(QBrush(Qt::black));
  m_performanceSeries->setName("CPU %");
  m_performanceSeries->setPen(QPen(Qt::green, 2));
  QValueAxis *cpuAxisX = new QValueAxis();
  QValueAxis *cpuAxisY = new QValueAxis();
  cpuAxisX->setRange(0, 60);
  cpuAxisY->setRange(0, 100);
  cpuAxisX->setGridLinePen(QPen(Qt::darkGreen));
  cpuAxisY->setGridLinePen(QPen(Qt::darkGreen));
  cpuChart->addAxis(cpuAxisX, Qt::AlignBottom);
  cpuChart->addAxis(cpuAxisY, Qt::AlignLeft);
  m_performanceSeries->attachAxis(cpuAxisX);
  m_performanceSeries->attachAxis(cpuAxisY);
  m_cpuChartView = new QChartView(cpuChart);

  // Create Memory chart
  m_memorySeries = new QLineSeries();
  QChart *memChart = new QChart();
  memChart->addSeries(m_memorySeries);
  memChart->legend()->hide();
  memChart->setBackgroundBrush(QBrush(Qt::black));
  memChart->setPlotAreaBackgroundVisible(true);
  memChart->setPlotAreaBackgroundBrush(QBrush(Qt::black));
  m_memorySeries->setName("Memory %");
  m_memorySeries->setPen(QPen(Qt::blue, 2));
  QValueAxis *memAxisX = new QValueAxis();
  QValueAxis *memAxisY = new QValueAxis();
  memAxisX->setRange(0, 60);
  memAxisY->setRange(0, 100);
  memAxisX->setGridLinePen(QPen(Qt::darkBlue));
  memAxisY->setGridLinePen(QPen(Qt::darkBlue));
  memChart->addAxis(memAxisX, Qt::AlignBottom);
  memChart->addAxis(memAxisY, Qt::AlignLeft);
  m_memorySeries->attachAxis(memAxisX);
  m_memorySeries->attachAxis(memAxisY);
  m_memoryChartView = new QChartView(memChart);

  // Container for per-core charts
  m_coreContainerWidget = new QWidget();
  m_coreGridLayout = new QGridLayout(m_coreContainerWidget);
  m_coreGridLayout->setSpacing(6);
  m_coreGridLayout->setContentsMargins(0, 0, 0, 0);

  m_coreScrollArea = new QScrollArea();
  m_coreScrollArea->setWidgetResizable(true);
  m_coreScrollArea->setWidget(m_coreContainerWidget);

  // Compose performance tab
  QWidget *performanceTab = new QWidget(this);
  QVBoxLayout *performanceLayout = new QVBoxLayout(performanceTab);
  performanceLayout->setContentsMargins(12, 12, 10, 10);
  performanceLayout->setSpacing(8);
  performanceLayout->addWidget(m_cpuChartView);
  performanceLayout->addWidget(m_coreScrollArea);
  performanceLayout->addWidget(m_memoryChartView);
  // hide per-core charts by default; summary (memory) remains visible
  if (m_coreScrollArea)
    m_coreScrollArea->setVisible(false);
  performanceTab->setLayout(performanceLayout);
  m_tabWidget->addTab(performanceTab, "Performance");
}

void TaskManager::refreshData()
{
  m_usage = m_dataProvider.refreshSystemUsage();
  updateStatusBar();
  updateActiveTab();
  updateGraphs();
}

void TaskManager::onTabChanged(int index)
{
  switch (index)
  {
  case 0:
    updateApplications();
    break;
  case 1:
    updateProcesses();
    break;
  case 2:
    updateServices();
    break;
  default:
    break;
  }
}

void TaskManager::updateActiveTab()
{
  onTabChanged(m_tabWidget->currentIndex());
}

void TaskManager::updateStatusBar()
{
  const double memoryPercent = m_usage.totalRam > 0 ? (m_usage.ramUsage * 100.0) / m_usage.totalRam : 0.0;
  const QString statusText = QString("Processes: %1 | CPU Usage: %2% | Physical Memory: %3%")
                                 .arg(m_usage.totalProcesses)
                                 .arg(m_usage.cpuUsage)
                                 .arg(QString::number(memoryPercent, 'f', 1));
  m_statusBar->showMessage(statusText);
}

void TaskManager::updateGraphs()
{
  if (!m_performanceSeries || !m_memorySeries || !m_cpuChartView || !m_memoryChartView)
    return;

  const int coreCount = m_usage.coreCount;

  // create or remove per-core chart widgets/series as needed
  while (m_coreSeries.size() < coreCount)
  {
    const int idx = m_coreSeries.size();
    QLineSeries *series = new QLineSeries();
    series->setName(QString("Core %1").arg(idx));
    QColor c = QColor::fromHsv((idx * 40) % 360, 200, 200);
    series->setPen(QPen(c, 1));

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->legend()->hide();
    chart->setBackgroundBrush(QBrush(Qt::black));
    QValueAxis *axisX = new QValueAxis();
    QValueAxis *axisY = new QValueAxis();
    axisX->setRange(0, 60);
    axisY->setRange(0, 100);
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    QChartView *view = new QChartView(chart);
    view->setMinimumHeight(80);

    const int cols = 4;
    const int row = idx / cols;
    const int col = idx % cols;
    m_coreGridLayout->addWidget(view, row, col);

    m_coreSeries.append(series);
    m_coreChartViews.append(view);
  }

  while (m_coreSeries.size() > coreCount)
  {
    QLineSeries *s = m_coreSeries.takeLast();
    QChartView *v = m_coreChartViews.takeLast();
    m_coreGridLayout->removeWidget(v);
    delete v;
    delete s;
  }

  auto shiftAndTrim = [](QLineSeries *series)
  {
    while (series->count() >= 60)
      series->removePoints(0, 1);
    for (int i = 0; i < series->count(); ++i)
    {
      QPointF point = series->at(i);
      series->replace(i, QPointF(point.x() - 1.0, point.y()));
    }
  };

  // Shift existing points
  shiftAndTrim(m_performanceSeries);
  shiftAndTrim(m_memorySeries);
  for (QLineSeries *coreSeries : m_coreSeries)
    shiftAndTrim(coreSeries);

  // Append new values at right edge (60)
  m_performanceSeries->append(60, m_usage.cpuUsage);
  const double memoryPercent = m_usage.totalRam > 0 ? (m_usage.ramUsage * 100.0) / m_usage.totalRam : 0.0;
  m_memorySeries->append(60, memoryPercent);

  for (int i = 0; i < m_coreSeries.size(); ++i)
  {
    int val = 0;
    if (i < m_usage.coreUsages.size())
      val = m_usage.coreUsages[i];
    m_coreSeries[i]->append(60, val);
  }

  // show/hide core area and CPU summary depending on 'Individual core usage' toggle
  if (m_graphSummaryAction && m_coreScrollArea && m_cpuChartView)
  {
    const bool showIndividual = m_graphSummaryAction->isChecked();
    m_coreScrollArea->setVisible(showIndividual);
    m_cpuChartView->setVisible(!showIndividual);
  }

  // refresh views
  m_cpuChartView->chart()->update();
  m_memoryChartView->chart()->update();
  for (QChartView *v : m_coreChartViews)
    v->chart()->update();
}

void TaskManager::updateApplications()
{
  const QStringList applications = m_dataProvider.refreshApplications();

  for (auto it = m_appToItemMap.begin(); it != m_appToItemMap.end(); ++it)
    it.value()->setData(0, Qt::UserRole, false);

  for (const QString &app : applications)
  {
    QTreeWidgetItem *item = nullptr;
    if (m_appToItemMap.contains(app))
    {
      item = m_appToItemMap[app];
    }
    else
    {
      item = new QTreeWidgetItem(m_applicationsTab);
      m_appToItemMap.insert(app, item);
    }

    item->setText(0, app);
    item->setText(1, "Running");
    item->setData(0, Qt::UserRole, true);
  }

  for (auto it = m_appToItemMap.begin(); it != m_appToItemMap.end();)
  {
    if (!it.value()->data(0, Qt::UserRole).toBool())
    {
      delete it.value();
      it = m_appToItemMap.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

void TaskManager::updateProcesses()
{
  const QList<ProcessInfo> processes = m_dataProvider.refreshProcessList(m_showAllProcesses);

  for (auto it = m_pidToItemMap.begin(); it != m_pidToItemMap.end(); ++it)
    it.value()->setData(0, Qt::UserRole, false);

  for (const ProcessInfo &process : processes)
  {
    QTreeWidgetItem *item = nullptr;
    if (m_pidToItemMap.contains(process.pid))
    {
      item = m_pidToItemMap[process.pid];
    }
    else
    {
      item = new QTreeWidgetItem(m_processesTab);
      m_pidToItemMap.insert(process.pid, item);
    }

    item->setText(0, process.name);
    item->setData(1, Qt::DisplayRole, process.pid);
    item->setData(1, Qt::UserRole, process.pid);
    item->setText(2, process.user);
    item->setText(3, QString::number(process.cpuPercent, 'f', 1));
    item->setTextAlignment(3, Qt::AlignCenter);

    const QLocale locale = QLocale::system();
    item->setText(4, locale.toString(process.memoryKb, 'f', 0) + " K");
    item->setData(4, Qt::UserRole, process.memoryKb);
    item->setTextAlignment(4, Qt::AlignRight);
    item->setData(0, Qt::UserRole, true);
  }

  for (auto it = m_pidToItemMap.begin(); it != m_pidToItemMap.end();)
  {
    if (!it.value()->data(0, Qt::UserRole).toBool())
    {
      delete it.value();
      it = m_pidToItemMap.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

void TaskManager::updateServices()
{
  const QList<ServiceInfo> services = m_dataProvider.refreshServices();

  for (auto it = m_serviceNameToItemMap.begin(); it != m_serviceNameToItemMap.end(); ++it)
    it.value()->setData(0, Qt::UserRole, false);

  for (const ServiceInfo &service : services)
  {
    QTreeWidgetItem *item = nullptr;
    if (m_serviceNameToItemMap.contains(service.name))
    {
      item = m_serviceNameToItemMap[service.name];
    }
    else
    {
      item = new QTreeWidgetItem(m_servicesTab);
      m_serviceNameToItemMap.insert(service.name, item);
    }

    item->setText(0, service.name);
    item->setText(1, service.pid);
    item->setText(2, service.description);
    item->setText(3, service.state);
    item->setData(0, Qt::UserRole, true);
  }

  for (auto it = m_serviceNameToItemMap.begin(); it != m_serviceNameToItemMap.end();)
  {
    if (!it.value()->data(0, Qt::UserRole).toBool())
    {
      delete it.value();
      it = m_serviceNameToItemMap.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

void TaskManager::runNewTask()
{
  qDebug() << "Run new task clicked";
}

void TaskManager::refreshNow()
{
  qDebug() << "Refresh now clicked";
  refreshData();
}

void TaskManager::openHelp()
{
  qDebug() << "Help topics opened";
}

void TaskManager::showAbout()
{
  qDebug() << "About Task Manager clicked";
}

void TaskManager::setUpdateSpeed(UpdateSpeed speed)
{
  if (!m_updateTimer)
    return;

  switch (speed)
  {
  case UpdateSpeed::High:
    m_updateTimer->start(500);
    break;
  case UpdateSpeed::Normal:
    m_updateTimer->start(1000);
    break;
  case UpdateSpeed::Low:
    m_updateTimer->start(2000);
    break;
  case UpdateSpeed::Paused:
    m_updateTimer->stop();
    break;
  }
}
