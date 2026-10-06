#pragma once

#include "backgroundjob.h"
#include "refreshablepage.h"

#include <QStringList>

class TableModel;

// The Applications tab: the applications running in the current graphical session.
class ApplicationsPage : public RefreshablePage
{
  Q_OBJECT

public:
  explicit ApplicationsPage(QWidget *parent = nullptr);

  void refresh() override;

private:
  void showApplications(const QStringList &applications);

  TableModel *m_model = nullptr;
  BackgroundJob<QStringList> m_job;
};
