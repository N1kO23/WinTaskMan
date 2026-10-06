#include "network.h"
#include "procfs.h"
#include "runtool.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace
{
QByteArray readSysFile(const QString &path)
{
  return readProcFile(path).trimmed();
}

qint64 linkSpeed(const QString &adapter, const QString &directory, bool connected)
{
  // Wired drivers report the speed in sysfs. Wi-Fi drivers don't, but NetworkManager knows it.
  const qint64 speed = readSysFile(directory + QLatin1String("speed")).toLongLong();
  if (speed > 0 || !connected)
    return qMax<qint64>(0, speed);

  const QStringList arguments = {QStringLiteral("-g"), QStringLiteral("CAPABILITIES.SPEED"),
                                 QStringLiteral("device"), QStringLiteral("show"), adapter};
  return parseNmcliSpeed(runTool(QStringLiteral("nmcli"), arguments, 1000).value_or(QByteArray()));
}
} // namespace

NetworkSnapshot readNetworkSnapshot()
{
  NetworkSnapshot snapshot;
  snapshot.uptimeSeconds = parseUptime(readProcFile(QStringLiteral("/proc/uptime")));

  const QDir interfaces(QStringLiteral("/sys/class/net"));
  for (const QString &name : interfaces.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
  {
    const QString directory = interfaces.filePath(name) + QLatin1Char('/');
    if (!QFileInfo::exists(directory + QLatin1String("device")))
      continue; // loopback, bridges, containers and other virtual interfaces

    NetworkAdapter adapter;
    adapter.name = name;
    adapter.connected = isAdapterConnected(readSysFile(directory + QLatin1String("operstate")),
                                           readSysFile(directory + QLatin1String("carrier")));
    adapter.linkSpeedMbps = linkSpeed(name, directory, adapter.connected);
    adapter.bytesReceived = readSysFile(directory + QLatin1String("statistics/rx_bytes")).toULongLong();
    adapter.bytesSent = readSysFile(directory + QLatin1String("statistics/tx_bytes")).toULongLong();
    snapshot.adapters.append(adapter);
  }
  return snapshot;
}

bool isAdapterConnected(const QByteArray &operState, const QByteArray &carrier)
{
  // Some drivers never report "up", only "unknown" with a carrier.
  return operState.trimmed() == "up" || (operState.trimmed() == "unknown" && carrier.trimmed() == "1");
}

qint64 parseNmcliSpeed(const QByteArray &output)
{
  // "1000 Mb/s", or "unknown" when there is no link.
  const QList<QByteArray> fields = output.simplified().split(' ');
  const qint64 speed = fields.value(0).toLongLong();
  return fields.value(1) == "Gb/s" ? speed * 1000 : speed;
}
