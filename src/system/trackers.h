#pragma once

#include "network.h"
#include "procfs.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>

struct SystemUsage
{
  double cpuPercent = 0.0;
  double kernelPercent = 0.0; // the part of cpuPercent spent in the kernel
  QList<double> corePercents;
  QList<double> coreKernelPercents;
  double memoryPercent = 0.0;
  MemoryInfo memory;
  int processCount = 0;
  int threadCount = 0;
  qint64 handleCount = 0;
  double uptimeSeconds = 0.0;
};

// Turns consecutive UsageSnapshots into percentages. CPU usage is measured between two
// snapshots, so the first update reports 0%.
class SystemUsageTracker
{
public:
  SystemUsage update(const UsageSnapshot &snapshot);

private:
  QList<CpuTimes> m_previousCpus;
};

struct ProcessInfo
{
  int pid = 0;
  uint uid = 0;
  QString user;
  QString name;
  double cpuPercent = 0.0; // share of the whole machine, as in the Windows Task Manager
  qint64 memoryKb = 0;
};

// Turns consecutive ProcessSnapshots into per-process CPU usage. A process seen for the
// first time reports 0%.
class ProcessUsageTracker
{
public:
  ProcessUsageTracker(); // uses this system's clock tick rate and CPU count
  ProcessUsageTracker(long ticksPerSecond, int cpuCount);

  QList<ProcessInfo> update(const ProcessSnapshot &snapshot);

private:
  // A pid can be reused, so a process is identified by its pid and start time.
  using ProcessKey = QPair<int, quint64>;

  long m_ticksPerSecond;
  int m_cpuCount;
  double m_previousUptime = 0.0;
  QHash<ProcessKey, quint64> m_previousCpuTicks;
};

struct NetworkUsage
{
  QString name;
  bool connected = false;
  qint64 linkSpeedMbps = 0;
  // Shares of the link speed. Total can't exceed 100%, even on a full-duplex link.
  double sentPercent = 0.0;
  double receivedPercent = 0.0;
  double totalPercent = 0.0;
};

// Turns consecutive NetworkSnapshots into link utilisation. An adapter seen for the first time,
// or one whose link speed is unknown, reports 0%.
class NetworkUsageTracker
{
public:
  QList<NetworkUsage> update(const NetworkSnapshot &snapshot);

private:
  double m_previousUptime = 0.0;
  QHash<QString, NetworkAdapter> m_previousAdapters;
};
