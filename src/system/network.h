#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

struct NetworkAdapter
{
  QString name;
  bool connected = false;
  qint64 linkSpeedMbps = 0; // 0 when unknown
  quint64 bytesReceived = 0;
  quint64 bytesSent = 0;
};

struct NetworkSnapshot
{
  double uptimeSeconds = 0.0;
  QList<NetworkAdapter> adapters;
};

// Reads the network adapters backed by hardware (so not the loopback, bridges or other virtual
// interfaces) from /sys/class/net. Blocks on I/O, keeps no state and is safe to call from any thread.
NetworkSnapshot readNetworkSnapshot();

// Whether an interface is connected, from its operstate and carrier files in /sys/class/net.
bool isAdapterConnected(const QByteArray &operState, const QByteArray &carrier);
// Parses the link speed `nmcli -g CAPABILITIES.SPEED device show` prints ("866 Mb/s"), in Mbps.
qint64 parseNmcliSpeed(const QByteArray &output);
