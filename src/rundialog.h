#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;
class QCheckBox;

class RunDialog : public QDialog
{
  Q_OBJECT

public:
  explicit RunDialog(QWidget *parent = nullptr);
  QString getCommand() const;

private slots:
  void onBrowseClicked();
  void onOkClicked();

private:
  void setupUI();

  QLineEdit *m_commandInput = nullptr;
  QPushButton *m_browseButton = nullptr;
  QPushButton *m_okButton = nullptr;
  QPushButton *m_cancelButton = nullptr;
  QCheckBox *m_restrictedAccessCheckbox = nullptr;
};
