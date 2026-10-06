#include "mainwindow.h"
#include "applicationspage.h"
#include "networkingpage.h"
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

QAction *addToggle(QMenu *menu, const QString &text, bool checked, QActionGroup *group = nullptr)
{
  QAction *action = menu->addAction(text);
  action->setCheckable(true);
  action->setChecked(checked);
  if (group)
    group->addAction(action);
  return action;
}
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
  m_applicationsPage = new ApplicationsPage(this);
  m_processesPage = new ProcessesPage(this);
  m_performancePage = new PerformancePage(this);
  m_networkingPage = new NetworkingPage(this);

  m_tabs = new QTabWidget(this);
  m_tabs->setContentsMargins(12, 12, 12, 12);
  m_tabs->addTab(m_applicationsPage, tr("Applications"));
  m_tabs->addTab(m_processesPage, tr("Processes"));
  m_tabs->addTab(new ServicesPage(this), tr("Services"));
  m_tabs->addTab(m_performancePage, tr("Performance"));
  m_tabs->addTab(m_networkingPage, tr("Networking"));
  m_tabs->addTab(new QWidget(this), tr("Users"));
  setCentralWidget(m_tabs);

  connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::refreshCurrentPage);
}

void MainWindow::createMenus()
{
  // The menus of the Windows 7 Task Manager, with its wording and defaults.
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(tr("&New Task (Run...)"), this, &MainWindow::runNewTask);
  fileMenu->addSeparator();
  fileMenu->addAction(tr("E&xit Task Manager"), this, &QWidget::close);

  QMenu *optionsMenu = menuBar()->addMenu(tr("&Options"));
  QAction *alwaysOnTop = addToggle(optionsMenu, tr("&Always On Top"), true);
  setAlwaysOnTop(alwaysOnTop->isChecked());
  connect(alwaysOnTop, &QAction::toggled, this, &MainWindow::setAlwaysOnTop);
  // These act on Switch To and on the notification area icon, neither of which exists yet.
  addToggle(optionsMenu, tr("&Minimize On Use"), false)->setEnabled(false);
  addToggle(optionsMenu, tr("&Hide When Minimized"), false)->setEnabled(false);
  optionsMenu->addSeparator();

  // The rest of the Options and View menus belongs to individual tabs.
  QAction *tabAlwaysActive = addToggle(optionsMenu, tr("&Tab Always Active"), false);
  connect(tabAlwaysActive, &QAction::toggled, this, [this](bool active) { m_networkingAlwaysActive = active; });
  QAction *showScale = addToggle(optionsMenu, tr("Show &Scale"), true);
  connect(showScale, &QAction::toggled, m_networkingPage, &NetworkingPage::setScaleVisible);
  QAction *autoZoom = addToggle(optionsMenu, tr("Auto &Zoom"), true);
  connect(autoZoom, &QAction::toggled, m_networkingPage, &NetworkingPage::setAutoZoom);

  QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(tr("&Refresh Now"), QKeySequence(Qt::Key_F5), this, &MainWindow::refresh);

  QMenu *speedMenu = viewMenu->addMenu(tr("&Update Speed"));
  auto *speedGroup = new QActionGroup(speedMenu);
  const auto addSpeed = [&](const QString &name, int intervalMs)
  {
    QAction *action = addToggle(speedMenu, name, intervalMs == kDefaultIntervalMs, speedGroup);
    connect(action, &QAction::triggered, this, [this, intervalMs] { setUpdateInterval(intervalMs); });
  };
  addSpeed(tr("&High"), 500);
  addSpeed(tr("&Normal"), 1000);
  addSpeed(tr("&Low"), 2000);
  addSpeed(tr("&Paused"), 0);
  viewMenu->addSeparator();

  auto *iconSizeGroup = new QActionGroup(viewMenu);
  QAction *largeIcons = addToggle(viewMenu, tr("La&rge Icons"), false, iconSizeGroup);
  QAction *smallIcons = addToggle(viewMenu, tr("S&mall Icons"), false, iconSizeGroup);
  QAction *details = addToggle(viewMenu, tr("&Details"), true, iconSizeGroup);
  largeIcons->setEnabled(false); // only the details view exists
  smallIcons->setEnabled(false);

  QAction *selectColumns = viewMenu->addAction(tr("&Select Columns..."));
  selectColumns->setEnabled(false); // not implemented yet

  QMenu *cpuHistoryMenu = viewMenu->addMenu(tr("&CPU History"));
  auto *cpuHistoryGroup = new QActionGroup(cpuHistoryMenu);
  addToggle(cpuHistoryMenu, tr("&One Graph, All CPUs"), false, cpuHistoryGroup);
  QAction *graphPerCpu = addToggle(cpuHistoryMenu, tr("One Graph &Per CPU"), true, cpuHistoryGroup);
  connect(graphPerCpu, &QAction::toggled, m_performancePage, &PerformancePage::setPerCoreGraphsVisible);
  QAction *kernelTimes = addToggle(viewMenu, tr("Show &Kernel Times"), false);
  connect(kernelTimes, &QAction::toggled, m_performancePage, &PerformancePage::setKernelTimesVisible);

  QMenu *adapterHistoryMenu = viewMenu->addMenu(tr("&Network Adapter History"));
  const auto addHistory = [&](const QString &name, NetworkingPage::History history, bool shown)
  {
    QAction *action = addToggle(adapterHistoryMenu, name, shown);
    connect(action, &QAction::toggled, m_networkingPage,
            [this, history](bool visible) { m_networkingPage->setHistoryVisible(history, visible); });
  };
  addHistory(tr("Bytes &Sent"), NetworkingPage::History::BytesSent, false);
  addHistory(tr("Bytes &Received"), NetworkingPage::History::BytesReceived, false);
  addHistory(tr("Bytes &Total"), NetworkingPage::History::BytesTotal, true);
  QAction *selectAdapterColumns = viewMenu->addAction(tr("&Select Columns..."));
  selectAdapterColumns->setEnabled(false); // not implemented yet
  QAction *cumulativeData = addToggle(viewMenu, tr("Show C&umulative Data"), false);
  cumulativeData->setEnabled(false); // only affects the byte count columns, which don't exist yet
  QAction *resetGraphs = viewMenu->addAction(tr("R&eset"), m_networkingPage, &NetworkingPage::reset);

  m_tabActions = {{m_applicationsPage, {largeIcons, smallIcons, details}},
                  {m_processesPage, {selectColumns}},
                  {m_performancePage, {cpuHistoryMenu->menuAction(), kernelTimes}},
                  {m_networkingPage,
                   {tabAlwaysActive, showScale, autoZoom, adapterHistoryMenu->menuAction(), selectAdapterColumns,
                    cumulativeData, resetGraphs}}};
  connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::updateTabMenus);
  updateTabMenus();

  QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(tr("&View Help"))->setEnabled(false); // not implemented yet
  helpMenu->addSeparator();
  helpMenu->addAction(tr("&About Task Manager"), this, &MainWindow::showAbout);
}

void MainWindow::updateTabMenus()
{
  for (auto tab = m_tabActions.cbegin(); tab != m_tabActions.cend(); ++tab)
  {
    for (QAction *action : tab.value())
      action->setVisible(tab.key() == m_tabs->currentWidget());
  }
}

void MainWindow::setAlwaysOnTop(bool onTop)
{
  // Changing the flags of a shown window hides it, so show it again.
  const bool visible = isVisible();
  setWindowFlag(Qt::WindowStaysOnTopHint, onTop);
  if (visible)
    show();
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

  // Options > Tab Always Active keeps the network graphs going while another tab is shown.
  if (m_networkingAlwaysActive && m_tabs->currentWidget() != m_networkingPage)
    m_networkingPage->refresh();
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
