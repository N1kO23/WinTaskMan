#pragma once

#include <QWidget>

// A tab that fetches fresh data when asked. MainWindow refreshes the visible tab on every
// update tick and whenever the user switches to it.
class RefreshablePage : public QWidget
{
  Q_OBJECT

public:
  using QWidget::QWidget;

  virtual void refresh() = 0;
};
