#include "systemdataprovider.h"
#include "helperutils.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTextStream>
#include <QDateTime>
#include <unistd.h>

SystemDataProvider::SystemDataProvider()
{
  m_currentUser = qgetenv("USER");
  if (m_currentUser.isEmpty())
    m_currentUser = qgetenv("LOGNAME");
}

QString SystemDataProvider::currentUser() const
{
  return m_currentUser;
}

SystemUsage SystemDataProvider::refreshSystemUsage()
{
  SystemUsage usage = readSystemUsage();
  usage.totalProcesses = 0;

  QDir procDir("/proc");
  const QStringList procEntries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString &entry : procEntries)
  {
    if (entry.toInt() > 0)
      usage.totalProcesses++;
  }

  return usage;
}

QList<ProcessInfo> SystemDataProvider::refreshProcessList(bool includeAllUsers)
{
  QList<ProcessInfo> processList;
  QDir procDir("/proc");
  const QFileInfoList procEntries = procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

  const long pageSizeKb = sysconf(_SC_PAGESIZE) / 1024;
  const long ticksPerSec = sysconf(_SC_CLK_TCK);
  const int numCores = sysconf(_SC_NPROCESSORS_ONLN);

  QFile uptimeFile("/proc/uptime");
  double uptimeSeconds = 0.0;
  if (uptimeFile.open(QIODevice::ReadOnly))
  {
    QTextStream uptimeStream(&uptimeFile);
    uptimeStream >> uptimeSeconds;
    uptimeFile.close();
  }

  for (const QFileInfo &entry : procEntries)
  {
    if (!entry.isDir())
      continue;

    const int pid = entry.fileName().toInt();
    if (pid <= 0)
      continue;

    QFile statFile(entry.filePath() + "/stat");
    QFile cmdlineFile(entry.filePath() + "/cmdline");
    QFile statusFile(entry.filePath() + "/status");

    if (!statFile.open(QIODevice::ReadOnly) || !cmdlineFile.open(QIODevice::ReadOnly) || !statusFile.open(QIODevice::ReadOnly))
      continue;

    const QString stat = QTextStream(&statFile).readAll();
    const QString cmdline = QTextStream(&cmdlineFile).readAll();
    const QString status = QTextStream(&statusFile).readAll();

    statFile.close();
    cmdlineFile.close();
    statusFile.close();

    uid_t uid = 0;
    for (const QString &line : status.split('\n', Qt::SkipEmptyParts))
    {
      if (line.startsWith("Uid:"))
      {
        const QString uidToken = line.mid(4).trimmed();
        const QStringList uidParts = uidToken.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (!uidParts.isEmpty())
        {
          bool ok = false;
          uid = static_cast<uid_t>(uidParts.first().toInt(&ok));
          if (!ok)
            uid = 0;
        }
        break;
      }
    }

    const QString user = getUserFromUid(uid);
    if (!includeAllUsers && user != m_currentUser)
      continue;

    const QStringList statParts = stat.split(' ', Qt::SkipEmptyParts);
    if (statParts.size() < 24)
      continue;

    const QString name = cmdline.split(QLatin1Char('\0'), Qt::SkipEmptyParts).join(' ').trimmed();
    const long utime = statParts[13].toLong();
    const long stime = statParts[14].toLong();
    const long starttime = statParts[21].toLong();
    const long rssPages = statParts[23].toLong();
    const double totalCpuTime = static_cast<double>(utime + stime);
    const double processSeconds = uptimeSeconds - (static_cast<double>(starttime) / ticksPerSec);

    double cpuPercent = 0.0;
    if (processSeconds > 0.0)
    {
      const double previousCpu = m_previousProcessCpu.value(pid, 0);
      const double previousTime = m_previousProcessTime.value(pid, 0.0);
      const double deltaCpu = totalCpuTime - previousCpu;
      const double deltaTime = processSeconds - previousTime;
      if (deltaTime > 0.0 && deltaCpu > 0.0)
        cpuPercent = (deltaCpu / ticksPerSec) / deltaTime * 100.0 / qMax(1, numCores);
    }

    m_previousProcessCpu[pid] = static_cast<qint64>(totalCpuTime);
    m_previousProcessTime[pid] = processSeconds;

    ProcessInfo info;
    info.pid = pid;
    info.name = name.isEmpty() ? statParts[1].trimmed().remove('(').remove(')') : name;
    info.user = user;
    info.cpuPercent = cpuPercent;
    info.memoryKb = static_cast<double>(rssPages) * pageSizeKb;
    processList.append(info);
  }

  return processList;
}

QList<ServiceInfo> SystemDataProvider::refreshServices()
{
  QList<ServiceInfo> services;
  QProcess process;
  process.setProgram("systemctl");
  process.setArguments({"--user", "list-units", "--type=service", "--all", "--output=json"});
  process.start();
  process.waitForFinished();

  const QByteArray output = process.readAllStandardOutput();
  const QJsonDocument document = QJsonDocument::fromJson(output);
  if (!document.isArray())
    return services;

  for (const QJsonValue &value : document.array())
  {
    const QJsonObject serviceObject = value.toObject();
    ServiceInfo service;
    service.name = serviceObject.value("unit").toString();
    service.pid = serviceObject.contains("mainPID") ? QString::number(serviceObject.value("mainPID").toInt()) : "-";
    service.description = serviceObject.value("description").toString();
    service.state = serviceObject.value("active").toString();
    services.append(service);
  }

  return services;
}

QStringList SystemDataProvider::refreshApplications()
{
  QString displayType = qEnvironmentVariable("XDG_SESSION_TYPE");
  QString script;
  if (displayType == "x11")
  {
    script = R"(xlsclients | awk '{print $2}' | sort -u)";
  }
  else if (displayType == "wayland")
  {
    script = R"(x_apps=$(xlsclients 2>/dev/null | awk '{print $2}' | sort -u)
w_apps=$(ps -eo pid,comm,args | grep -E 'wayland|Xwayland' | awk '{print $2}' | sort -u)
echo -e "$x_apps
$w_apps" | sort -u)";
  }
  else
  {
    script = R"(echo 'Unknown display server')";
  }

  QProcess process;
  process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
  process.start("bash", QStringList() << "-lc" << script);
  process.waitForFinished();

  const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
  const QStringList applications = output.split('\n', Qt::SkipEmptyParts);
  return applications;
}

SystemUsage SystemDataProvider::readSystemUsage()
{
  SystemUsage usage;

  QFile cpuFile("/proc/stat");
  if (cpuFile.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QTextStream stream(&cpuFile);
    QString line;
    int index = 0;
    while (!stream.atEnd())
    {
      line = stream.readLine().trimmed();
      if (!line.startsWith("cpu"))
        break;

      const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
      if (parts.size() < 5)
        continue;

      const qint64 user = parts[1].toLongLong();
      const qint64 nice = parts[2].toLongLong();
      const qint64 system = parts[3].toLongLong();
      const qint64 idle = parts[4].toLongLong();
      const qint64 iowait = parts.size() > 5 ? parts[5].toLongLong() : 0;
      const qint64 irq = parts.size() > 6 ? parts[6].toLongLong() : 0;
      const qint64 softirq = parts.size() > 7 ? parts[7].toLongLong() : 0;
      const qint64 steal = parts.size() > 8 ? parts[8].toLongLong() : 0;

      const qint64 total = user + nice + system + idle + iowait + irq + softirq + steal;
      const qint64 used = total - idle;

      if (m_previousCpuTotals.size() != index + 1)
      {
        m_previousCpuTotals.resize(index + 1);
        m_previousCpuIdles.resize(index + 1);
        m_previousCpuTotals[index] = 0;
        m_previousCpuIdles[index] = 0;
      }

      if (m_previousCpuTotals[index] > 0 && total > m_previousCpuTotals[index])
      {
        const qint64 deltaTotal = total - m_previousCpuTotals[index];
        const qint64 deltaIdle = idle - m_previousCpuIdles[index];
        const int usageValue = static_cast<int>((deltaTotal - deltaIdle) * 100 / deltaTotal);
        if (index == 0)
          usage.cpuUsage = usageValue;
        else
          usage.coreUsages.append(usageValue);
      }
      else if (index > 0)
      {
        usage.coreUsages.append(0);
      }

      m_previousCpuTotals[index] = total;
      m_previousCpuIdles[index] = idle;
      index++;
    }

    usage.coreCount = index > 0 ? index - 1 : 0;
    cpuFile.close();
  }

  QFile memFile("/proc/meminfo");
  if (memFile.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QTextStream stream(&memFile);
    const QString memTotalLine = stream.readLine();
    const QString memAvailableLine = stream.readLine();
    memFile.close();

    const QStringList totalParts = memTotalLine.split(' ', Qt::SkipEmptyParts);
    const QStringList availableParts = memAvailableLine.split(' ', Qt::SkipEmptyParts);
    if (totalParts.size() > 1 && availableParts.size() > 1)
    {
      usage.totalRam = totalParts[1].toInt();
      const int available = availableParts[1].toInt();
      usage.ramUsage = usage.totalRam - available;
    }
  }

  return usage;
}
