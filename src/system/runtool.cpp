#include "runtool.h"

#include <QProcess>
#include <QStandardPaths>

std::optional<QByteArray> runTool(const QString &program, const QStringList &arguments, int timeoutMs)
{
  const QString path = QStandardPaths::findExecutable(program);
  if (path.isEmpty())
    return std::nullopt;

  QProcess process;
  process.start(path, arguments);
  if (!process.waitForFinished(timeoutMs) || process.exitStatus() != QProcess::NormalExit)
    return std::nullopt;
  return process.readAllStandardOutput();
}
