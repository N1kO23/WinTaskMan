#include "trackers.h"

#include <unistd.h>

namespace
{
// How much a tick counter grew, as a percentage of the elapsed ticks.
double percentOf(quint64 now, quint64 before, double elapsedTicks)
{
  return qBound(0.0, 100.0 * (double(now) - double(before)) / elapsedTicks, 100.0);
}
} // namespace

SystemUsage SystemUsageTracker::update(const UsageSnapshot &snapshot)
{
  SystemUsage usage;
  usage.memory = snapshot.memory;
  usage.processCount = snapshot.processCount;
  usage.threadCount = snapshot.threadCount;
  usage.handleCount = snapshot.handleCount;
  usage.uptimeSeconds = snapshot.uptimeSeconds;
  if (snapshot.memory.totalKb > 0)
    usage.memoryPercent = 100.0 * (snapshot.memory.totalKb - snapshot.memory.availableKb) / snapshot.memory.totalKb;

  // Without a previous reading for the same set of CPUs there is nothing to compare against.
  const bool haveBaseline = m_previousCpus.size() == snapshot.cpus.size();
  for (int i = 0; i < snapshot.cpus.size(); ++i)
  {
    double percent = 0.0;
    double kernelPercent = 0.0;
    const CpuTimes &now = snapshot.cpus[i];
    if (haveBaseline && now.total > m_previousCpus[i].total)
    {
      const CpuTimes &before = m_previousCpus[i];
      const double elapsedTicks = double(now.total - before.total);
      percent = percentOf(now.busy, before.busy, elapsedTicks);
      kernelPercent = percentOf(now.kernel, before.kernel, elapsedTicks);
    }

    if (i == 0)
    {
      usage.cpuPercent = percent;
      usage.kernelPercent = kernelPercent;
    }
    else
    {
      usage.corePercents.append(percent);
      usage.coreKernelPercents.append(kernelPercent);
    }
  }

  m_previousCpus = snapshot.cpus;
  return usage;
}

ProcessUsageTracker::ProcessUsageTracker()
    : ProcessUsageTracker(sysconf(_SC_CLK_TCK), static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN)))
{
}

ProcessUsageTracker::ProcessUsageTracker(long ticksPerSecond, int cpuCount)
    : m_ticksPerSecond(qMax(1L, ticksPerSecond)), m_cpuCount(qMax(1, cpuCount))
{
}

QList<ProcessInfo> ProcessUsageTracker::update(const ProcessSnapshot &snapshot)
{
  const double elapsedSeconds = snapshot.uptimeSeconds - m_previousUptime;
  QHash<ProcessKey, quint64> cpuTicks;
  cpuTicks.reserve(snapshot.processes.size());

  QList<ProcessInfo> processes;
  processes.reserve(snapshot.processes.size());
  for (const ProcessSample &sample : snapshot.processes)
  {
    const ProcessKey key(sample.pid, sample.startTicks);
    cpuTicks.insert(key, sample.cpuTicks);

    ProcessInfo info;
    info.pid = sample.pid;
    info.uid = sample.uid;
    info.user = sample.user;
    info.name = sample.name;
    info.memoryKb = sample.memoryKb;

    const auto previous = m_previousCpuTicks.constFind(key);
    if (previous != m_previousCpuTicks.constEnd() && elapsedSeconds > 0.0 && sample.cpuTicks > *previous)
    {
      const double cpuSeconds = double(sample.cpuTicks - *previous) / m_ticksPerSecond;
      info.cpuPercent = qMin(100.0, 100.0 * cpuSeconds / elapsedSeconds / m_cpuCount);
    }
    processes.append(info);
  }

  // Replacing the map drops the processes that have exited.
  m_previousCpuTicks = std::move(cpuTicks);
  m_previousUptime = snapshot.uptimeSeconds;
  return processes;
}

QList<NetworkUsage> NetworkUsageTracker::update(const NetworkSnapshot &snapshot)
{
  const double elapsedSeconds = snapshot.uptimeSeconds - m_previousUptime;
  QHash<QString, NetworkAdapter> adapters;

  QList<NetworkUsage> usages;
  for (const NetworkAdapter &adapter : snapshot.adapters)
  {
    adapters.insert(adapter.name, adapter);

    NetworkUsage usage;
    usage.name = adapter.name;
    usage.connected = adapter.connected;
    usage.linkSpeedMbps = adapter.linkSpeedMbps;

    const auto previous = m_previousAdapters.constFind(adapter.name);
    if (previous != m_previousAdapters.constEnd() && elapsedSeconds > 0.0 && adapter.linkSpeedMbps > 0)
    {
      // Bytes moved per second, as a percentage of the link's capacity in bytes per second.
      const double capacity = adapter.linkSpeedMbps * 1e6 / 8 * elapsedSeconds;
      const auto share = [capacity](quint64 now, quint64 before)
      { return now > before ? qMin(100.0, 100.0 * double(now - before) / capacity) : 0.0; };
      usage.sentPercent = share(adapter.bytesSent, previous->bytesSent);
      usage.receivedPercent = share(adapter.bytesReceived, previous->bytesReceived);
      usage.totalPercent = qMin(100.0, usage.sentPercent + usage.receivedPercent);
    }
    usages.append(usage);
  }

  m_previousAdapters = std::move(adapters);
  m_previousUptime = snapshot.uptimeSeconds;
  return usages;
}
