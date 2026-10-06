#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

struct ServiceInfo
{
  QString name;
  int pid = 0; // 0 when the service isn't running or its PID is unknown
  QString description;
  QString state;
};

// Lists the systemd user services, or the OpenRC services when systemd isn't available.
// Blocks while the helper tool runs.
QList<ServiceInfo> listServices();

// Parses `systemctl list-units --output=json`; nullopt if the output isn't a JSON array.
std::optional<QList<ServiceInfo>> parseSystemdUnits(const QByteArray &json);
// Parses `rc-status --all`. PIDs are left at 0.
QList<ServiceInfo> parseOpenRcStatus(const QByteArray &output);
