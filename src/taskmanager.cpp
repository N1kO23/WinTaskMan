#include "taskmanager.h"
#include <QAction>
#include <QChart>
#include <QChartView>
#include <QDebug>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineSeries>
#include <QMenuBar>
#include <QProcess>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
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
  viewMenu->addAction("Graphs summary view", this, []() {})->setCheckable(true);
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
  m_performanceChart = new QChart();
  m_performanceSeries = new QLineSeries();
  m_performanceChart->addSeries(m_performanceSeries);
  m_performanceChart->legend()->hide();
  m_performanceChart->setBackgroundBrush(QBrush(Qt::black));
  m_performanceChart->setTitleBrush(QBrush(Qt::white));
  m_performanceChart->setPlotAreaBackgroundVisible(true);
  m_performanceChart->setPlotAreaBackgroundBrush(QBrush(Qt::black));
  m_performanceSeries->setColor(Qt::green);

  QValueAxis *axisX = new QValueAxis();
  QValueAxis *axisY = new QValueAxis();
  axisX->setRange(0, 60);
  axisY->setRange(0, 100);
  axisX->setGridLinePen(QPen(Qt::darkGreen));
  axisY->setGridLinePen(QPen(Qt::darkGreen));

  m_performanceChart->addAxis(axisX, Qt::AlignBottom);
  m_performanceChart->addAxis(axisY, Qt::AlignLeft);
  m_performanceSeries->attachAxis(axisX);
  m_performanceSeries->attachAxis(axisY);
  m_performanceSeries->setPen(QPen(Qt::green, 2));

  QChartView *performanceChartView = new QChartView(m_performanceChart);
  QWidget *performanceTab = new QWidget(this);
  QVBoxLayout *performanceLayout = new QVBoxLayout(performanceTab);
  performanceLayout->addWidget(performanceChartView);
  performanceLayout->setContentsMargins(12, 12, 10, 10);
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
  if (!m_performanceSeries)
    return;

  while (m_performanceSeries->count() >= 60)
    m_performanceSeries->removePoints(0, 1);

  for (int i = 0; i < m_performanceSeries->count(); ++i)
  {
    QPointF point = m_performanceSeries->at(i);
    m_performanceSeries->replace(i, QPointF(point.x() - 1.0, point.y()));
  }

  m_performanceSeries->append(60, m_usage.cpuUsage);
  m_performanceChart->update();
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
    item->setText(4, locale.toString(process.memoryKb) + " K");
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
