#include "rundialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QIcon>
#include <QPixmap>
#include <QFileDialog>
#include <QProcess>
#include <QStandardPaths>
#include <QSize>
#include <QDesktopServices>
#include <QUrl>
#include <QScreen>
#include <QApplication>

RunDialog::RunDialog(QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle("Run");
  setWindowIcon(QIcon(":/src/assets/icons/taskmgr.ico"));
  setModal(true);
  setMinimumWidth(420);
  setMaximumWidth(500);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setupUI();

  connect(m_browseButton, &QPushButton::clicked, this, &RunDialog::onBrowseClicked);
  connect(m_okButton, &QPushButton::clicked, this, &RunDialog::onOkClicked);
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_commandInput, &QLineEdit::returnPressed, this, &RunDialog::onOkClicked);

  // Center the dialog on the parent window
  if (parent)
  {
    QScreen *screen = QApplication::screenAt(parent->mapToGlobal(QPoint(parent->width() / 2, parent->height() / 2)));
    if (screen)
    {
      move(screen->geometry().center() - rect().center());
    }
  }
}

void RunDialog::setupUI()
{
  // Main layout with classic Windows spacing
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(15, 15, 15, 15);
  mainLayout->setSpacing(10);

  // Content area with icon and input
  QHBoxLayout *contentLayout = new QHBoxLayout();
  contentLayout->setSpacing(15);

  // Icon on the left - styled like classic Windows
  QLabel *iconLabel = new QLabel(this);
  QPixmap iconPixmap(":/src/assets/icons/taskmgr.ico");
  if (!iconPixmap.isNull())
  {
    iconPixmap = iconPixmap.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    iconLabel->setPixmap(iconPixmap);
  }
  else
  {
    iconLabel->setText("Run");
    iconLabel->setStyleSheet("font-weight: bold; font-size: 16px;");
  }
  iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  iconLabel->setFixedWidth(60);
  contentLayout->addWidget(iconLabel);

  // Right side: label and input
  QVBoxLayout *inputLayout = new QVBoxLayout();
  inputLayout->setSpacing(6);
  inputLayout->setContentsMargins(0, 0, 0, 0);

  QLabel *openLabel = new QLabel("Open:", this);
  openLabel->setStyleSheet("font-weight: bold; color: #000000;");
  inputLayout->addWidget(openLabel);

  m_commandInput = new QLineEdit(this);
  m_commandInput->setPlaceholderText("Type the name of a program, folder, document, or Internet resource");
  m_commandInput->setMinimumHeight(22);
  inputLayout->addWidget(m_commandInput);

  inputLayout->addStretch();

  contentLayout->addLayout(inputLayout, 1);

  mainLayout->addLayout(contentLayout);

  // Restricted access checkbox
  m_restrictedAccessCheckbox = new QCheckBox("Run this command with restricted access", this);
  m_restrictedAccessCheckbox->setVisible(false); // Hidden on non-Windows or when not applicable
  mainLayout->addWidget(m_restrictedAccessCheckbox);

  mainLayout->addSpacing(5);

  // Buttons - arranged like classic Windows dialog
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  buttonLayout->setSpacing(6);
  buttonLayout->addStretch();

  m_okButton = new QPushButton("OK", this);
  m_okButton->setMinimumWidth(75);
  m_okButton->setDefault(true);
  buttonLayout->addWidget(m_okButton);

  m_cancelButton = new QPushButton("Cancel", this);
  m_cancelButton->setMinimumWidth(75);
  buttonLayout->addWidget(m_cancelButton);

  m_browseButton = new QPushButton("Browse...", this);
  m_browseButton->setMinimumWidth(75);
  buttonLayout->addWidget(m_browseButton);

  mainLayout->addLayout(buttonLayout);

  setLayout(mainLayout);
  m_commandInput->setFocus();
}

QString RunDialog::getCommand() const
{
  return m_commandInput->text();
}

void RunDialog::onBrowseClicked()
{
  QString filter = "Executable Files (*.exe *.sh *.bin *.app *.com *.bat *.cmd);;All Files (*)";

  QString startPath = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
  if (startPath.isEmpty())
  {
    startPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  }

  QString selectedFile = QFileDialog::getOpenFileName(this, "Browse for a program", startPath, filter);

  if (!selectedFile.isEmpty())
  {
    m_commandInput->setText(selectedFile);
  }
}

void RunDialog::onOkClicked()
{
  QString command = m_commandInput->text().trimmed();

  if (command.isEmpty())
  {
    return;
  }

  // Parse the command - handle quoted paths and arguments
  QString program = command;
  QStringList arguments;

  // Check if the command starts with a quote
  if (command.startsWith('"'))
  {
    int endQuote = command.indexOf('"', 1);
    if (endQuote != -1)
    {
      program = command.mid(1, endQuote - 1);
      QString remaining = command.mid(endQuote + 1).trimmed();
      if (!remaining.isEmpty())
      {
        arguments = remaining.split(' ', Qt::SkipEmptyParts);
      }
    }
    else
    {
      program = command;
    }
  }
  else
  {
    // Split on first space
    int spaceIndex = command.indexOf(' ');
    if (spaceIndex != -1)
    {
      program = command.left(spaceIndex);
      arguments = command.mid(spaceIndex + 1).split(' ', Qt::SkipEmptyParts);
    }
  }

  // Try to start the command/program
  if (!QProcess::startDetached(program, arguments))
  {
    // If it's not an executable, try to open it with the system default application
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(command)))
    {
      // Fallback: try the command as-is with shell
      QProcess::startDetached("sh", {"-c", command});
    }
  }

  accept();
}
