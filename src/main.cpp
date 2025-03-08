#include <QApplication>
#include <QIcon>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMenuBar>
#include <QProcess>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QMessageBox>

#include <QChart>
#include <QChartView>
#include <QDir>
#include <QFile>
#include <QLineSeries>
#include <QMap>
#include <QStringList>
#include <QSysInfo>
#include <QTextStream>
#include <QValueAxis>
#include <unistd.h>
#include <csignal>

#include "fetchfunctions.h"
#include "helperutils.h"

QMap<int, long> previousCpuTimes;
QMap<int, long> previousTotalTimes;

QMap<QString, QTreeWidgetItem *> appToItemMap;
QMap<int, QTreeWidgetItem *> pidToItemMap;
QMap<QString, QTreeWidgetItem *> serviceNameToItemMap;

enum class UpdateSpeed
{
  High,
  Normal,
  Low,
  Paused
};

class TaskManager : public QMainWindow
{
public:
  TaskManager(QWidget *parent = nullptr) : QMainWindow(parent)
  {
    setWindowTitle("Task Manager");
    setWindowIcon(QIcon(":/src/assets/icons/taskmgr.ico"));

    updateSystemUsage();

    // 🔹 Create menu bar
    createMenus();

    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);

    // 🔹 Create tab widget
    tabWidget = new QTabWidget(this);
    tabWidget->setContentsMargins(128, 128, 8, 8);
    // Connect the currentChanged signal to the custom slot
    connect(tabWidget, &QTabWidget::currentChanged, this, &TaskManager::onTabChanged);

    // 🔹 Applications Tab
    applicationsTab = new QTreeWidget(this);
    applicationsTab->setColumnCount(2);
    applicationsTab->setHeaderLabels({"Task", "Status"});
    applicationsTab->setRootIsDecorated(false);
    applicationsTab->setSortingEnabled(true);
    applicationsTab->setStyleSheet(R"(
      QTreeWidget { border: 1px solid gray; font-size: 11px; }
    )");
    tabWidget->addTab(applicationsTab, "Applications");

    // 🔹 Processes Tab
    QWidget *processesTabContainer = new QWidget(this);
    QVBoxLayout *processesLayout = new QVBoxLayout(processesTabContainer);

    // Process list tree
    processesTab = new QTreeWidget(this);
    processesTab->setColumnCount(5);
    processesTab->setHeaderLabels({"Name", "PID", "User", "CPU", "Working Set (Memory)"});
    processesTab->setRootIsDecorated(false);
    processesTab->setSortingEnabled(true);
    processesTab->setStyleSheet(R"(
      QTreeWidget { border: 1px solid gray; font-size: 11px; }
    )");

    // Layout for controls at the bottom
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    QCheckBox *toggleFilterButton = new QCheckBox("Show processes from all users", this);
    toggleFilterButton->setChecked(showAllProcesses);
    QPushButton *endProcessButton = new QPushButton("End Process", this);
    endProcessButton->setEnabled(false);

    controlsLayout->addWidget(toggleFilterButton);
    controlsLayout->addStretch(); // Pushes the button to the right
    controlsLayout->addWidget(endProcessButton);

    // Processes tab layout
    processesLayout->addWidget(processesTab);
    processesLayout->addLayout(controlsLayout);
    processesLayout->setContentsMargins(12, 12, 10, 10);
    processesLayout->setSpacing(5);
    processesTabContainer->setLayout(processesLayout);

    tabWidget->addTab(processesTabContainer, "Processes");

    // Connect "Show All Processes" checkbox
    connect(toggleFilterButton, &QCheckBox::toggled, this, [this](bool checked)
            {
              showAllProcesses = checked;
              updateProcesses(); });

    // Connect "End Process" button
    connect(processesTab, &QTreeWidget::itemSelectionChanged, this, [this, endProcessButton]()
            { endProcessButton->setEnabled(processesTab->selectedItems().count() > 0); });
    connect(endProcessButton, &QPushButton::clicked, this, [this]()
            {
              QTreeWidgetItem *selectedItem = processesTab->currentItem();
              int pid = selectedItem->text(1).toInt(); // Assuming PID is in column 1
              if (QMessageBox::question(this, "Confirm", "Are you sure you want to end this process?") == QMessageBox::Yes) {
                  kill(pid, SIGTERM); // Gracefully terminate process (use SIGKILL if force needed)
                  updateProcesses();
              }
              QMessageBox::warning(this, "No Selection", "Please select a process to end."); });

    // 🔹 Services Tab
    servicesTab = new QTreeWidget(this);
    servicesTab->setColumnCount(4);
    servicesTab->setHeaderLabels({"Name", "PID", "Description", "Status"});
    servicesTab->setRootIsDecorated(false);
    servicesTab->setSortingEnabled(true);
    servicesTab->setStyleSheet(R"(
      QTreeWidget { border: 1px solid gray; font-size: 11px; }
    )");
    tabWidget->addTab(servicesTab, "Services");

    // 🔹 Performance Tab with Graphs
    QVBoxLayout *performanceLayout = new QVBoxLayout();
    performanceChart = new QChart();
    performanceSeries = new QLineSeries();
    performanceChart->addSeries(performanceSeries);
    performanceChart->legend()->hide();

    // Customize XP style
    performanceChart->setBackgroundBrush(QBrush(Qt::black));
    performanceChart->setTitleBrush(QBrush(Qt::white));
    performanceChart->setPlotAreaBackgroundVisible(true);
    performanceChart->setPlotAreaBackgroundBrush(QBrush(Qt::black));
    performanceSeries->setColor(Qt::green);

    QValueAxis *axisX = new QValueAxis();
    QValueAxis *axisY = new QValueAxis();

    axisX->setRange(0, 60);
    axisY->setRange(0, 100);

    axisX->setGridLinePen(QPen(Qt::darkGreen));
    axisY->setGridLinePen(QPen(Qt::darkGreen));

    performanceChart->addAxis(axisX, Qt::AlignBottom);
    performanceChart->addAxis(axisY, Qt::AlignLeft);
    performanceSeries->attachAxis(axisX);
    performanceSeries->attachAxis(axisY);

    // Green XP-style line
    QPen pen(Qt::green);
    pen.setWidth(2);
    performanceSeries->setPen(pen);

    QChartView *performanceChartView = new QChartView(performanceChart);
    performanceLayout->addWidget(performanceChartView);

    QWidget *performanceTab = new QWidget();
    performanceTab->setLayout(performanceLayout);
    tabWidget->addTab(performanceTab, "Performance");

    // 🔹 Networking Tab (Placeholder)
    QWidget *networkingTab = new QWidget(this);
    tabWidget->addTab(networkingTab, "Networking");

    // 🔹 Users Tab (Placeholder)
    QWidget *usersTab = new QWidget(this);
    tabWidget->addTab(usersTab, "Users");

    tabWidget->setStyleSheet(R"(
      QTabWidget { border: 1px solid gray; font-size: 11px; }
    )");

    setCentralWidget(tabWidget);

    onTabChanged(tabWidget->currentIndex()); // Run the function for the initially active tab
    refreshDatas();

    // 🔹 Timer to refresh process & service data
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &TaskManager::refreshDatas);
    setUpdateSpeed(UpdateSpeed::Normal); // Start the timer
  }

private:
  QTabWidget *tabWidget;
  QStatusBar *statusBar;

  QTimer *updateTimer;

  QTreeWidget *applicationsTab;
  QTreeWidget *processesTab;
  QTreeWidget *servicesTab;

  QChart *performanceChart;
  QLineSeries *performanceSeries;
  QList<int> coreUsages;
  int coreCount = 0, cpuUsage = 0, ramUsage = 0, totalRam = 0, totalProcesses = 0;
  QString currentUser = getCurrentUser();

  bool showAllProcesses = false;

  // The main refresh function
  void refreshDatas()
  {
    updateSystemUsage();
    updateStatusBar();
    updateActiveTab();
    updateGraphs();
  }

  void onTabChanged(int index)
  {
    switch (index)
    {
    case 0: // Applications tab
      updateApplications();
      break;
    case 1: // Processes tab
      updateProcesses();
      break;
    case 2: // Services tab
      updateServices();
      break;
    default:
      break;
    }
  }

  void updateActiveTab()
  {
    int currentIndex = tabWidget->currentIndex();
    onTabChanged(currentIndex); // Run the function for the current active tab
  }

  void createMenus()
  {
    // Create menu bar
    QMenuBar *menuBar = new QMenuBar(this);

    // Create menus
    QMenu *fileMenu = menuBar->addMenu("File");
    QMenu *optionsMenu = menuBar->addMenu("Options");
    QMenu *viewMenu = menuBar->addMenu("View");
    QMenu *helpMenu = menuBar->addMenu("Help");

    // File Menu
    fileMenu->addAction("Run new task", this, [this]
                        { runNewTask(); }, QKeySequence("Ctrl+N"));
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QMainWindow::close);

    // Options Menu
    QAction *alwaysOnTop = optionsMenu->addAction("Always on top");
    alwaysOnTop->setCheckable(true);
    QAction *minimizeOnUse = optionsMenu->addAction("Minimize on use");
    minimizeOnUse->setCheckable(true);

    // View Menu
    viewMenu->addAction("Refresh now", this, [this]
                        { refreshNow(); }, QKeySequence("F5"));

    QMenu *updateSpeedMenu = viewMenu->addMenu("Update speed");
    updateSpeedMenu->addAction("High", this, [this]
                               { setUpdateSpeed(UpdateSpeed::High); });
    updateSpeedMenu->addAction("Normal", this, [this]
                               { setUpdateSpeed(UpdateSpeed::Normal); });
    updateSpeedMenu->addAction("Low", this, [this]
                               { setUpdateSpeed(UpdateSpeed::Low); });
    updateSpeedMenu->addAction("Paused", this, [this]
                               { setUpdateSpeed(UpdateSpeed::Paused); });

    QAction *graphsSummary = viewMenu->addAction("Graphs summary view");
    graphsSummary->setCheckable(true);
    QAction *showHistory = viewMenu->addAction("Show history for all processes");
    showHistory->setCheckable(true);

    // Help Menu
    helpMenu->addAction("Help topics", this, [this]
                        { openHelp(); });
    helpMenu->addSeparator();
    helpMenu->addAction("About Task Manager", this, [this]
                        { showAbout(); });

    // Set menu bar
    setMenuBar(menuBar);
  }

  void setUpdateSpeed(UpdateSpeed speed)
  {
    int interval;

    switch (speed)
    {
    case UpdateSpeed::High:
      interval = 500; // Update every 500ms
      qDebug() << "Setting update speed to High (500ms)";
      break;
    case UpdateSpeed::Normal:
      interval = 1000; // Update every 1s
      qDebug() << "Setting update speed to Normal (1000ms)";
      break;
    case UpdateSpeed::Low:
      interval = 2000; // Update every 2s
      qDebug() << "Setting update speed to Low (2000ms)";
      break;
    case UpdateSpeed::Paused:
      updateTimer->stop(); // Stop the timer
      qDebug() << "Updates paused";
      return;
    }

    // Restart timer with the new interval
    updateTimer->start(interval);
  }

  void updateSystemUsage()
  {
    getSystemUsage(cpuUsage, ramUsage, totalRam, coreCount, coreUsages);
    qDebug() << "CPU Core usages:" << coreUsages << "Total CPU Usage:" << cpuUsage << "RAM Usage:" << ramUsage << "Total RAM:" << totalRam;
    totalProcesses = getTotalProcesses();
  }

  void updateStatusBar()
  {
    QString statusText =
        QString("Processes: %1 | CPU Usage: %2% | Physical Memory: %3%")
            .arg(totalProcesses)
            .arg(cpuUsage)
            .arg(ramUsage / totalRam * 100);
    statusBar->showMessage(statusText);
  }

  void updateGraphs()
  {
    if (!performanceSeries)
    {
      qDebug() << "Error: performanceSeries is NULL!";
      return;
    }

    // Shift all points left
    for (int i = 0; i < performanceSeries->count(); i++)
    {
      QPointF point = performanceSeries->at(i);
      performanceSeries->replace(i, QPointF(point.x() - 1, point.y()));
    }

    if (performanceSeries->count() >= 60)
    {
      performanceSeries->remove(0);
    }

    performanceSeries->append(60, cpuUsage);

    performanceChart->update();
  }

  void updateApplications()
  {
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    process.setProcessEnvironment(env);
    process.start("bash", QStringList() << "-c" << R"(
          if [ "$XDG_SESSION_TYPE" = "x11" ]; then
              xlsclients | awk '{print $2}' | sort -u
          elif [ "$XDG_SESSION_TYPE" = "wayland" ]; then
              x_apps=$(xlsclients 2>/dev/null | awk '{print $2}' | sort -u)
              w_apps=$(ps -eo pid,comm,args | grep -E 'wayland|Xwayland' | awk '{print $2}' | sort -u)
              echo -e "$x_apps\n$w_apps" | sort -u
          else
              echo "Unknown display server"
          fi
      )");
    process.waitForFinished();

    QString output = process.readAllStandardOutput();
    QStringList appList = output.split("\n", Qt::SkipEmptyParts);

    // Mark all items as unused
    for (auto it = appToItemMap.begin(); it != appToItemMap.end(); ++it)
    {
      it.value()->setData(0, Qt::UserRole, false);
    }

    for (const QString &app : appList)
    {
      QString status = "Running"; // Hardcode this for now

      QTreeWidgetItem *item;
      if (appToItemMap.contains(app))
      {
        // Update existing item
        item = appToItemMap[app];
      }
      else
      {
        // Create new item
        item = new QTreeWidgetItem(applicationsTab);
        appToItemMap[app] = item;
        applicationsTab->addTopLevelItem(item);
      }

      item->setText(0, app);
      item->setText(1, status);
      item->setData(0, Qt::UserRole, true); // Mark as used
    }

    // Remove leftover items
    for (auto it = appToItemMap.begin(); it != appToItemMap.end();)
    {
      if (!it.value()->data(0, Qt::UserRole).toBool())
      {
        delete it.value();
        it = appToItemMap.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  void updateProcesses()
  {
    QDir procDir("/proc");
    QFileInfoList procEntries = procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    long pageSizeKb = sysconf(_SC_PAGESIZE) / 1024; // Get page size in kilobytes
    long ticksPerSec = sysconf(_SC_CLK_TCK);        // Get clock ticks per second

    // Read system uptime
    QFile uptimeFile("/proc/uptime");
    if (!uptimeFile.open(QIODevice::ReadOnly))
    {
      qDebug() << "Failed to open /proc/uptime";
      return;
    }
    QTextStream uptimeStream(&uptimeFile);
    double uptime;
    uptimeStream >> uptime;
    uptimeFile.close();

    // Get the number of CPU cores
    int numCores = sysconf(_SC_NPROCESSORS_ONLN);

    // Mark all items as unused
    for (auto it = pidToItemMap.begin(); it != pidToItemMap.end(); ++it)
    {
      it.value()->setData(0, Qt::UserRole, false);
    }

    foreach (const QFileInfo &entry, procEntries)
    {
      if (!entry.isDir() || !entry.fileName().toInt())
        continue;

      QString pidStr = entry.fileName();
      int pid = pidStr.toInt();
      QFile statFile(entry.filePath() + "/stat");
      QFile cmdlineFile(entry.filePath() + "/cmdline");
      QFile statusFile(entry.filePath() + "/status");

      if (!statFile.open(QIODevice::ReadOnly) ||
          !cmdlineFile.open(QIODevice::ReadOnly) ||
          !statusFile.open(QIODevice::ReadOnly))
      {
        qDebug() << "Failed to open stat, cmdline, or status file for PID" << pid;
        continue;
      }

      QTextStream statStream(&statFile);
      QTextStream cmdlineStream(&cmdlineFile);
      QTextStream statusStream(&statusFile);

      QString stat = statStream.readAll();
      QString cmdline = cmdlineStream.readAll();
      QString status = statusStream.readAll();

      QStringList lines = status.split('\n');
      uid_t uid;
      for (const QString &line : lines)
      {
        if (line.startsWith("Uid:"))
        {
          QString uidLine =
              line.mid(4).trimmed();            // Remove "Uid:" and trim whitespace
          int tabIndex = uidLine.indexOf('\t'); // Find the first tab
          if (tabIndex != -1)
          {
            QString uidStr =
                uidLine.left(tabIndex).trimmed(); // Extract the first UID
            bool ok;
            uid = uidStr.toInt(&ok); // Convert to integer
            if (!ok)
            {
              qDebug() << "Error converting UID to int";
            }
          }
          else
          {
            bool ok;
            uid = uidLine.toInt(&ok);
            if (!ok)
            {
              qDebug() << "Error converting UID to int";
            }
          }
          break; // found so stop wasting cycles
        }
      }
      QString user = getUserFromUid(uid);

      // Check if we only want to show current user processes
      if (showAllProcesses || user == currentUser)
      {

        QStringList statParts = stat.split(" ");
        if (statParts.size() < 24)
        {
          qDebug() << "Invalid stat file format for PID" << pid;
          continue;
        }

        QString comm = cmdline.split('\0').join(' ');

        long utime = statParts[13].toLong(); // User mode time
        long stime = statParts[14].toLong(); // Kernel mode time
        long totalCpuTime = utime + stime;
        long starttime = statParts[21].toLong(); // Process start time

        double totalTime = uptime - (double)starttime / ticksPerSec;

        double cpuUsage = 0.0;
        if (previousCpuTimes.contains(pid) && previousTotalTimes.contains(pid))
        {
          long prevCpuTime = previousCpuTimes[pid];
          double prevTotalTime = previousTotalTimes[pid];

          // Correct the CPU usage calculation
          cpuUsage = ((double)(totalCpuTime - prevCpuTime) / ticksPerSec) /
                     (totalTime - prevTotalTime) * 100.0 / numCores;
        }

        previousCpuTimes[pid] = totalCpuTime;
        previousTotalTimes[pid] = totalTime;

        long rss = statParts[23].toLong();  // RSS in pages
        double memUsage = rss * pageSizeKb; // Convert to megabytes

        QTreeWidgetItem *item;
        if (pidToItemMap.contains(pid))
        {
          // Update existing item
          item = pidToItemMap[pid];
        }
        else
        {
          // Create new item
          item = new QTreeWidgetItem(processesTab);
          pidToItemMap[pid] = item;
          processesTab->addTopLevelItem(item);
        }

        item->setText(0, comm);                        // Name
        item->setData(1, Qt::DisplayRole, pid);        // PID as integer
        item->setData(1, Qt::UserRole, QVariant(pid)); // Store PID as number for sorting
        item->setText(2, user);                        // User

        // Format CPU usage as 2 whole digits
        int cpuUsageInt = static_cast<int>(cpuUsage);                                               // Convert to integer
        item->setData(3, Qt::DisplayRole, QString::number(cpuUsageInt, 10).rightJustified(2, '0')); // CPU Usage
        item->setData(3, Qt::UserRole, cpuUsage);                                                   // Store CPU as double for sorting
        item->setTextAlignment(3, Qt::AlignCenter);

        // Format memory usage with commas
        QLocale locale = QLocale::system();
        QString formattedMemory = locale.toString(memUsage);
        item->setData(4, Qt::DisplayRole, formattedMemory + " K"); // Memory in kilobytes
        item->setData(4, Qt::UserRole, memUsage);                  // Store memory as double for sorting
        item->setTextAlignment(4, Qt::AlignRight);

        item->setData(0, Qt::UserRole, true); // Mark as used
      }
    }

    // Remove leftover items
    for (auto it = pidToItemMap.begin(); it != pidToItemMap.end();)
    {
      if (!it.value()->data(0, Qt::UserRole).toBool())
      {
        delete it.value();
        it = pidToItemMap.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  void updateServices()
  {
    QProcess process;
    process.setProgram("systemctl");
    process.setArguments({"--user", "list-units", "--type=service", "--all", "--output=json"});
    process.start();
    process.waitForFinished();

    QByteArray output = process.readAllStandardOutput();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(output);
    if (!jsonDoc.isArray())
    {
      qWarning() << "Failed to parse JSON!";
      return;
    }

    // Mark all items as unused
    for (auto it = serviceNameToItemMap.begin(); it != serviceNameToItemMap.end(); ++it)
    {
      it.value()->setData(0, Qt::UserRole, false);
    }

    QJsonArray services = jsonDoc.array();
    for (const QJsonValue &serviceValue : services)
    {
      QJsonObject service = serviceValue.toObject();
      QString name = service["unit"].toString();
      QString pid = service.contains("mainPID") ? QString::number(service["mainPID"].toInt()) : "-";
      QString description = service["description"].toString();
      QString activeState = service["active"].toString();

      QTreeWidgetItem *item;
      if (serviceNameToItemMap.contains(name))
      {
        // Update existing item
        item = serviceNameToItemMap[name];
      }
      else
      {
        // Create new item
        item = new QTreeWidgetItem(servicesTab);
        serviceNameToItemMap[name] = item;
        servicesTab->addTopLevelItem(item);
      }

      item->setText(0, name);
      item->setText(1, pid);
      item->setText(2, description);
      item->setText(3, activeState);
      item->setData(0, Qt::UserRole, true); // Mark as used
    }

    // Remove leftover items
    for (auto it = serviceNameToItemMap.begin(); it != serviceNameToItemMap.end();)
    {
      if (!it.value()->data(0, Qt::UserRole).toBool())
      {
        delete it.value();
        it = serviceNameToItemMap.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  // 🔹 Implement the menu functions
  void runNewTask()
  {
    qDebug() << "Run new task clicked";
  }

  void refreshNow()
  {
    qDebug() << "Refresh now clicked";
    refreshDatas();
  }

  void openHelp()
  {
    qDebug() << "Help topics opened";
  }

  void showAbout()
  {
    qDebug() << "About Task Manager clicked";
  }
};

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  TaskManager taskManager;
  taskManager.show();
  return app.exec();
}