#pragma once

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>

// Runs a function on Qt's global thread pool and hands its result to a callback on the
// receiver's thread. If the owner is destroyed mid-run the result is dropped, but the function
// still runs to completion, so it must not capture anything the owner might delete.
template <typename T>
class BackgroundJob
{
public:
  BackgroundJob(QObject *receiver, std::function<void(const T &)> onFinished)
  {
    QObject::connect(&m_watcher, &QFutureWatcher<T>::finished, receiver,
                     [this, onFinished = std::move(onFinished)]
                     { onFinished(m_watcher.result()); });
  }

  // Starts `function` unless the previous run is still in progress.
  template <typename Function>
  void start(Function function)
  {
    if (!m_watcher.isRunning())
      m_watcher.setFuture(QtConcurrent::run(std::move(function)));
  }

private:
  QFutureWatcher<T> m_watcher;
};
