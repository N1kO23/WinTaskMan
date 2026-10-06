#pragma once

#include "procfs.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>

struct SystemUsage
{
  double cpuPercent = 0.0;
  QList<double> corePercents;
  double memoryPercent = 0.0;
  int processCount = 0;
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
