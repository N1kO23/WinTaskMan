#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTimer>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

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
    processesTab->setStyleSheet(R"(
      QTreeWidget { border: 1px solid gray; background: white; font-size: 11px; }
      QTreeWidget::item { padding: 1px; }
  )");
    tabWidget->addTab(processesTab, "Processes");

    // 🔹 Services Tab
    servicesTab = new QTreeWidget(this);
    servicesTab->setColumnCount(4);
    servicesTab->setHeaderLabels({"Name", "PID", "Description", "Status"});
    servicesTab->setRootIsDecorated(false);
    servicesTab->setStyleSheet(R"(
      QTreeWidget { border: 1px solid gray; background: white; font-size: 11px; }
      QTreeWidget::item { padding: 1px; }
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
      QTabWidget::item { padding: 0px; }
  )");

    setCentralWidget(tabWidget);

    // 🔹 Timer to refresh process & service data
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TaskManager::updateProcesses);
    connect(timer, &QTimer::timeout, this, &TaskManager::updateServices);
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
    QProcess process;
    process.start("ps -eo pid,comm,%cpu,%mem --no-headers");
    process.waitForFinished();
    QByteArray output = process.readAllStandardOutput();
    QList<QByteArray> lines = output.split('\n');

    for (const QByteArray &line : lines)
    {
      QList<QByteArray> columns = line.simplified().split(' ');
      if (columns.size() >= 4)
      {
        QTreeWidgetItem *item = new QTreeWidgetItem(processesTab);
        item->setText(0, columns[1]);         // Name
        item->setText(1, columns[0]);         // PID
        item->setText(2, columns[2] + "%");   // CPU
        item->setText(3, columns[3] + " MB"); // Memory
        processesTab->addTopLevelItem(item);
      }
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

      item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator); // hide this shi
      // Color code active/inactive status
      // if (activeState == "active")
      // {
      //   item->setBackground(3, QBrush(QColor(200, 255, 200))); // Light green for active
      // }
      // else
      // {
      //   item->setBackground(3, QBrush(QColor(255, 200, 200))); // Light red for inactive
      // }

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

    // Resize columns based on content
    // servicesTab->header()->setSectionResizeMode(true);
    // servicesTab->header()->setStretchLastSection(true);
  }
};

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  TaskManager taskManager;
  taskManager.show();
  return app.exec();
}
