#include <QDir>
#include <QTextStream>
#include <QStringList>
#include "fetchfunctions.h"

int getTotalProcesses()
{
  QDir procDir("/proc");
  QStringList procEntries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  int processCount = 0;

  foreach (QString entry, procEntries)
  {
    if (entry.toInt())
    {
      processCount++;
    }
  }

  return processCount;
}
void getSystemUsage(int &cpuUsage, int &ramUsage)
{
  // Get total CPU usage
  static int prevTotalCpu = 0;
  static int prevCpuIdle = 0;

  QFile cpuFile("/proc/stat");
  if (!cpuFile.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    // qDebug() << "Failed to open /proc/stat";
    return;
  }
  QTextStream cpuStream(&cpuFile);
  QString cpuLine = cpuStream.readLine();
  cpuFile.close();

  QStringList cpuValues = cpuLine.split(" ", Qt::SkipEmptyParts);
  int cpuUser = cpuValues[1].toInt();
  int cpuNice = cpuValues[2].toInt();
  int cpuSystem = cpuValues[3].toInt();
  int cpuIdle = cpuValues[4].toInt();
  int cpuIowait = cpuValues[5].toInt();
  int cpuIrq = cpuValues[6].toInt();
  int cpuSoftirq = cpuValues[7].toInt();
  int cpuSteal = cpuValues[8].toInt();

  int totalCpu = cpuUser + cpuNice + cpuSystem + cpuIdle + cpuIowait + cpuIrq + cpuSoftirq + cpuSteal;

  if (prevTotalCpu != 0 && prevCpuIdle != 0)
  {
    int totalDiff = totalCpu - prevTotalCpu;
    int idleDiff = cpuIdle - prevCpuIdle;

    cpuUsage = (totalDiff - idleDiff) * 100 / totalDiff;
  }

  prevTotalCpu = totalCpu;
  prevCpuIdle = cpuIdle;

  // Get RAM usage
  QFile memFile("/proc/meminfo");
  memFile.open(QIODevice::ReadOnly | QIODevice::Text);
  QTextStream memStream(&memFile);
  QString memTotalLine = memStream.readLine();
  QString memAvailableLine = memStream.readLine();
  memFile.close();

  QStringList memTotalValues = memTotalLine.split(" ", Qt::SkipEmptyParts);
  QStringList memAvailableValues = memAvailableLine.split(" ", Qt::SkipEmptyParts);
  int memTotal = memTotalValues[1].toInt();
  int memAvailable = memAvailableValues[1].toInt();
  ramUsage = memTotal - memAvailable;
}