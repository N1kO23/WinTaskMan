#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QSplitter>
#include <QScrollBar>
#include <QListWidget>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

class ServiceViewer : public QWidget
{
public:
  ServiceViewer(QWidget *parent = nullptr) : QWidget(parent)
  {
    QVBoxLayout *layout = new QVBoxLayout(this);

    QSplitter *splitter = new QSplitter(this);

    splitter->setHandleWidth(-1);

    nameList = new QListWidget();
    pidList = new QListWidget();
    descList = new QListWidget();
    statusList = new QListWidget();

    nameList->setFixedWidth(200);
    pidList->setFixedWidth(80);
    descList->setFixedWidth(300);
    statusList->setFixedWidth(100);

    nameList->verticalScrollBar()->setStyleSheet("QScrollBar {width:0px;}");
    pidList->verticalScrollBar()->setStyleSheet("QScrollBar {width:0px;}");
    descList->verticalScrollBar()->setStyleSheet("QScrollBar {width:0px;}");

    nameList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pidList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    descList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    statusList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    splitter->addWidget(nameList);
    splitter->addWidget(pidList);
    splitter->addWidget(descList);
    splitter->addWidget(statusList);

    layout->addWidget(splitter);
    setLayout(layout);
    setWindowTitle("Systemd User Services");
    resize(800, 400);

    loadServices();

    // Synchronize scrolling
    connect(statusList->verticalScrollBar(), &QScrollBar::valueChanged, pidList->verticalScrollBar(), &QScrollBar::setValue);
    connect(statusList->verticalScrollBar(), &QScrollBar::valueChanged, descList->verticalScrollBar(), &QScrollBar::setValue);
    connect(statusList->verticalScrollBar(), &QScrollBar::valueChanged, nameList->verticalScrollBar(), &QScrollBar::setValue);
  }

private:
  QListWidget *nameList;
  QListWidget *pidList;
  QListWidget *descList;
  QListWidget *statusList;

  void loadServices()
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

    QJsonArray services = jsonDoc.array();
    for (const QJsonValue &serviceValue : services)
    {
      QJsonObject service = serviceValue.toObject();
      QString name = service["unit"].toString();
      QString pid = service.contains("mainPID") ? QString::number(service["mainPID"].toInt()) : "-";
      QString description = service["description"].toString();
      QString activeState = service["active"].toString();

      nameList->addItem(name);
      pidList->addItem(pid);
      descList->addItem(description);
      statusList->addItem(activeState);
    }
  }
};

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  ServiceViewer viewer;
  viewer.show();
  return app.exec();
}
