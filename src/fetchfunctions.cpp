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

void getSystemUsage(int &cpuUsage, int &ramUsage, int &totalRam, int &cpuCoreCount, QList<int> &coreUsages)
{
    cpuCoreCount = 0;
    QFile cpuFile("/proc/stat");
    if (!cpuFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QTextStream cpuStream(&cpuFile);
    QStringList cpuLines;
    bool keepGoing = true;
    while (keepGoing) {
        QString line = cpuStream.readLine();
        if (line.startsWith("cpu")) {
            bool isCore = line.at(3) != QChar(' '); // Check if it's a core line (cpu0, cpu1, etc.)
            if (isCore) {
                cpuCoreCount++;
            }
            cpuLines.append(line);
        } else {
          keepGoing = false;
        }
    }
    cpuFile.close();

    // Get total CPU and per-core usage
    static QList<int> prevTotalCpu;
    static QList<int> prevCpuIdle;

    int totalCpuUsage = 0;
    coreUsages.clear();
    if (prevTotalCpu.isEmpty()) {
        prevTotalCpu.fill(0, cpuLines.size());
        prevCpuIdle.fill(0, cpuLines.size());
    }
    
    for (int i = 0; i < cpuLines.size(); ++i) {
        if (!cpuLines[i].startsWith("cpu")) {
            break;
        }
        
        QStringList cpuValues = cpuLines[i].split(" ", Qt::SkipEmptyParts);
        if (cpuValues.size() < 9) continue;
        
        int cpuUser = cpuValues[1].toInt();
        int cpuNice = cpuValues[2].toInt();
        int cpuSystem = cpuValues[3].toInt();
        int cpuIdle = cpuValues[4].toInt();
        int cpuIowait = cpuValues[5].toInt();
        int cpuIrq = cpuValues[6].toInt();
        int cpuSoftirq = cpuValues[7].toInt();
        int cpuSteal = cpuValues[8].toInt();
        
        int totalCpu = cpuUser + cpuNice + cpuSystem + cpuIdle + cpuIowait + cpuIrq + cpuSoftirq + cpuSteal;
        
        if (prevTotalCpu[i] != 0 && prevCpuIdle[i] != 0) {
            int totalDiff = totalCpu - prevTotalCpu[i];
            int idleDiff = cpuIdle - prevCpuIdle[i];
            int usage = (totalDiff - idleDiff) * 100 / totalDiff;
            
            if (i == 0) { // First line represents total CPU usage
                totalCpuUsage = usage;
            } else {
              coreUsages.append(usage);
            }
        } else if (i != 0) {
            coreUsages.append(0);
        }

        prevTotalCpu[i] = totalCpu;
        prevCpuIdle[i] = cpuIdle;
    }

    cpuUsage = totalCpuUsage;

    // Get RAM usage
    QFile memFile("/proc/meminfo");
    if (!memFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QTextStream memStream(&memFile);
    QString memTotalLine = memStream.readLine();
    QString memAvailableLine = memStream.readLine();
    memFile.close();

    QStringList memTotalValues = memTotalLine.split(" ", Qt::SkipEmptyParts);
    QStringList memAvailableValues = memAvailableLine.split(" ", Qt::SkipEmptyParts);
    int memTotal = memTotalValues[1].toInt();
    int memAvailable = memAvailableValues[1].toInt();
    ramUsage = memTotal - memAvailable;
    totalRam = memTotal;
}
