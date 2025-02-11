#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTimer>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QSysInfo>
#include <QStringList>
#include <unistd.h>

int getTotalProcesses()
{
  QDir procDir("/proc");
  QStringList procEntries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  int processCount = 0;

  foreach (QString entry, procEntries)
  {
    if (entry.toInt())
    {
      processCount++;
    }
  }

  return processCount;
}
void getSystemUsage(int &cpuUsage, int &ramUsage)
{
  // Get CPU usage
  QFile cpuFile("/proc/stat");
  cpuFile.open(QIODevice::ReadOnly | QIODevice::Text);
  QTextStream cpuStream(&cpuFile);
  QString cpuLine = cpuStream.readLine();
  cpuFile.close();

  QStringList cpuValues = cpuLine.split(" ", Qt::SkipEmptyParts);
  int cpuUser = cpuValues[1].toInt();
  int cpuNice = cpuValues[2].toInt();
  int cpuSystem = cpuValues[3].toInt();
  int cpuIdle = cpuValues[4].toInt();
  int totalCpu = cpuUser + cpuNice + cpuSystem + cpuIdle;
  cpuUsage = totalCpu - cpuIdle;

  // Get RAM usage
  QFile memFile("/proc/meminfo");
  memFile.open(QIODevice::ReadOnly | QIODevice::Text);
  QTextStream memStream(&memFile);
  QString memTotalLine = memStream.readLine();
  QString memAvailableLine = memStream.readLine();
  memFile.close();

  QStringList memTotalValues = memTotalLine.split(" ", Qt::SkipEmptyParts);
  QStringList memAvailableValues = memAvailableLine.split(" ", Qt::SkipEmptyParts);
  int memTotal = memTotalValues[1].toInt();
  int memAvailable = memAvailableValues[1].toInt();
  ramUsage = memTotal - memAvailable;
}

void updateStatusBar(QStatusBar *statusBar)
{
  int cpuUsage, ramUsage;
  getSystemUsage(cpuUsage, ramUsage);
  int totalProcesses = getTotalProcesses();

  QString statusText = QString("Processes: %1 | CPU Usage: %2% | RAM Usage: %3 kB")
                           .arg(totalProcesses)
                           .arg(cpuUsage)
                           .arg(ramUsage);
  statusBar->showMessage(statusText);
}

class TaskManager : public QMainWindow
{
public:
  TaskManager(QWidget *parent = nullptr) : QMainWindow(parent)
  {
    setWindowTitle("Task Manager");

    // 🔹 Create menu bar
    QMenuBar *menuBar = new QMenuBar(this);
    QMenu *fileMenu = menuBar->addMenu("File");
    QMenu *optionsMenu = menuBar->addMenu("Options");
    QMenu *viewMenu = menuBar->addMenu("View");
    QMenu *helpMenu = menuBar->addMenu("Help");
    setMenuBar(menuBar);

    QStatusBar *statusBar = new QStatusBar(this);
    setStatusBar(statusBar);

    // 🔹 Create tab widget
    QTabWidget *tabWidget = new QTabWidget(this);

    // 🔹 Applications Tab (Placeholder)
    QWidget *applicationsTab = new QWidget(this);
    tabWidget->addTab(applicationsTab, "Applications");

    // 🔹 Processes Tab
    processesTab = new QTreeWidget(this);
    processesTab->setColumnCount(4);
    processesTab->setHeaderLabels({"Name", "PID", "CPU", "Memory"});
    processesTab->setRootIsDecorated(false);
    processesTab->setSortingEnabled(true);
    processesTab->setStyleSheet(R"(
      QTreeWidget { border: 1px solid gray; background: white; font-size: 11px; }
      QTreeWidget::item { padding: 1px; border-right: 1px solid lightgray; }
    )");
    tabWidget->addTab(processesTab, "Processes");

    // 🔹 Services Tab
    servicesTab = new QTreeWidget(this);
    servicesTab->setColumnCount(4);
    servicesTab->setHeaderLabels({"Name", "PID", "Description", "Status"});
    servicesTab->setRootIsDecorated(false);
    servicesTab->setSortingEnabled(true);
    servicesTab->setStyleSheet(R"(
      QTreeWidget { border: 1px solid gray; background: white; font-size: 11px; }
      QTreeWidget::item { padding: 1px; border-right: 1px solid lightgray; }
    )");
    tabWidget->addTab(servicesTab, "Services");

    // 🔹 Performance Tab (Placeholder)
    QWidget *performanceTab = new QWidget(this);
    tabWidget->addTab(performanceTab, "Performance");

    // 🔹 Networking Tab (Placeholder)
    QWidget *networkingTab = new QWidget(this);
    tabWidget->addTab(networkingTab, "Networking");

    // 🔹 Users Tab (Placeholder)
    QWidget *usersTab = new QWidget(this);
    tabWidget->addTab(usersTab, "Users");

    tabWidget->setStyleSheet(R"(
      QTabWidget { border: 1px solid gray; background: white; font-size: 11px; }
      QTabWidget::item { padding: 128px; }
  )");

    setCentralWidget(tabWidget);

    // 🔹 Timer to refresh process & service data
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TaskManager::updateProcesses);
    connect(timer, &QTimer::timeout, this, &TaskManager::updateServices);
    connect(timer, &QTimer::timeout, [statusBar]()
            { updateStatusBar(statusBar); });
    timer->start(1000);

    updateProcesses();
    updateServices();
  }

private:
  QTreeWidget *processesTab;
  QTreeWidget *servicesTab;

  void updateProcesses()
  {
    processesTab->clear();
    QDir procDir("/proc");
    QFileInfoList procEntries = procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    long pageSizeKb = sysconf(_SC_PAGESIZE) / 1024; // Get page size in kilobytes
    long ticksPerSec = sysconf(_SC_CLK_TCK);        // Get clock ticks per second

    foreach (const QFileInfo &entry, procEntries)
    {
      if (!entry.isDir() || !entry.fileName().toInt())
        continue;

      QString pidStr = entry.fileName();
      int pid = pidStr.toInt();
      QFile statFile(entry.filePath() + "/stat");
      QFile cmdlineFile(entry.filePath() + "/cmdline");

      if (!statFile.open(QIODevice::ReadOnly) || !cmdlineFile.open(QIODevice::ReadOnly))
        continue;

      QTextStream statStream(&statFile);
      QTextStream cmdlineStream(&cmdlineFile);

      QString stat = statStream.readAll();
      QString cmdline = cmdlineStream.readAll();

      QStringList statParts = stat.split(" ");
      if (statParts.size() < 24)
        continue;

      QString comm = cmdline.split('\0').join(' ');

      long utime = statParts[13].toLong(); // User mode time
      long stime = statParts[14].toLong(); // Kernel mode time
      long totalCpuTime = utime + stime;
      double cpuUsage = (double)totalCpuTime / ticksPerSec * 100.0; // CPU usage percentage

      long rss = statParts[23].toLong();           // RSS in pages
      double memUsage = rss * pageSizeKb / 1024.0; // Convert to megabytes

      QTreeWidgetItem *item = new QTreeWidgetItem(processesTab);
      item->setText(0, comm);                                                       // Name
      item->setData(1, Qt::DisplayRole, pid);                                       // PID as integer
      item->setData(2, Qt::DisplayRole, QString::number(cpuUsage, 'f', 2) + "%");   // CPU Percentage
      item->setData(3, Qt::DisplayRole, QString::number(memUsage, 'f', 2) + " MB"); // Memory in Megabytes

      item->setData(1, Qt::UserRole, QVariant(pid));      // Store PID as user data
      item->setData(2, Qt::UserRole, QVariant(cpuUsage)); // Store CPU as user data
      item->setData(3, Qt::UserRole, QVariant(memUsage)); // Store Memory as user data
      processesTab->addTopLevelItem(item);
    }
  }

  void updateServices()
  {

    QString selectedService;
    if (servicesTab->currentItem())
    {
      selectedService = servicesTab->currentItem()->text(0); // Save service name
    }

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

    servicesTab->clear();                    // clear the old shi
    QTreeWidgetItem *selectedItem = nullptr; // for restoring selection later

    QJsonArray services = jsonDoc.array();
    for (const QJsonValue &serviceValue : services)
    {
      QJsonObject service = serviceValue.toObject();
      QString name = service["unit"].toString();
      QString pid = service.contains("mainPID") ? QString::number(service["mainPID"].toInt()) : "-";
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