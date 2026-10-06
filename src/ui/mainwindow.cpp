#include "mainwindow.h"
#include "applicationspage.h"
#include "performancepage.h"
#include "processespage.h"
#include "refreshablepage.h"
#include "rundialog.h"
#include "servicespage.h"

#include <QActionGroup>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>

namespace
{
constexpr int kDefaultIntervalMs = 1000;
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_usageJob(this, [this](const UsageSnapshot &snapshot) { showUsage(snapshot); })
{
  setWindowTitle(tr("Task Manager"));

  createTabs();
  createMenus();
  createStatusBar();

  connect(&m_timer, &QTimer::timeout, this, &MainWindow::refresh);
  setUpdateInterval(kDefaultIntervalMs);
  refresh();
}

void MainWindow::createTabs()
{
  m_performancePage = new PerformancePage(this);

  m_tabs = new QTabWidget(this);
  m_tabs->setContentsMargins(12, 12, 12, 12);
  m_tabs->addTab(new ApplicationsPage(this), tr("Applications"));
  m_tabs->addTab(new ProcessesPage(this), tr("Processes"));
  m_tabs->addTab(new ServicesPage(this), tr("Services"));
  m_tabs->addTab(m_performancePage, tr("Performance"));
  m_tabs->addTab(new QWidget(this), tr("Networking"));
  m_tabs->addTab(new QWidget(this), tr("Users"));
  setCentralWidget(m_tabs);

  connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::refreshCurrentPage);
}

void MainWindow::createMenus()
{
  QMenu *fileMenu = menuBar()->addMenu(tr("File"));
  fileMenu->addAction(tr("Run new task"), QKeySequence(tr("Ctrl+N")), this, &MainWindow::runNewTask);
  fileMenu->addSeparator();
  fileMenu->addAction(tr("Exit"), this, &QWidget::close);

  QMenu *viewMenu = menuBar()->addMenu(tr("View"));
  viewMenu->addAction(tr("Refresh now"), QKeySequence(Qt::Key_F5), this, &MainWindow::refresh);

  QMenu *speedMenu = viewMenu->addMenu(tr("Update speed"));
  auto *speedGroup = new QActionGroup(speedMenu);
  const auto addSpeed = [&](const QString &name, int intervalMs)
  {
    QAction *action = speedMenu->addAction(name, this, [this, intervalMs] { setUpdateInterval(intervalMs); });
    action->setCheckable(true);
    action->setChecked(intervalMs == kDefaultIntervalMs);
    speedGroup->addAction(action);
  };
  addSpeed(tr("High"), 500);
  addSpeed(tr("Normal"), 1000);
  addSpeed(tr("Low"), 2000);
  addSpeed(tr("Paused"), 0);

  viewMenu->addSeparator();
  QAction *perCoreGraphs = viewMenu->addAction(tr("Individual core usage"));
  perCoreGraphs->setCheckable(true);
  connect(perCoreGraphs, &QAction::toggled, m_performancePage, &PerformancePage::setPerCoreGraphsVisible);
  QAction *processHistory = viewMenu->addAction(tr("Show history for all processes"));
  processHistory->setCheckable(true);
  processHistory->setEnabled(false); // not implemented yet

  QMenu *helpMenu = menuBar()->addMenu(tr("Help"));
  helpMenu->addAction(tr("Help topics"))->setEnabled(false); // not implemented yet
  helpMenu->addSeparator();
  helpMenu->addAction(tr("About Task Manager"), this, &MainWindow::showAbout);
}

void MainWindow::createStatusBar()
{
  // Reserve room for the widest text, so the sections don't shift as the numbers change.
  const auto addSection = [this](const QString &widestText)
  {
    auto *label = new QLabel(this);
    label->setMinimumWidth(label->fontMetrics().horizontalAdvance(widestText));
    statusBar()->addWidget(label);
    return label;
  };
  m_processCountLabel = addSection(tr("Processes: %1").arg(99999));
  m_cpuUsageLabel = addSection(tr("CPU Usage: %1%").arg(100));
  m_memoryUsageLabel = addSection(tr("Physical Memory: %1%").arg(100.0, 0, 'f', 1));
}

void MainWindow::refresh()
{
  m_usageJob.start(&readUsageSnapshot);
  refreshCurrentPage();
}

void MainWindow::refreshCurrentPage()
{
  if (auto *page = qobject_cast<RefreshablePage *>(m_tabs->currentWidget()))
    page->refresh();
}

void MainWindow::showUsage(const UsageSnapshot &snapshot)
{
  const SystemUsage usage = m_usageTracker.update(snapshot);
  m_processCountLabel->setText(tr("Processes: %1").arg(usage.processCount));
  m_cpuUsageLabel->setText(tr("CPU Usage: %1%").arg(qRound(usage.cpuPercent)));
  m_memoryUsageLabel->setText(tr("Physical Memory: %1%").arg(usage.memoryPercent, 0, 'f', 1));
  m_performancePage->addSample(usage);
}

void MainWindow::setUpdateInterval(int intervalMs)
{
  if (intervalMs > 0)
    m_timer.start(intervalMs);
  else
    m_timer.stop();
}

void MainWindow::runNewTask()
{
  RunDialog dialog(this);
  dialog.exec();
}

void MainWindow::showAbout()
{
  QMessageBox::about(this, tr("About Task Manager"),
                     tr("<b>WinTaskMan</b><p>The old-fashioned Windows task manager, brought to Linux.</p>"
                        "<p>Running on Qt %1.</p>")
                         .arg(QLatin1String(qVersion())));
}
