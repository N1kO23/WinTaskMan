#pragma once

#include <QByteArray>
#include <QStringList>

#include <optional>

// Runs a program found on PATH and returns its standard output, or nullopt if the program
// is missing, crashes or doesn't finish within the timeout.
std::optional<QByteArray> runTool(const QString &program, const QStringList &arguments, int timeoutMs = 5000);
