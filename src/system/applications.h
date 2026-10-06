#pragma once

#include <QByteArray>
#include <QStringList>

// Lists the applications running in the current graphical session: xlsclients on X11,
// swaymsg on sway and a /proc scan on other Wayland compositors. Blocks while it runs.
QStringList listApplications();

// Parses `xlsclients` output ("host  command args") into sorted, unique command names.
QStringList parseXlsclients(const QByteArray &output);
// Parses `swaymsg -t get_tree` output into the sorted, unique names of visible windows.
QStringList parseSwayTree(const QByteArray &json);
