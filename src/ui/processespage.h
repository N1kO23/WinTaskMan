#pragma once

#include "backgroundjob.h"
#include "refreshablepage.h"

#include "system/procfs.h"
#include "system/trackers.h"

class QPushButton;
class TableModel;
class TableView;

// The Processes tab: every process with its owner, CPU and memory use, and a way to end it.
class ProcessesPage : public RefreshablePage
{
  Q_OBJECT

public:
  explicit ProcessesPage(QWidget *parent = nullptr);

  void refresh() override;

private:
  void showProcesses(const ProcessSnapshot &snapshot);
  void updateRows();
  void updateEndProcessButton();
  void endSelectedProcess();

  TableModel *m_model = nullptr;
  TableView *m_view = nullptr;
  QPushButton *m_endProcessButton = nullptr;
  BackgroundJob<ProcessSnapshot> m_job;
  ProcessUsageTracker m_tracker;
  QList<ProcessInfo> m_processes; // from all users; filtered when shown
  bool m_showAllUsers = false;
};
