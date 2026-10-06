#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

// Cumulative time a CPU (or all of them together) has spent busy, in clock ticks.
struct CpuTimes
{
  quint64 busy = 0;
  quint64 kernel = 0; // the part of busy spent in the kernel: system, irq and softirq time
  quint64 total = 0;
};

// Figures from /proc/meminfo, in KiB.
struct MemoryInfo
{
  qint64 totalKb = 0;
  qint64 availableKb = 0;
  qint64 freeKb = 0;
  qint64 buffersKb = 0;
  qint64 cachedKb = 0;
  qint64 reclaimableSlabKb = 0;   // SReclaimable
  qint64 unreclaimableSlabKb = 0; // SUnreclaim
  qint64 kernelStackKb = 0;
  qint64 pageTablesKb = 0;
  qint64 committedKb = 0; // Committed_AS
  qint64 commitLimitKb = 0;
};

// The fields of /proc/<pid>/stat that WinTaskMan uses.
struct ProcessStat
{
  QString comm;
  quint64 cpuTicks = 0;   // utime + stime
  quint64 startTicks = 0; // since boot
  qint64 rssPages = 0;
};

// Raw counters for one process; ProcessUsageTracker turns the CPU ticks into a percentage.
struct ProcessSample
{
  int pid = 0;
  uint uid = 0;
  QString user;
  QString name;
  quint64 startTicks = 0;
  quint64 cpuTicks = 0;
  qint64 memoryKb = 0;
};

struct UsageSnapshot
{
  QList<CpuTimes> cpus; // all CPUs combined, followed by one entry per core
  MemoryInfo memory;
  int processCount = 0;
  int threadCount = 0;
  qint64 handleCount = 0; // open file handles, system-wide
  double uptimeSeconds = 0.0;
};

struct ProcessSnapshot
{
  double uptimeSeconds = 0.0;
  QList<ProcessSample> processes;
};

// Readers. They block on I/O, keep no state and are safe to call from any thread.
UsageSnapshot readUsageSnapshot();
ProcessSnapshot readProcessSnapshot();
QList<int> listPids();
QByteArray readProcFile(const QString &path);

// Parsers for the /proc file formats, kept free of I/O so they can be unit tested.
QList<CpuTimes> parseCpuTimes(const QByteArray &procStat);
MemoryInfo parseMemInfo(const QByteArray &memInfo);
double parseUptime(const QByteArray &uptime);        // /proc/uptime
int parseThreadCount(const QByteArray &loadAvg);     // /proc/loadavg
qint64 parseHandleCount(const QByteArray &fileNr);   // /proc/sys/fs/file-nr
std::optional<ProcessStat> parseProcessStat(const QByteArray &stat);
std::optional<uint> parseStatusUid(const QByteArray &status);
QString parseCmdline(const QByteArray &cmdline);
