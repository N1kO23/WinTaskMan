#include "processespage.h"
#include "tablemodel.h"
#include "tableview.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <cerrno>
#include <cstring>
#include <signal.h>
#include <unistd.h>

ProcessesPage::ProcessesPage(QWidget *parent)
    : RefreshablePage(parent),
      m_model(new TableModel({tr("Name"),
                              tr("PID"),
                              tr("User"),
                              {tr("CPU"), Qt::AlignCenter},
                              {tr("Working Set (Memory)"), Qt::AlignRight | Qt::AlignVCenter}},
                             this)),
      m_job(this, [this](const ProcessSnapshot &snapshot) { showProcesses(snapshot); })
{
  m_view = new TableView(m_model, this);

  auto *showAllUsers = new QCheckBox(tr("Show processes from all users"), this);
  m_endProcessButton = new QPushButton(tr("End Process"), this);
  m_endProcessButton->setEnabled(false);

  auto *controls = new QHBoxLayout;
  controls->addWidget(showAllUsers);
  controls->addStretch();
  controls->addWidget(m_endProcessButton);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 10, 10);
  layout->setSpacing(5);
  layout->addWidget(m_view);
  layout->addLayout(controls);

  connect(showAllUsers, &QCheckBox::toggled, this, [this](bool checked)
          {
            m_showAllUsers = checked;
            updateRows();
          });
  connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ProcessesPage::updateEndProcessButton);
  connect(m_endProcessButton, &QPushButton::clicked, this, &ProcessesPage::endSelectedProcess);
}

void ProcessesPage::refresh()
{
  m_job.start(&readProcessSnapshot);
}

void ProcessesPage::showProcesses(const ProcessSnapshot &snapshot)
{
  m_processes = m_tracker.update(snapshot);
  updateRows();
}

void ProcessesPage::updateRows()
{
  const uint currentUid = getuid();
  const QLocale locale = QLocale::system();

  QList<TableRow> rows;
  rows.reserve(m_processes.size());
  for (const ProcessInfo &process : std::as_const(m_processes))
  {
    if (!m_showAllUsers && process.uid != currentUid)
      continue;

    // Sort on the value as shown, so processes that display the same CPU usage keep their order.
    const double cpuPercent = qRound(process.cpuPercent * 10) / 10.0;
    const QString pid = QString::number(process.pid);
    rows.append(TableRow{pid,
                         {process.name,
                          {pid, process.pid},
                          process.user,
                          {QString::number(cpuPercent, 'f', 1), cpuPercent},
                          {locale.toString(process.memoryKb) + QLatin1String(" K"), process.memoryKb}}});
  }
  m_model->setRows(std::move(rows));

  // The selected process may have exited.
  updateEndProcessButton();
}

void ProcessesPage::updateEndProcessButton()
{
  m_endProcessButton->setEnabled(m_view->selectionModel()->hasSelection());
}

void ProcessesPage::endSelectedProcess()
{
  const int pid = m_view->selectedKey().toInt();
  if (pid <= 0)
    return;

  if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to end this process?")) != QMessageBox::Yes)
    return;

  if (::kill(pid, SIGTERM) != 0)
  {
    const int error = errno;
    QMessageBox::warning(this, tr("Unable to End Process"),
                         tr("The process could not be ended: %1").arg(QString::fromLocal8Bit(std::strerror(error))));
    return;
  }

  refresh();
}
