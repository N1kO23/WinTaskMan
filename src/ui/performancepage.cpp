#include "performancepage.h"
#include "historygraph.h"
#include "usagemeter.h"

#include "system/trackers.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace
{
const QColor kCpuLineColor(0x00, 0xff, 0x00);
const QColor kMemoryLineColor(0x00, 0x80, 0xff);

QGroupBox *createBox(const QString &title, QWidget *content)
{
  auto *box = new QGroupBox(title);
  auto *layout = new QVBoxLayout(box);
  layout->addWidget(content);
  return box;
}

QString megabytes(qint64 kb)
{
  return QString::number(kb / 1024);
}

// "2.92 GB", or "950 MB" below a gigabyte.
QString memorySize(qint64 kb)
{
  if (kb < 1024 * 1024)
    return QStringLiteral("%1 MB").arg(kb / 1024);
  return QStringLiteral("%1 GB").arg(kb / (1024.0 * 1024.0), 0, 'f', 2);
}

// days:hours:minutes:seconds, as Windows shows the up time.
QString upTime(double seconds)
{
  const qint64 total = static_cast<qint64>(seconds);
  const QLatin1Char zero('0');
  return QStringLiteral("%1:%2:%3:%4")
      .arg(total / 86400)
      .arg(total / 3600 % 24, 2, 10, zero)
      .arg(total / 60 % 60, 2, 10, zero)
      .arg(total % 60, 2, 10, zero);
}
} // namespace

PerformancePage::PerformancePage(QWidget *parent)
    : QWidget(parent),
      m_cpuMeter(new UsageMeter(this)),
      m_cpuHistoryStack(new QStackedWidget(this)),
      m_cpuHistory(new HistoryGraph(kCpuLineColor, 1, this)),
      m_coreHistory(new HistoryGraph(kCpuLineColor, 1, this)),
      m_memoryMeter(new UsageMeter(this)),
      m_memoryHistory(new HistoryGraph(kMemoryLineColor, 2, this))
{
  m_cpuHistoryStack->addWidget(m_cpuHistory);
  m_cpuHistoryStack->addWidget(m_coreHistory);
  setPerCoreGraphsVisible(true);

  auto *graphs = new QGridLayout;
  graphs->addWidget(createBox(tr("CPU Usage"), m_cpuMeter), 0, 0);
  graphs->addWidget(createBox(tr("CPU Usage History"), m_cpuHistoryStack), 0, 1);
  graphs->addWidget(createBox(tr("Memory"), m_memoryMeter), 1, 0);
  graphs->addWidget(createBox(tr("Physical Memory Usage History"), m_memoryHistory), 1, 1);
  graphs->setColumnStretch(1, 1);
  graphs->setRowStretch(0, 1);
  graphs->setRowStretch(1, 1);

  // How the Windows figures map onto Linux: "Cached" matches free's buff/cache, paged kernel
  // memory is the slab the kernel can reclaim, and handles are open file handles.
  auto *memoryColumn = new QVBoxLayout;
  memoryColumn->addWidget(createStatsBox(
      tr("Physical Memory (MB)"),
      {{tr("Total"), [](const SystemUsage &usage) { return megabytes(usage.memory.totalKb); }},
       {tr("Cached"),
        [](const SystemUsage &usage)
        { return megabytes(usage.memory.buffersKb + usage.memory.cachedKb + usage.memory.reclaimableSlabKb); }},
       {tr("Available"), [](const SystemUsage &usage) { return megabytes(usage.memory.availableKb); }},
       {tr("Free"), [](const SystemUsage &usage) { return megabytes(usage.memory.freeKb); }}}));
  memoryColumn->addWidget(createStatsBox(
      tr("Kernel Memory (MB)"),
      {{tr("Paged"), [](const SystemUsage &usage) { return megabytes(usage.memory.reclaimableSlabKb); }},
       {tr("Nonpaged"),
        [](const SystemUsage &usage)
        {
          const MemoryInfo &memory = usage.memory;
          return megabytes(memory.unreclaimableSlabKb + memory.kernelStackKb + memory.pageTablesKb);
        }}}));

  auto *systemColumn = new QVBoxLayout;
  systemColumn->addWidget(createStatsBox(
      tr("System"),
      {{tr("Handles"), [](const SystemUsage &usage) { return QString::number(usage.handleCount); }},
       {tr("Threads"), [](const SystemUsage &usage) { return QString::number(usage.threadCount); }},
       {tr("Processes"), [](const SystemUsage &usage) { return QString::number(usage.processCount); }},
       {tr("Up Time"), [](const SystemUsage &usage) { return upTime(usage.uptimeSeconds); }},
       {tr("Commit (MB)"),
        [](const SystemUsage &usage)
        { return QStringLiteral("%1 / %2").arg(megabytes(usage.memory.committedKb), megabytes(usage.memory.commitLimitKb)); }}}));
  systemColumn->addStretch();

  auto *figures = new QHBoxLayout;
  figures->addLayout(memoryColumn, 1);
  figures->addLayout(systemColumn, 1);
  figures->addStretch(1);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 10, 10);
  layout->addLayout(graphs, 1);
  layout->addLayout(figures);
}

void PerformancePage::addSample(const SystemUsage &usage)
{
  m_cpuMeter->setValue(usage.cpuPercent, tr("%1 %").arg(qRound(usage.cpuPercent)));
  m_cpuHistory->addSample({usage.cpuPercent}, {usage.kernelPercent});
  m_coreHistory->addSample(usage.corePercents, usage.coreKernelPercents);

  m_memoryMeter->setValue(usage.memoryPercent, memorySize(usage.memory.totalKb - usage.memory.availableKb));
  m_memoryHistory->addSample({usage.memoryPercent});

  for (const Stat &stat : std::as_const(m_stats))
    stat.label->setText(stat.value(usage));
}

void PerformancePage::setPerCoreGraphsVisible(bool visible)
{
  m_cpuHistoryStack->setCurrentWidget(visible ? m_coreHistory : m_cpuHistory);
}

void PerformancePage::setKernelTimesVisible(bool visible)
{
  m_cpuHistory->setKernelTimesVisible(visible);
  m_coreHistory->setKernelTimesVisible(visible);
}

QGroupBox *PerformancePage::createStatsBox(const QString &title, const QList<QPair<QString, StatValue>> &rows)
{
  // Names on the left, values right-aligned, as in the Windows figures.
  auto *box = new QGroupBox(title, this);
  auto *layout = new QGridLayout(box);
  layout->setColumnStretch(0, 1);
  layout->setVerticalSpacing(2);
  for (int row = 0; row < rows.size(); ++row)
  {
    auto *value = new QLabel(box);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(new QLabel(rows[row].first, box), row, 0);
    layout->addWidget(value, row, 1);
    m_stats.append({value, rows[row].second});
  }
  return box;
}
