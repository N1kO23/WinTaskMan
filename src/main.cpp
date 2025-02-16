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

class TaskManager : public QMainWindow
{
public:
  TaskManager(QWidget *parent = nullptr) : QMainWindow(parent)
  {
    setWindowTitle("Task Manager");
    setWindowIcon(QIcon(":/src/assets/icons/taskmgr.ico"));

    updateSystemUsage();

    // 🔹 Create menu bar
    QMenuBar *menuBar = new QMenuBar(this);
    QMenu *fileMenu = menuBar->addMenu("File");
    QMenu *optionsMenu = menuBar->addMenu("Options");
    QMenu *viewMenu = menuBar->addMenu("View");
    QMenu *helpMenu = menuBar->addMenu("Help");
    setMenuBar(menuBar);

    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);

    // 🔹 Create tab widget
    QTabWidget *tabWidget = new QTabWidget(this);
    tabWidget->setContentsMargins(128, 128, 8, 8);

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

    // 🔹 Timer to refresh process & service data
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TaskManager::updateSystemUsage);
    connect(timer, &QTimer::timeout, this, &TaskManager::updateStatusBar);
    connect(timer, &QTimer::timeout, this, &TaskManager::updateApplications);
    connect(timer, &QTimer::timeout, this, &TaskManager::updateProcesses);
    connect(timer, &QTimer::timeout, this, &TaskManager::updateServices);
    connect(timer, &QTimer::timeout, this, &TaskManager::updateGraphs);
    timer->start(1000);

    updateSystemUsage();
    updateStatusBar();
    updateApplications();
    updateProcesses();
    updateServices();
    updateGraphs();
  }

private:
  QStatusBar *statusBar;

  QTreeWidget *applicationsTab;
  QTreeWidget *processesTab;
  QTreeWidget *servicesTab;

  QChart *performanceChart;
  QLineSeries *performanceSeries;
  QList<int> coreUsages;
  int coreCount = 0, cpuUsage = 0, ramUsage = 0, totalRam = 0, totalProcesses = 0;
  QString currentUser = getCurrentUser();

  bool showAllProcesses = false;

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

    applicationsTab->clear(); // Clear previous entries

    for (const QString &app : appList)
    {
      QString status = "Running"; // Hardcode this for now
      QTreeWidgetItem *item = new QTreeWidgetItem(applicationsTab);
      item->setText(0, app);
      item->setText(1, status);
      applicationsTab->addTopLevelItem(item);
    }
  }

  void updateProcesses()
  {
    processesTab->clear();
    QDir procDir("/proc");
    QFileInfoList procEntries =
        procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    long pageSizeKb =
        sysconf(_SC_PAGESIZE) / 1024;        // Get page size in kilobytes
    long ticksPerSec = sysconf(_SC_CLK_TCK); // Get clock ticks per second

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
        qDebug() << "Failed to open stat, cmdline, or status file for PID"
                 << pid;
        continue;
      }

      QTextStream statStream(&statFile);
      QTextStream cmdlineStream(&cmdlineFile);
      QTextStream statusStream(&statusFile);

      QString stat = statStream.readAll();
      QString cmdline = cmdlineStream.readAll();
      QString status = statusStream.readAll();

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

      if (showAllProcesses || user == currentUser)
      {
        QTreeWidgetItem *item = new QTreeWidgetItem(processesTab);
        item->setText(0, comm);                 // Name
        item->setData(1, Qt::DisplayRole, pid); // PID as integer
        item->setText(2, user);                 // User

        int cpuUsageInt = static_cast<int>(cpuUsage);                              // Convert to integer
        item->setText(3, QString::number(cpuUsageInt, 10).rightJustified(2, '0')); // CPU Usage
        item->setTextAlignment(3, Qt::AlignCenter);

        // Format memory usage with commas
        QLocale locale = QLocale::system();
        QString formattedMemory = locale.toString(memUsage);
        item->setText(4, formattedMemory + " K"); // Memory in kilobytes, broken
        item->setTextAlignment(4, Qt::AlignRight);

        item->setData(1, Qt::UserRole, QVariant(pid)); // Store PID as user data
        item->setData(3, Qt::UserRole,
                      QVariant(cpuUsage)); // Store CPU as user data
        item->setData(4, Qt::UserRole,
                      QVariant(memUsage)); // Store Memory as user data, needs fix
        processesTab->addTopLevelItem(item);
      }
    }
  }

  void updateServices()
  {

    QString selectedService;
    if (servicesTab->currentItem())
    {
      selectedService =
          servicesTab->currentItem()->text(0); // Save service name
    }

    QProcess process;
    process.setProgram("systemctl");
    process.setArguments(
        {"--user", "list-units", "--type=service", "--all", "--output=json"});
    process.start();
    process.waitForFinished();

    QByteArray output = process.readAllStandardOutput();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(output);
    if (!jsonDoc.isArray())
    {
      qWarning() << "Failed to parse JSON!";
      return;
    }

    servicesTab->clear();                    // clear the old shi
    QTreeWidgetItem *selectedItem = nullptr; // for restoring selection later

    QJsonArray services = jsonDoc.array();
    for (const QJsonValue &serviceValue : services)
    {
      QJsonObject service = serviceValue.toObject();
      QString name = service["unit"].toString();
      QString pid = service.contains("mainPID")
                        ? QString::number(service["mainPID"].toInt())
                        : "-";
      QString description = service["description"].toString();
      QString activeState = service["active"].toString();

      QTreeWidgetItem *item = new QTreeWidgetItem(servicesTab);
      item->setText(0, name);
      item->setText(1, pid);
      item->setText(2, description);
      item->setText(3, activeState);

      servicesTab->addTopLevelItem(item);

      // Check if this item was selected before refresh
      if (name == selectedService)
      {
        selectedItem = item;
      }
    }

    // restore selection if its there
    if (selectedItem)
    {
      servicesTab->setCurrentItem(selectedItem);
    }
  }
};

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  TaskManager taskManager;
  taskManager.show();
  return app.exec();
}