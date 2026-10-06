#include "services.h"
#include "runtool.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

namespace
{
// OpenRC doesn't report PIDs, so look for the pid file a started service conventionally leaves.
int findOpenRcPid(const QString &service)
{
  static const QStringList pidFiles = {
      QStringLiteral("/run/%1.pid"),
      QStringLiteral("/var/run/%1.pid"),
      QStringLiteral("/run/%1/%1.pid"),
      QStringLiteral("/var/run/%1/%1.pid")};

  for (const QString &pidFile : pidFiles)
  {
    QFile file(pidFile.arg(service));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    const int pid = file.readLine().trimmed().toInt();
    if (pid > 0)
      return pid;
  }
  return 0;
}
} // namespace

QList<ServiceInfo> listServices()
{
  const QStringList systemctlArguments = {QStringLiteral("--user"), QStringLiteral("list-units"),
                                          QStringLiteral("--type=service"), QStringLiteral("--all"),
                                          QStringLiteral("--output=json")};
  if (const std::optional<QByteArray> output = runTool(QStringLiteral("systemctl"), systemctlArguments))
  {
    if (std::optional<QList<ServiceInfo>> services = parseSystemdUnits(*output))
      return *services;
  }

  if (const std::optional<QByteArray> output = runTool(QStringLiteral("rc-status"), {QStringLiteral("--all")}))
  {
    QList<ServiceInfo> services = parseOpenRcStatus(*output);
    for (ServiceInfo &service : services)
    {
      if (service.state.compare(QLatin1String("started"), Qt::CaseInsensitive) == 0)
        service.pid = findOpenRcPid(service.name);
    }
    return services;
  }

  return {};
}

std::optional<QList<ServiceInfo>> parseSystemdUnits(const QByteArray &json)
{
  const QJsonDocument document = QJsonDocument::fromJson(json);
  if (!document.isArray())
    return std::nullopt;

  QList<ServiceInfo> services;
  for (const QJsonValue &value : document.array())
  {
    const QJsonObject unit = value.toObject();
    ServiceInfo service;
    service.name = unit.value(QLatin1String("unit")).toString();
    service.pid = unit.value(QLatin1String("mainPID")).toInt();
    service.description = unit.value(QLatin1String("description")).toString();
    service.state = unit.value(QLatin1String("active")).toString();
    services.append(service);
  }
  return services;
}

QList<ServiceInfo> parseOpenRcStatus(const QByteArray &output)
{
  // " sshd        [  started  ]", where supervised services add their uptime after the
  // state: "[  started 04:15:59 (0) ]". Runlevel headers have no brackets.
  static const QRegularExpression serviceLine(QStringLiteral(R"(^\s*([^\s\[]+)\s+\[\s*([^\s\]]+)[^\]]*\])"));

  QList<ServiceInfo> services;
  const QStringList lines = QString::fromLocal8Bit(output).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  for (const QString &line : lines)
  {
    const QRegularExpressionMatch match = serviceLine.match(line);
    if (!match.hasMatch())
      continue;

    ServiceInfo service;
    service.name = match.captured(1);
    service.state = match.captured(2);
    services.append(service);
  }
  return services;
}
