#include "procfs.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QStringList>

#include <cerrno>
#include <pwd.h>
#include <unistd.h>
#include <vector>

namespace
{
QString userName(uint uid)
{
  const long sizeHint = sysconf(_SC_GETPW_R_SIZE_MAX);
  std::vector<char> buffer(sizeHint > 0 ? sizeHint : 1024);
  passwd entry;
  passwd *result = nullptr;
  while (getpwuid_r(uid, &entry, buffer.data(), buffer.size(), &result) == ERANGE)
    buffer.resize(buffer.size() * 2);
  return result ? QString::fromLocal8Bit(entry.pw_name) : QStringLiteral("unknown");
}
} // namespace

UsageSnapshot readUsageSnapshot()
{
  UsageSnapshot snapshot;
  snapshot.cpus = parseCpuTimes(readProcFile(QStringLiteral("/proc/stat")));
  snapshot.memory = parseMemInfo(readProcFile(QStringLiteral("/proc/meminfo")));
  snapshot.processCount = listPids().size();
  return snapshot;
}

ProcessSnapshot readProcessSnapshot()
{
  ProcessSnapshot snapshot;
  snapshot.uptimeSeconds = readProcFile(QStringLiteral("/proc/uptime")).split(' ').value(0).toDouble();

  const qint64 pageSizeKb = sysconf(_SC_PAGESIZE) / 1024;
  QHash<uint, QString> userNames;

  for (const int pid : listPids())
  {
    const QString directory = QStringLiteral("/proc/%1/").arg(pid);
    const std::optional<ProcessStat> stat = parseProcessStat(readProcFile(directory + QLatin1String("stat")));
    const std::optional<uint> uid = parseStatusUid(readProcFile(directory + QLatin1String("status")));
    if (!stat || !uid)
      continue; // the process exited while we were reading it

    auto user = userNames.constFind(*uid);
    if (user == userNames.constEnd())
      user = userNames.insert(*uid, userName(*uid));

    ProcessSample sample;
    sample.pid = pid;
    sample.uid = *uid;
    sample.user = *user;
    sample.name = parseCmdline(readProcFile(directory + QLatin1String("cmdline")));
    if (sample.name.isEmpty())
      sample.name = stat->comm; // kernel threads have no command line
    sample.startTicks = stat->startTicks;
    sample.cpuTicks = stat->cpuTicks;
    sample.memoryKb = stat->rssPages * pageSizeKb;
    snapshot.processes.append(sample);
  }

  return snapshot;
}

QList<int> listPids()
{
  QList<int> pids;
  const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString &entry : entries)
  {
    const int pid = entry.toInt();
    if (pid > 0)
      pids.append(pid);
  }
  return pids;
}

QByteArray readProcFile(const QString &path)
{
  // Files in /proc report a size of 0, so QFile::atEnd() is true before anything has been
  // read; readAll() keeps reading until the kernel has nothing more to give.
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return QByteArray();
  return file.readAll();
}

QList<CpuTimes> parseCpuTimes(const QByteArray &procStat)
{
  QList<CpuTimes> cpus;
  for (const QByteArray &line : procStat.split('\n'))
  {
    if (!line.startsWith("cpu"))
      break; // the cpu lines come first, followed by unrelated counters

    // "cpu<N> user nice system idle iowait irq softirq steal guest guest_nice". Guest time is
    // already included in user and nice, so it isn't added again.
    const QList<QByteArray> fields = line.simplified().split(' ');
    if (fields.size() < 5)
      continue;

    quint64 total = 0;
    for (int i = 1; i < qMin<int>(fields.size(), 9); ++i)
      total += fields[i].toULongLong();
    const quint64 idle = fields[4].toULongLong() + fields.value(5).toULongLong();
    cpus.append({total - idle, total});
  }
  return cpus;
}

MemoryInfo parseMemInfo(const QByteArray &memInfo)
{
  MemoryInfo info;
  for (const QByteArray &line : memInfo.split('\n'))
  {
    // "MemTotal:       16318444 kB"
    const int colon = line.indexOf(':');
    if (colon < 0)
      continue;

    const QByteArray key = line.left(colon);
    const qint64 valueKb = line.mid(colon + 1).simplified().split(' ').value(0).toLongLong();
    if (key == "MemTotal")
      info.totalKb = valueKb;
    else if (key == "MemAvailable")
      info.availableKb = valueKb;
  }
  return info;
}

std::optional<ProcessStat> parseProcessStat(const QByteArray &stat)
{
  // The command name is in parentheses and may itself contain spaces or parentheses, so the
  // other fields are located from the last ')'.
  const int nameStart = stat.indexOf('(');
  const int nameEnd = stat.lastIndexOf(')');
  if (nameStart < 0 || nameEnd <= nameStart)
    return std::nullopt;

  // fields[0] is field 3 (state) in proc(5)'s numbering.
  const QList<QByteArray> fields = stat.mid(nameEnd + 1).simplified().split(' ');
  if (fields.size() < 22)
    return std::nullopt;

  ProcessStat result;
  result.comm = QString::fromLocal8Bit(stat.mid(nameStart + 1, nameEnd - nameStart - 1));
  result.cpuTicks = fields[11].toULongLong() + fields[12].toULongLong(); // utime, stime
  result.startTicks = fields[19].toULongLong();                          // starttime
  result.rssPages = fields[21].toLongLong();                             // rss
  return result;
}

std::optional<uint> parseStatusUid(const QByteArray &status)
{
  for (const QByteArray &line : status.split('\n'))
  {
    // "Uid:\t<real>\t<effective>\t<saved>\t<filesystem>"
    if (!line.startsWith("Uid:"))
      continue;

    bool ok = false;
    const uint uid = line.mid(4).simplified().split(' ').value(0).toUInt(&ok);
    if (!ok)
      return std::nullopt;
    return uid;
  }
  return std::nullopt;
}

QString parseCmdline(const QByteArray &cmdline)
{
  // The arguments are NUL-terminated; show them space-separated like a shell command line.
  QStringList arguments;
  for (const QByteArray &argument : cmdline.split('\0'))
  {
    if (!argument.isEmpty())
      arguments.append(QString::fromLocal8Bit(argument));
  }
  return arguments.join(QLatin1Char(' ')).trimmed();
}
