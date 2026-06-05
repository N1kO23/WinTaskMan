#pragma once

#include <QList>
#include <QMap>
#include <QVector>
#include <QString>

struct ProcessInfo
{
  int pid = 0;
  QString name;
  QString user;
  double cpuPercent = 0.0;
  double memoryKb = 0.0;
};

struct ServiceInfo
{
  QString name;
  QString pid;
  QString description;
  QString state;
};

struct SystemUsage
{
  int cpuUsage = 0;
  int ramUsage = 0;
  int totalRam = 0;
  int coreCount = 0;
  QVector<int> coreUsages;
  int totalProcesses = 0;
};

class SystemDataProvider
{
public:
  SystemDataProvider();

  QString currentUser() const;
  SystemUsage refreshSystemUsage();
  QList<ProcessInfo> refreshProcessList(bool includeAllUsers);
  QList<ServiceInfo> refreshServices();
  QStringList refreshApplications();

private:
  QString m_currentUser;
  QVector<qint64> m_previousCpuTotals;
  QVector<qint64> m_previousCpuIdles;
  QMap<int, qint64> m_previousProcessCpu;
  QMap<int, double> m_previousProcessTime;

  SystemUsage readSystemUsage();
};
