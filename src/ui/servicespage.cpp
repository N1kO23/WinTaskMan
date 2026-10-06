#include "servicespage.h"
#include "tablemodel.h"
#include "tableview.h"

#include <QVBoxLayout>

ServicesPage::ServicesPage(QWidget *parent)
    : RefreshablePage(parent),
      m_model(new TableModel({tr("Name"), tr("PID"), tr("Description"), tr("Status")}, this)),
      m_job(this, [this](const QList<ServiceInfo> &services) { showServices(services); })
{
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(new TableView(m_model, this));
}

void ServicesPage::refresh()
{
  m_job.start(&listServices);
}

void ServicesPage::showServices(const QList<ServiceInfo> &services)
{
  QList<TableRow> rows;
  rows.reserve(services.size());
  for (const ServiceInfo &service : services)
  {
    const QString pid = service.pid > 0 ? QString::number(service.pid) : QStringLiteral("-");
    rows.append(TableRow{service.name, {service.name, {pid, service.pid}, service.description, service.state}});
  }
  m_model->setRows(std::move(rows));
}
