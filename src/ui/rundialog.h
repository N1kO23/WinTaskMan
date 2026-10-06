#pragma once

#include <QDialog>

class QLineEdit;

// File > Run new task: starts a program, or opens a file, folder or URL.
class RunDialog : public QDialog
{
  Q_OBJECT

public:
  explicit RunDialog(QWidget *parent = nullptr);

private:
  void browse();
  void run();

  QLineEdit *m_commandInput = nullptr;
};
