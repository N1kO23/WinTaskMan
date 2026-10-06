#include "applications.h"
#include "procfs.h"
#include "runtool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <unistd.h>

namespace
{
// Session infrastructure that talks to the compositor but isn't an application.
bool isExcludedWaylandClient(const QString &name)
{
  static const QSet<QString> excluded = {
      "xwayland", "wayland", "gnome-shell", "kwin_wayland", "plasmashell",
      "sway", "weston", "waybar", "pipewire", "wireplumber",
      "xdg-desktop-portal", "xdg-desktop-portal-wlr", "dbus-daemon",
      "systemd", "bash", "sh", "zsh", "fish", "login", "loginctl",
      "gnome-session", "ksmserver", "kded5", "autostart"};
  return excluded.contains(name.toLower());
}

QStringList sortedUnique(QStringList names)
{
  names.removeDuplicates();
  names.sort();
  return names;
}

// Whether the process has a file open whose path mentions Wayland, such as the shared memory
// libwayland-cursor allocates ("/memfd:wayland-cursor").
bool hasOpenWaylandFile(int pid)
{
  const QFileInfoList descriptors = QDir(QStringLiteral("/proc/%1/fd").arg(pid)).entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
  for (const QFileInfo &descriptor : descriptors)
  {
    const QString target = QFile::symLinkTarget(descriptor.filePath());
    if (target.contains(QLatin1String("wayland-"), Qt::CaseInsensitive))
      return true;
    if (target.contains(QLatin1String("/run/user/"), Qt::CaseInsensitive) && target.contains(QLatin1String("wayland"), Qt::CaseInsensitive))
      return true;
  }
  return false;
}

// The executable's file name from the command line, falling back to the kernel's name for it.
QString applicationName(const QString &procDirectory)
{
  const QStringList arguments = QString::fromLocal8Bit(readProcFile(procDirectory + QLatin1String("cmdline")))
                                    .split(QLatin1Char('\0'), Qt::SkipEmptyParts);
  if (!arguments.isEmpty())
  {
    const QString name = QFileInfo(arguments.first()).fileName();
    if (!name.isEmpty())
      return name;
  }
  return QString::fromLocal8Bit(readProcFile(procDirectory + QLatin1String("comm"))).trimmed();
}

// Wayland has no way to list another client's windows, so this lists the current user's
// processes that were started in the Wayland session and have a Wayland file open.
QStringList listWaylandClients()
{
  const uint currentUid = geteuid();
  QStringList applications;
  for (const int pid : listPids())
  {
    const QString directory = QStringLiteral("/proc/%1/").arg(pid);
    if (parseStatusUid(readProcFile(directory + QLatin1String("status"))) != currentUid)
      continue;

    const QByteArray environment = readProcFile(directory + QLatin1String("environ"));
    if (!environment.contains("WAYLAND_DISPLAY=") && !environment.contains("WAYLAND_SOCKET="))
      continue;
    if (!hasOpenWaylandFile(pid))
      continue;

    const QString name = applicationName(directory);
    if (!name.isEmpty() && !isExcludedWaylandClient(name))
      applications.append(name);
  }
  return sortedUnique(applications);
}

void collectSwayApplications(const QJsonObject &node, QStringList &applications)
{
  const QString type = node.value(QLatin1String("type")).toString();
  const bool isVisibleWindow = (type == QLatin1String("con") || type == QLatin1String("floating_con"))
                               && !node.value(QLatin1String("window")).isNull()
                               && node.value(QLatin1String("visible")).toBool();
  if (isVisibleWindow)
  {
    // X11 windows carry their class in window_properties; native Wayland windows have an app_id.
    const QJsonObject properties = node.value(QLatin1String("window_properties")).toObject();
    QString name = properties.value(QLatin1String("class")).toString();
    if (name.isEmpty())
      name = properties.value(QLatin1String("instance")).toString();
    if (name.isEmpty())
      name = node.value(QLatin1String("app_id")).toString();
    if (name.isEmpty())
      name = node.value(QLatin1String("name")).toString();

    if (!name.isEmpty() && !isExcludedWaylandClient(name))
      applications.append(name);
  }

  for (const QJsonValue &child : node.value(QLatin1String("nodes")).toArray())
    collectSwayApplications(child.toObject(), applications);
  for (const QJsonValue &child : node.value(QLatin1String("floating_nodes")).toArray())
    collectSwayApplications(child.toObject(), applications);
}
} // namespace

QStringList listApplications()
{
  const QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
  if (sessionType == QLatin1String("x11") || sessionType == QLatin1String("xorg"))
    return parseXlsclients(runTool(QStringLiteral("xlsclients"), {}).value_or(QByteArray()));

  if (sessionType == QLatin1String("wayland") || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
  {
    if (!qEnvironmentVariableIsEmpty("SWAYSOCK"))
      return parseSwayTree(runTool(QStringLiteral("swaymsg"), {QStringLiteral("-t"), QStringLiteral("get_tree")}, 1000).value_or(QByteArray()));
    return listWaylandClients();
  }

  return {QStringLiteral("Unknown display server")};
}

QStringList parseXlsclients(const QByteArray &output)
{
  QStringList applications;
  for (const QByteArray &line : output.split('\n'))
  {
    const QList<QByteArray> fields = line.simplified().split(' ');
    if (fields.size() >= 2)
      applications.append(QString::fromLocal8Bit(fields[1]));
  }
  return sortedUnique(applications);
}

QStringList parseSwayTree(const QByteArray &json)
{
  const QJsonDocument document = QJsonDocument::fromJson(json);
  if (!document.isObject())
    return {};

  QStringList applications;
  collectSwayApplications(document.object(), applications);
  return sortedUnique(applications);
}
