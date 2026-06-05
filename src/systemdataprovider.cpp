#include "systemdataprovider.h"
#include "helperutils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QDateTime>
#include <functional>
#include <unistd.h>
#include <sys/sysinfo.h>

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

    int begin = stat.indexOf('(');
    int end = stat.lastIndexOf(')');
    if (begin < 0 || end < 0 || end <= begin)
      continue;

    const QStringList statFields = stat.mid(end + 2).split(' ', Qt::SkipEmptyParts);
    if (statFields.size() < 22)
      continue;

    const QString commandLine = cmdline.split(QLatin1Char('\0'), Qt::SkipEmptyParts).join(' ').trimmed();
    const QString statName = stat.mid(begin + 1, end - begin - 1);
    const QString name = commandLine.isEmpty() ? statName : commandLine;

    const long utime = statFields[11].toLong();
    const long stime = statFields[12].toLong();
    const long starttime = statFields[19].toLong();
    const long rssPages = statFields[21].toLong();
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
    info.name = name.isEmpty() ? statName : name;
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

static bool isExcludedWaylandClient(const QString &name)
{
  static const QSet<QString> excluded = {
      "xwayland", "wayland", "gnome-shell", "kwin_wayland", "plasmashell",
      "sway", "weston", "waybar", "pipewire", "wireplumber",
      "xdg-desktop-portal", "xdg-desktop-portal-wlr", "dbus-daemon",
      "systemd", "bash", "sh", "zsh", "fish", "login", "loginctl",
      "gnome-session", "ksmserver", "kded5", "autostart"};
  return excluded.contains(name.toLower());
}

static bool hasOpenWaylandSocket(int pid)
{
  const QString fdDirPath = QStringLiteral("/proc/%1/fd").arg(pid);
  QDir fdDir(fdDirPath);
  if (!fdDir.exists())
    return false;

  const QFileInfoList fdEntries = fdDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
  for (const QFileInfo &fdEntry : fdEntries)
  {
    const QString target = QFile::symLinkTarget(fdEntry.filePath());
    if (target.contains(QStringLiteral("wayland-"), Qt::CaseInsensitive))
      return true;
    if (target.contains(QStringLiteral("/run/user/"), Qt::CaseInsensitive) && target.contains(QStringLiteral("wayland"), Qt::CaseInsensitive))
      return true;
  }

  return false;
}

static QStringList collectSwayApplications()
{
  QProcess process;
  process.setProgram("swaymsg");
  process.setArguments({"-t", "get_tree"});
  process.start();
  if (!process.waitForFinished(1000))
    return {};

  const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput());
  if (!document.isObject())
    return {};

  QStringList applications;
  const std::function<void(const QJsonObject &)> scanNode = [&](const QJsonObject &node)
  {
    const QString type = node.value("type").toString();
    const bool visible = node.value("visible").toBool();
    const bool hasWindow = !node.value("window").isNull();

    if ((type == "con" || type == "floating_con") && hasWindow && visible)
    {
      const QJsonObject properties = node.value("window_properties").toObject();
      QString appId = properties.value("class").toString();
      if (appId.isEmpty())
        appId = properties.value("instance").toString();
      if (appId.isEmpty())
        appId = node.value("app_id").toString();
      if (appId.isEmpty())
        appId = node.value("name").toString();

      if (!appId.isEmpty() && !isExcludedWaylandClient(appId))
        applications.append(appId);
    }

    for (const QJsonValue &child : node.value("nodes").toArray())
      scanNode(child.toObject());
    for (const QJsonValue &child : node.value("floating_nodes").toArray())
      scanNode(child.toObject());
  };

  scanNode(document.object());
  applications.removeDuplicates();
  applications.sort();
  return applications;
}

static QStringList collectGenericWaylandApplications(const QString &currentUser)
{
  const uid_t currentUid = geteuid();
  QStringList applications;
  QDir procDir("/proc");
  const QFileInfoList procEntries = procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

  for (const QFileInfo &entry : procEntries)
  {
    const int pid = entry.fileName().toInt();
    if (pid <= 0)
      continue;

    QFile environFile(entry.filePath() + "/environ");
    if (!environFile.open(QIODevice::ReadOnly))
    {
      qDebug() << "cannot open environ for" << pid << environFile.errorString();
      continue;
    }

    const QByteArray envData = environFile.readAll();
    environFile.close();
    if (!envData.contains("WAYLAND_DISPLAY=") && !envData.contains("WAYLAND_SOCKET="))
      continue;

    if (!hasOpenWaylandSocket(pid))
    {
      qDebug() << "pid" << pid << "has no open Wayland socket";
      continue;
    }

    qDebug() << "wayland candidate pid" << pid << "env contains" << envData.contains("WAYLAND_DISPLAY=") << envData.contains("WAYLAND_SOCKET=");

    QFile statusFile(entry.filePath() + "/status");
    if (!statusFile.open(QIODevice::ReadOnly))
    {
      qDebug() << "cannot open status for" << pid << statusFile.errorString();
      continue;
    }

    const QByteArray statusData = statusFile.readAll();
    statusFile.close();

    uid_t ownerUid = 0;
    const QStringList statusLines = QString::fromLocal8Bit(statusData).split('\n', Qt::SkipEmptyParts);
    for (const QString &line : statusLines)
    {
      if (line.startsWith("Uid:"))
      {
        const QStringList uidParts = line.mid(4).trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (!uidParts.isEmpty())
          ownerUid = static_cast<uid_t>(uidParts.first().toInt());
        break;
      }
    }

    if (ownerUid != currentUid)
    {
      qDebug() << "pid" << pid << "skipping ownerUid" << ownerUid << "currentUid" << currentUid;
      continue;
    }

    QString appName;
    QFile cmdlineFile(entry.filePath() + "/cmdline");
    if (cmdlineFile.open(QIODevice::ReadOnly))
    {
      const QByteArray cmdlineData = cmdlineFile.readAll();
      cmdlineFile.close();
      const QStringList parts = QString::fromLocal8Bit(cmdlineData).split(QLatin1Char('\0'), Qt::SkipEmptyParts);
      qDebug() << "pid" << pid << "cmdline raw" << cmdlineData << "parts" << parts;
      if (!parts.isEmpty())
        appName = QFileInfo(parts.first()).fileName();
    }

    if (appName.isEmpty())
    {
      QFile commFile(entry.filePath() + "/comm");
      if (commFile.open(QIODevice::ReadOnly))
      {
        appName = QString::fromLocal8Bit(commFile.readAll()).trimmed();
        commFile.close();
        qDebug() << "pid" << pid << "comm name" << appName;
      }
    }

    qDebug() << "pid" << pid << "appName" << appName;
    if (appName.isEmpty())
      continue;

    if (isExcludedWaylandClient(appName))
    {
      qDebug() << "pid" << pid << "excluded" << appName;
      continue;
    }

    applications.append(appName);
  }

  applications.removeDuplicates();
  applications.sort();
  qDebug() << "collectWaylandApplications found" << applications.size() << "apps";
  return applications;
}

static QStringList collectWaylandApplications(const QString &currentUser)
{
  if (!qEnvironmentVariable("SWAYSOCK").isEmpty())
    return collectSwayApplications();

  return collectGenericWaylandApplications(currentUser);
}

QStringList SystemDataProvider::refreshApplications()
{
  const QString displayType = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
  const bool isWayland = displayType == "wayland" || !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty();
  const bool isX11 = displayType == "x11" || displayType == "xorg";

  if (isX11)
  {
    QStringList applications;
    QProcess process;
    process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    process.start("bash", QStringList() << "-lc" << R"(xlsclients | awk '{print $2}' | sort -u)");
    process.waitForFinished();
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    for (const QString &line : output.split('\n', Qt::SkipEmptyParts))
      applications.append(line.trimmed());
    applications.removeDuplicates();
    applications.sort();
    return applications;
  }
  else if (isWayland)
  {
    return collectWaylandApplications(m_currentUser);
  }

  return QStringList{"Unknown display server"};
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

  qint64 memTotal = -1;
  qint64 memAvailable = -1;
  qint64 memFree = -1;
  qint64 buffers = 0;
  qint64 cached = 0;
  qint64 sReclaimable = 0;
  qint64 shmem = 0;

  QFile memFile("/proc/meminfo");
  if (memFile.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    // Dump raw meminfo content for debugging
    const QByteArray rawMem = memFile.readAll();
    memFile.seek(0);
    QTextStream stream(&memFile);
    QFile rawDbg("/tmp/wintaskman_meminfo_raw.log");
    if (rawDbg.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      rawDbg.write(rawMem);
      rawDbg.close();
    }

    while (!stream.atEnd())
    {
      const QString line = stream.readLine().trimmed();
      const QStringList fields = line.split(':', Qt::SkipEmptyParts);
      if (fields.size() < 2)
        continue;

      const QString key = fields[0].trimmed();
      const QStringList tokens = fields[1].trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
      const qint64 value = tokens.value(0).toLongLong();

      if (key == QLatin1String("MemTotal"))
        memTotal = value;
      else if (key == QLatin1String("MemAvailable"))
        memAvailable = value;
      else if (key == QLatin1String("MemFree"))
        memFree = value;
      else if (key == QLatin1String("Buffers"))
        buffers = value;
      else if (key == QLatin1String("Cached"))
        cached = value;
      else if (key == QLatin1String("SReclaimable"))
        sReclaimable = value;
      else if (key == QLatin1String("Shmem"))
        shmem = value;

      if (memTotal > 0 && memAvailable > 0)
        break;
    }
    memFile.close();

    qDebug() << "meminfo parsed:" << "MemTotal=" << memTotal << "MemAvailable=" << memAvailable
             << "MemFree=" << memFree << "Buffers=" << buffers << "Cached=" << cached
             << "SReclaimable=" << sReclaimable << "Shmem=" << shmem;
    QFile dbgFile("/tmp/wintaskman_meminfo_debug.log");
    if (dbgFile.open(QIODevice::Append | QIODevice::Text))
    {
      QTextStream ts(&dbgFile);
      ts << "meminfo parsed: MemTotal=" << memTotal << " MemAvailable=" << memAvailable
         << " MemFree=" << memFree << " Buffers=" << buffers << " Cached=" << cached
         << " SReclaimable=" << sReclaimable << " Shmem=" << shmem << "\n";
      dbgFile.close();
    }

    if (memTotal <= 0)
    {
      const long pageSizeKb = sysconf(_SC_PAGESIZE) / 1024;
      const long physPages = sysconf(_SC_PHYS_PAGES);
      if (pageSizeKb > 0 && physPages > 0)
        memTotal = static_cast<qint64>(physPages) * pageSizeKb;
    }

    if (memTotal > 0)
    {
      usage.totalRam = memTotal;
      if (memAvailable > 0)
      {
        usage.ramUsage = memTotal - memAvailable;
      }
      else if (memFree >= 0)
      {
        qint64 availableEstimate = memFree + buffers + cached;
        if (sReclaimable > 0)
          availableEstimate += sReclaimable;
        if (shmem > 0)
          availableEstimate -= shmem;
        usage.ramUsage = qMax<qint64>(0, memTotal - availableEstimate);
      }
    }

    qDebug() << "readSystemUsage:" << usage.totalRam << "kB total," << usage.ramUsage << "kB used";
  }

  // Use sysinfo() when available as a reliable source for total/free RAM. Prefer MemAvailable if parsed.
  struct sysinfo si;
  if (sysinfo(&si) == 0)
  {
    const qint64 totalKb_sys = (static_cast<qint64>(si.totalram) * si.mem_unit) / 1024;
    const qint64 freeKb_sys = (static_cast<qint64>(si.freeram) * si.mem_unit) / 1024;
    // Ensure totalRam is set
    if (usage.totalRam <= 0)
      usage.totalRam = totalKb_sys;

    // Prefer MemAvailable-based usage when available, otherwise use sysinfo values
    if (memAvailable > 0 && memTotal > 0)
      usage.ramUsage = memTotal - memAvailable;
    else
      usage.ramUsage = qMax<qint64>(0, usage.totalRam - freeKb_sys);

    qDebug() << "sysinfo primary: totalKb" << totalKb_sys << "freeKb" << freeKb_sys << "usedKb" << usage.ramUsage;
    QFile dbgFile2("/tmp/wintaskman_meminfo_debug.log");
    if (dbgFile2.open(QIODevice::Append | QIODevice::Text))
    {
      QTextStream ts(&dbgFile2);
      ts << "sysinfo primary: totalKb=" << totalKb_sys << " freeKb=" << freeKb_sys << " usedKb=" << usage.ramUsage << "\n";
      dbgFile2.close();
    }
  }

  return usage;
}
