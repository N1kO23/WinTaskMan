#include "applicationspage.h"
#include "tablemodel.h"
#include "tableview.h"

#include "system/applications.h"

#include <QVBoxLayout>

ApplicationsPage::ApplicationsPage(QWidget *parent)
    : RefreshablePage(parent),
      m_model(new TableModel({tr("Task"), tr("Status")}, this)),
      m_job(this, [this](const QStringList &applications) { showApplications(applications); })
{
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(new TableView(m_model, this));
}

void ApplicationsPage::refresh()
{
  m_job.start(&listApplications);
}

void ApplicationsPage::showApplications(const QStringList &applications)
{
  QList<TableRow> rows;
  rows.reserve(applications.size());
  for (const QString &application : applications)
    rows.append(TableRow{application, {application, tr("Running")}});
  m_model->setRows(std::move(rows));
}
