#include <QApplication>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QTimer>

class ServiceViewer : public QWidget
{
public:
  ServiceViewer(QWidget *parent = nullptr) : QWidget(parent)
  {
    QVBoxLayout *layout = new QVBoxLayout(this);

    treeWidget = new QTreeWidget(this);
    treeWidget->setColumnCount(4);
    treeWidget->setHeaderLabels({"Name", "PID", "Description", "Status"});
    treeWidget->setSortingEnabled(true); // Enable sorting on columns

    treeWidget->setAlternatingRowColors(false);
    treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    treeWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    treeWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(treeWidget);
    setLayout(layout);
    setWindowTitle("Task Manager");
    resize(800, 400);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ServiceViewer::loadServices);
    timer->start(1000); // 1 second interval

    loadServices();
  }

private:
  QTreeWidget *treeWidget;

  void loadServices()
  {

    QString selectedService;
    if (treeWidget->currentItem())
    {
      selectedService = treeWidget->currentItem()->text(0); // Save service name
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

    treeWidget->clear();                     // clear the old shi
    QTreeWidgetItem *selectedItem = nullptr; // for restoring selection later

    QJsonArray services = jsonDoc.array();
    for (const QJsonValue &serviceValue : services)
    {
      QJsonObject service = serviceValue.toObject();
      QString name = service["unit"].toString();
      QString pid = service.contains("mainPID") ? QString::number(service["mainPID"].toInt()) : "-";
      QString description = service["description"].toString();
      QString activeState = service["active"].toString();

      QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget);
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

      treeWidget->addTopLevelItem(item);

      // Check if this item was selected before refresh
      if (name == selectedService)
      {
        selectedItem = item;
      }
    }

    // restore selection if its there
    if (selectedItem)
    {
      treeWidget->setCurrentItem(selectedItem);
    }

    // Resize columns based on content
    // treeWidget->header()->setSectionResizeMode(true);
    // treeWidget->header()->setStretchLastSection(true);

    treeWidget->setStyleSheet(R"(
      QTreeWidget { border: 1px solid gray; background: white; font-size: 12px; }
      QTreeWidget::item { padding: 1px; }
  )");
  }
};

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  ServiceViewer viewer;
  viewer.show();
  return app.exec();
}
