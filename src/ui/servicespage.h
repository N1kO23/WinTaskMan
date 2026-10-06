#pragma once

#include "backgroundjob.h"
#include "refreshablepage.h"

#include "system/services.h"

class TableModel;

// The Services tab: systemd user services, or OpenRC services.
class ServicesPage : public RefreshablePage
{
  Q_OBJECT

public:
  explicit ServicesPage(QWidget *parent = nullptr);

  void refresh() override;

private:
  void showServices(const QList<ServiceInfo> &services);

  TableModel *m_model = nullptr;
  BackgroundJob<QList<ServiceInfo>> m_job;
};
