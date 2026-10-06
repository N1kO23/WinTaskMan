#include "system/applications.h"
#include "system/network.h"
#include "system/procfs.h"
#include "system/services.h"
#include "system/trackers.h"

#include <QTest>

class TestSystem : public QObject
{
  Q_OBJECT

private slots:
  void parsesCpuTimes();
  void parsesMemInfo();
  void parsesUptimeThreadsAndHandles();
  void parsesProcessStat();
  void rejectsMalformedProcessStat();
  void parsesStatusUid();
  void parsesCmdline();
  void tracksSystemUsage();
  void tracksProcessUsage();
  void parsesAdapterState();
  void tracksNetworkUsage();
  void parsesSystemdUnits();
  void parsesOpenRcStatus();
  void parsesXlsclients();
  void parsesSwayTree();
};

void TestSystem::parsesCpuTimes()
{
  const QByteArray procStat = "cpu  100 20 30 800 50 5 10 15 7 3\n"
                              "cpu0 60 10 20 400 10 0 0 0 0 0\n"
                              "cpu1 40 10 10 400 40 5 10 15 7 3\n"
                              "intr 1234 5 6\n"
                              "ctxt 99\n";

  const QList<CpuTimes> cpus = parseCpuTimes(procStat);
  QCOMPARE(cpus.size(), 3);
  // Total excludes guest time, which is already counted in user and nice; idle includes iowait.
  QCOMPARE(cpus[0].total, quint64(1030));
  QCOMPARE(cpus[0].busy, quint64(180));
  QCOMPARE(cpus[0].kernel, quint64(45)); // system + irq + softirq
  QCOMPARE(cpus[1].total, quint64(500));
  QCOMPARE(cpus[1].busy, quint64(90));
  QCOMPARE(cpus[1].kernel, quint64(20));
  QCOMPARE(cpus[2].total, quint64(530));
  QCOMPARE(cpus[2].busy, quint64(90));
  QCOMPARE(cpus[2].kernel, quint64(25));

  QVERIFY(parseCpuTimes(QByteArray()).isEmpty());
}

void TestSystem::parsesMemInfo()
{
  const QByteArray memInfo = "MemTotal:       16318444 kB\n"
                             "MemFree:         1018276 kB\n"
                             "MemAvailable:    9876543 kB\n"
                             "Buffers:          123456 kB\n"
                             "Cached:          7654321 kB\n"
                             "SwapCached:         1024 kB\n"
                             "SReclaimable:     562356 kB\n"
                             "SUnreclaim:       424140 kB\n"
                             "KernelStack:       41904 kB\n"
                             "PageTables:       128468 kB\n"
                             "CommitLimit:    49543528 kB\n"
                             "Committed_AS:   31193348 kB\n"
                             "HugePages_Total:       0\n";

  const MemoryInfo info = parseMemInfo(memInfo);
  QCOMPARE(info.totalKb, qint64(16318444));
  QCOMPARE(info.availableKb, qint64(9876543));
  QCOMPARE(info.freeKb, qint64(1018276));
  QCOMPARE(info.buffersKb, qint64(123456));
  QCOMPARE(info.cachedKb, qint64(7654321)); // not SwapCached
  QCOMPARE(info.reclaimableSlabKb, qint64(562356));
  QCOMPARE(info.unreclaimableSlabKb, qint64(424140));
  QCOMPARE(info.kernelStackKb, qint64(41904));
  QCOMPARE(info.pageTablesKb, qint64(128468));
  QCOMPARE(info.committedKb, qint64(31193348));
  QCOMPARE(info.commitLimitKb, qint64(49543528));
}

void TestSystem::parsesUptimeThreadsAndHandles()
{
  QCOMPARE(parseUptime("18503.08 431216.28\n"), 18503.08);
  QCOMPARE(parseThreadCount("2.02 1.82 1.65 1/2596 219514\n"), 2596);
  QCOMPARE(parseHandleCount("19015\t0\t3183594\n"), qint64(19015));

  QCOMPARE(parseUptime(QByteArray()), 0.0);
  QCOMPARE(parseThreadCount(QByteArray()), 0);
  QCOMPARE(parseHandleCount(QByteArray()), qint64(0));
}

void TestSystem::parsesProcessStat()
{
  // The command name contains spaces and parentheses; utime is 250, stime 150,
  // starttime 98765 and rss 4321 pages.
  const QByteArray stat = "1234 (my (odd) proc) S 1 1234 1234 0 -1 4194560 1000 0 0 0 250 150 0 0 20 0 3 0 "
                          "98765 123456789 4321 18446744073709551615 1 1 0 0 0 0 0 0 0 0 0 0 17 3 0 0 0 0 0\n";

  const std::optional<ProcessStat> parsed = parseProcessStat(stat);
  QVERIFY(parsed.has_value());
  QCOMPARE(parsed->comm, QStringLiteral("my (odd) proc"));
  QCOMPARE(parsed->cpuTicks, quint64(400));
  QCOMPARE(parsed->startTicks, quint64(98765));
  QCOMPARE(parsed->rssPages, qint64(4321));
}

void TestSystem::rejectsMalformedProcessStat()
{
  QVERIFY(!parseProcessStat(QByteArray()).has_value());
  QVERIFY(!parseProcessStat("1234 no parentheses here").has_value());
  QVERIFY(!parseProcessStat("1234 (truncated) S 1 2 3").has_value());
}

void TestSystem::parsesStatusUid()
{
  const QByteArray status = "Name:\tbash\n"
                            "Umask:\t0022\n"
                            "State:\tS (sleeping)\n"
                            "Uid:\t1000\t1001\t1002\t1003\n"
                            "Gid:\t100\t100\t100\t100\n";

  QCOMPARE(parseStatusUid(status), std::optional<uint>(1000)); // the real uid
  QVERIFY(!parseStatusUid("Name:\tbash\n").has_value());
}

void TestSystem::parsesCmdline()
{
  const char cmdline[] = "/usr/bin/foo\0--bar\0baz qux\0";
  QCOMPARE(parseCmdline(QByteArray(cmdline, sizeof(cmdline) - 1)), QStringLiteral("/usr/bin/foo --bar baz qux"));
  QVERIFY(parseCmdline(QByteArray()).isEmpty());
}

void TestSystem::tracksSystemUsage()
{
  SystemUsageTracker tracker;

  UsageSnapshot first;
  first.cpus = {{100, 40, 1000}, {50, 20, 500}, {50, 20, 500}}; // busy, kernel, total
  first.memory.totalKb = 1000;
  first.memory.availableKb = 250;
  first.memory.freeKb = 100;
  first.processCount = 42;
  first.threadCount = 300;
  first.handleCount = 5000;
  first.uptimeSeconds = 3600.0;
  const SystemUsage initial = tracker.update(first);
  QCOMPARE(initial.cpuPercent, 0.0); // no baseline yet
  QCOMPARE(initial.kernelPercent, 0.0);
  QCOMPARE(initial.corePercents, QList<double>({0.0, 0.0}));
  QCOMPARE(initial.coreKernelPercents, QList<double>({0.0, 0.0}));
  QCOMPARE(initial.memoryPercent, 75.0);
  QCOMPARE(initial.memory.freeKb, qint64(100));
  QCOMPARE(initial.processCount, 42);
  QCOMPARE(initial.threadCount, 300);
  QCOMPARE(initial.handleCount, qint64(5000));
  QCOMPARE(initial.uptimeSeconds, 3600.0);

  UsageSnapshot second = first;
  second.cpus = {{150, 60, 1200}, {90, 30, 600}, {60, 25, 600}};
  const SystemUsage usage = tracker.update(second);
  QCOMPARE(usage.cpuPercent, 25.0);
  QCOMPARE(usage.kernelPercent, 10.0);
  QCOMPARE(usage.corePercents, QList<double>({40.0, 10.0}));
  QCOMPARE(usage.coreKernelPercents, QList<double>({10.0, 5.0}));

  // A different number of CPUs (hotplug) starts over.
  UsageSnapshot third = second;
  third.cpus = {{200, 70, 1400}, {100, 35, 700}};
  const SystemUsage restarted = tracker.update(third);
  QCOMPARE(restarted.cpuPercent, 0.0);
  QCOMPARE(restarted.corePercents, QList<double>({0.0}));
}

void TestSystem::tracksProcessUsage()
{
  ProcessUsageTracker tracker(100, 2); // 100 ticks per second, 2 CPUs

  const auto sample = [](int pid, quint64 startTicks, quint64 cpuTicks)
  {
    ProcessSample process;
    process.pid = pid;
    process.uid = 1000;
    process.user = QStringLiteral("alice");
    process.name = QStringLiteral("process %1").arg(pid);
    process.startTicks = startTicks;
    process.cpuTicks = cpuTicks;
    process.memoryKb = 2048;
    return process;
  };

  ProcessSnapshot first;
  first.uptimeSeconds = 100.0;
  first.processes = {sample(10, 500, 1000), sample(11, 600, 50)};
  for (const ProcessInfo &process : tracker.update(first))
    QCOMPARE(process.cpuPercent, 0.0); // first sighting

  ProcessSnapshot second;
  second.uptimeSeconds = 102.0;
  second.processes = {sample(10, 500, 1200), // 2 CPU seconds in 2 seconds, on 2 CPUs
                      sample(11, 700, 60),   // pid 11 was reused by a new process
                      sample(12, 800, 30)};  // new process
  const QList<ProcessInfo> processes = tracker.update(second);
  QCOMPARE(processes.size(), 3);
  QCOMPARE(processes[0].pid, 10);
  QCOMPARE(processes[0].cpuPercent, 50.0);
  QCOMPARE(processes[0].uid, uint(1000));
  QCOMPARE(processes[0].user, QStringLiteral("alice"));
  QCOMPARE(processes[0].name, QStringLiteral("process 10"));
  QCOMPARE(processes[0].memoryKb, qint64(2048));
  QCOMPARE(processes[1].cpuPercent, 0.0);
  QCOMPARE(processes[2].cpuPercent, 0.0);
}

void TestSystem::parsesAdapterState()
{
  QVERIFY(isAdapterConnected("up\n", "1\n"));
  QVERIFY(!isAdapterConnected("down\n", "0\n"));
  QVERIFY(!isAdapterConnected("dormant\n", "1\n")); // Wi-Fi still authenticating
  QVERIFY(isAdapterConnected("unknown\n", "1\n"));
  QVERIFY(!isAdapterConnected("unknown\n", ""));

  QCOMPARE(parseNmcliSpeed("1000 Mb/s\n"), qint64(1000));
  QCOMPARE(parseNmcliSpeed("866 Mb/s\n"), qint64(866));
  QCOMPARE(parseNmcliSpeed("unknown\n"), qint64(0));
  QCOMPARE(parseNmcliSpeed(QByteArray()), qint64(0));
}

void TestSystem::tracksNetworkUsage()
{
  NetworkUsageTracker tracker;
  const auto adapter = [](const QString &name, qint64 mbps, quint64 received, quint64 sent)
  {
    NetworkAdapter result;
    result.name = name;
    result.connected = mbps > 0;
    result.linkSpeedMbps = mbps;
    result.bytesReceived = received;
    result.bytesSent = sent;
    return result;
  };

  NetworkSnapshot first;
  first.uptimeSeconds = 100.0;
  first.adapters = {adapter("eth0", 100, 1'000'000, 500'000), adapter("wlan0", 0, 0, 0)};
  for (const NetworkUsage &usage : tracker.update(first))
    QCOMPARE(usage.totalPercent, 0.0); // first sighting

  // A 100 Mbps link moves 12.5 MB per second; over 2 seconds that is 25 MB.
  NetworkSnapshot second;
  second.uptimeSeconds = 102.0;
  second.adapters = {adapter("eth0", 100, 1'000'000 + 5'000'000, 500'000 + 2'500'000), adapter("wlan0", 0, 10, 10)};
  const QList<NetworkUsage> usages = tracker.update(second);
  QCOMPARE(usages.size(), 2);
  QCOMPARE(usages[0].name, QStringLiteral("eth0"));
  QCOMPARE(usages[0].receivedPercent, 20.0);
  QCOMPARE(usages[0].sentPercent, 10.0);
  QCOMPARE(usages[0].totalPercent, 30.0);
  QCOMPARE(usages[0].linkSpeedMbps, qint64(100));
  QVERIFY(usages[0].connected);
  QCOMPARE(usages[1].totalPercent, 0.0); // unknown link speed
  QVERIFY(!usages[1].connected);

  // Counters that went backwards (the driver reset them) count as no traffic.
  NetworkSnapshot third;
  third.uptimeSeconds = 103.0;
  third.adapters = {adapter("eth0", 100, 0, 0)};
  QCOMPARE(tracker.update(third).value(0).totalPercent, 0.0);
}

void TestSystem::parsesSystemdUnits()
{
  const QByteArray json = R"([
    {"unit":"pipewire.service","load":"loaded","active":"active","sub":"running","description":"PipeWire Multimedia Service"},
    {"unit":"foo.service","load":"not-found","active":"inactive","sub":"dead","description":"foo.service"}
  ])";

  const std::optional<QList<ServiceInfo>> services = parseSystemdUnits(json);
  QVERIFY(services.has_value());
  QCOMPARE(services->size(), 2);
  QCOMPARE(services->at(0).name, QStringLiteral("pipewire.service"));
  QCOMPARE(services->at(0).state, QStringLiteral("active"));
  QCOMPARE(services->at(0).description, QStringLiteral("PipeWire Multimedia Service"));
  QCOMPARE(services->at(0).pid, 0);
  QCOMPARE(services->at(1).state, QStringLiteral("inactive"));

  QVERIFY(parseSystemdUnits("[]").has_value());
  QVERIFY(!parseSystemdUnits("Failed to connect to bus: No medium found").has_value());
}

void TestSystem::parsesOpenRcStatus()
{
  const QByteArray output = "Runlevel: sysinit\n"
                            " devfs                                     [  started  ]\n"
                            "Runlevel: shutdown\n"
                            " killprocs                                 [  stopped  ]\n"
                            "Runlevel: nonetwork\n"
                            " local                                     [  started  ]\n"
                            "Runlevel: default\n"
                            " local                                     [  started  ]\n"
                            "Dynamic Runlevel: hotplugged\n"
                            " user.alice                     [  started 04:15:59 (0) ]\n"
                            "Dynamic Runlevel: manual\n";

  const QList<ServiceInfo> services = parseOpenRcStatus(output);
  QStringList names;
  QStringList states;
  for (const ServiceInfo &service : services)
  {
    names.append(service.name);
    states.append(service.state);
    QCOMPARE(service.pid, 0);
  }
  QCOMPARE(names, QStringList({"devfs", "killprocs", "local", "local", "user.alice"}));
  QCOMPARE(states, QStringList({"started", "stopped", "started", "started", "started"}));
}

void TestSystem::parsesXlsclients()
{
  const QByteArray output = "laptop  xterm\n"
                            "laptop  firefox -P default\n"
                            "laptop  xterm\n"
                            "\n"
                            "lonely\n";

  QCOMPARE(parseXlsclients(output), QStringList({"firefox", "xterm"}));
}

void TestSystem::parsesSwayTree()
{
  const QByteArray json = R"({"type": "root", "nodes": [{"type": "output", "nodes": [{"type": "workspace",
    "nodes": [
      {"type": "con", "visible": true, "app_id": "foot", "name": "~"},
      {"type": "con", "visible": false, "app_id": "hidden"},
      {"type": "con", "visible": true, "window": 4194311,
       "window_properties": {"class": "Firefox", "instance": "Navigator"}}
    ],
    "floating_nodes": [
      {"type": "floating_con", "visible": true, "app_id": "pavucontrol"},
      {"type": "floating_con", "visible": true, "app_id": "waybar"}
    ]}]}]})";

  QCOMPARE(parseSwayTree(json), QStringList({"Firefox", "foot", "pavucontrol"}));
  QVERIFY(parseSwayTree("not json").isEmpty());
}

QTEST_GUILESS_MAIN(TestSystem)
#include "tst_system.moc"
