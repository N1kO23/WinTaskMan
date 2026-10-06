#include "networkingpage.h"
#include "historygraph.h"
#include "tablemodel.h"
#include "tableview.h"

#include <QGroupBox>
#include <QLocale>
#include <QVBoxLayout>

namespace
{
// Colours of the Bytes Sent, Bytes Received and Bytes Total lines, as in Windows.
const QColor kSentColor(0xff, 0x00, 0x00);
const QColor kReceivedColor(0xff, 0xff, 0x00);
const QColor kTotalColor(0x00, 0xff, 0x00);

// The graph cells are taller than on the Performance tab.
constexpr int kGridRowHeight = 22;

QString linkSpeedText(qint64 mbps)
{
  if (mbps <= 0)
    return QStringLiteral("-");
  if (mbps < 1000)
    return QStringLiteral("%1 Mbps").arg(mbps);
  return QLocale::system().toString(mbps / 1000.0) + QLatin1String(" Gbps");
}

QString utilizationText(double percent)
{
  if (percent <= 0.0)
    return QStringLiteral("0 %");
  return QLocale::system().toString(percent, 'f', 2) + QLatin1String(" %");
}
} // namespace

NetworkingPage::NetworkingPage(QWidget *parent)
    : RefreshablePage(parent),
      m_graphLayout(new QVBoxLayout),
      m_model(new TableModel({tr("Adapter Name"),
                              {tr("Network Utilization"), Qt::AlignRight | Qt::AlignVCenter},
                              {tr("Link Speed"), Qt::AlignRight | Qt::AlignVCenter},
                              tr("State")},
                             this)),
      m_job(this, [this](const NetworkSnapshot &snapshot) { showUsage(snapshot); })
{
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 10, 10);
  layout->addLayout(m_graphLayout, 3);
  layout->addWidget(new TableView(m_model, this), 1);
}

void NetworkingPage::refresh()
{
  m_job.start(&readNetworkSnapshot);
}

void NetworkingPage::setHistoryVisible(History history, bool visible)
{
  m_historyVisible[static_cast<int>(history)] = visible;
  for (const AdapterGraph &adapter : std::as_const(m_graphs))
    configure(adapter.graph);
}

void NetworkingPage::setScaleVisible(bool visible)
{
  m_scaleVisible = visible;
  for (const AdapterGraph &adapter : std::as_const(m_graphs))
    configure(adapter.graph);
}

void NetworkingPage::setAutoZoom(bool autoZoom)
{
  m_autoZoom = autoZoom;
  for (const AdapterGraph &adapter : std::as_const(m_graphs))
    configure(adapter.graph);
}

void NetworkingPage::reset()
{
  for (const AdapterGraph &adapter : std::as_const(m_graphs))
    adapter.graph->clear();
}

void NetworkingPage::showUsage(const NetworkSnapshot &snapshot)
{
  const QList<NetworkUsage> usages = m_tracker.update(snapshot);

  QStringList adapters;
  for (const NetworkUsage &usage : usages)
    adapters.append(usage.name);
  setAdapters(adapters);

  QList<TableRow> rows;
  for (const NetworkUsage &usage : usages)
  {
    m_graphs.value(usage.name).graph->addSample({{usage.sentPercent}, {usage.receivedPercent}, {usage.totalPercent}});
    rows.append(TableRow{usage.name,
                         {usage.name,
                          {utilizationText(usage.totalPercent), usage.totalPercent},
                          {linkSpeedText(usage.linkSpeedMbps), usage.linkSpeedMbps},
                          usage.connected ? tr("Connected") : tr("Disconnected")}});
  }
  m_model->setRows(std::move(rows));
}

void NetworkingPage::setAdapters(const QStringList &adapters)
{
  // Adapters rarely come and go, so rebuilding every graph when they do is simplest.
  QStringList sorted = adapters;
  sorted.sort();
  if (sorted == m_graphs.keys())
    return;

  for (const AdapterGraph &adapter : std::as_const(m_graphs))
    delete adapter.box;
  m_graphs.clear();

  for (const QString &name : std::as_const(sorted))
  {
    auto *graph = new HistoryGraph({kSentColor, kReceivedColor, kTotalColor});
    graph->setGridRowHeight(kGridRowHeight);
    configure(graph);

    auto *box = new QGroupBox(name, this);
    auto *boxLayout = new QVBoxLayout(box);
    boxLayout->addWidget(graph);
    m_graphLayout->addWidget(box);
    m_graphs.insert(name, {box, graph});
  }
}

void NetworkingPage::configure(HistoryGraph *graph) const
{
  for (int history = 0; history < m_historyVisible.size(); ++history)
    graph->setSeriesVisible(history, m_historyVisible[history]);
  graph->setScaleVisible(m_scaleVisible);
  graph->setAutoZoom(m_autoZoom);
}
