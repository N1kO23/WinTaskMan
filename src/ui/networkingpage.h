#pragma once

#include "backgroundjob.h"
#include "refreshablepage.h"

#include "system/network.h"
#include "system/trackers.h"

#include <QMap>

class HistoryGraph;
class QGroupBox;
class QVBoxLayout;
class TableModel;

// The Networking tab: a utilisation history graph for each network adapter, above a list of
// the adapters, as in the Windows 7 Task Manager.
class NetworkingPage : public RefreshablePage
{
  Q_OBJECT

public:
  // The lines a graph can show, in the order of View > Network Adapter History.
  enum class History
  {
    BytesSent,
    BytesReceived,
    BytesTotal,
  };

  explicit NetworkingPage(QWidget *parent = nullptr);

  void refresh() override;
  void setHistoryVisible(History history, bool visible);
  void setScaleVisible(bool visible);
  void setAutoZoom(bool autoZoom);
  void reset();

private:
  struct AdapterGraph
  {
    QGroupBox *box;
    HistoryGraph *graph;
  };

  void showUsage(const NetworkSnapshot &snapshot);
  void setAdapters(const QStringList &adapters);
  void configure(HistoryGraph *graph) const;

  QVBoxLayout *m_graphLayout = nullptr;
  QMap<QString, AdapterGraph> m_graphs; // by adapter name
  TableModel *m_model = nullptr;
  BackgroundJob<NetworkSnapshot> m_job;
  NetworkUsageTracker m_tracker;

  // Applied to every graph, including those of adapters that appear later.
  QList<bool> m_historyVisible = {false, false, true};
  bool m_scaleVisible = true;
  bool m_autoZoom = true;
};
