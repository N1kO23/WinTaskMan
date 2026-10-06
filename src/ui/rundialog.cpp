#include "rundialog.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

RunDialog::RunDialog(QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("Run"));
  setMinimumWidth(420);
  setMaximumWidth(500);

  auto *iconLabel = new QLabel(this);
  iconLabel->setPixmap(windowIcon().pixmap(48, 48));
  iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  iconLabel->setFixedWidth(60);

  auto *openLabel = new QLabel(tr("Open:"), this);
  openLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));

  m_commandInput = new QLineEdit(this);
  m_commandInput->setPlaceholderText(tr("Type the name of a program, folder, document, or Internet resource"));
  m_commandInput->setMinimumHeight(22);

  auto *inputLayout = new QVBoxLayout;
  inputLayout->setSpacing(6);
  inputLayout->addWidget(openLabel);
  inputLayout->addWidget(m_commandInput);
  inputLayout->addStretch();

  auto *contentLayout = new QHBoxLayout;
  contentLayout->setSpacing(15);
  contentLayout->addWidget(iconLabel);
  contentLayout->addLayout(inputLayout, 1);

  // Enter in the input field presses the default button, OK.
  auto *okButton = new QPushButton(tr("OK"), this);
  okButton->setDefault(true);
  auto *cancelButton = new QPushButton(tr("Cancel"), this);
  auto *browseButton = new QPushButton(tr("Browse..."), this);

  auto *buttonLayout = new QHBoxLayout;
  buttonLayout->setSpacing(6);
  buttonLayout->addStretch();
  for (QPushButton *button : {okButton, cancelButton, browseButton})
  {
    button->setMinimumWidth(75);
    buttonLayout->addWidget(button);
  }

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(15, 15, 15, 15);
  mainLayout->setSpacing(10);
  mainLayout->addLayout(contentLayout);
  mainLayout->addSpacing(5);
  mainLayout->addLayout(buttonLayout);

  connect(okButton, &QPushButton::clicked, this, &RunDialog::run);
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(browseButton, &QPushButton::clicked, this, &RunDialog::browse);

  m_commandInput->setFocus();
}

void RunDialog::browse()
{
  QString startPath = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
  if (startPath.isEmpty())
    startPath = QDir::homePath();

  const QString file = QFileDialog::getOpenFileName(this, tr("Browse for a program"), startPath,
                                                    tr("Executable Files (*.exe *.sh *.bin *.app *.com *.bat *.cmd);;All Files (*)"));
  if (file.isEmpty())
    return;

  // Quote paths with spaces so they aren't split into arguments.
  m_commandInput->setText(file.contains(QLatin1Char(' ')) ? QLatin1Char('"') + file + QLatin1Char('"') : file);
}

void RunDialog::run()
{
  const QString command = m_commandInput->text().trimmed();
  if (command.isEmpty())
    return;

  // Start it as a program; failing that, open it as a file, folder or URL; failing that,
  // let the shell have a go.
  QStringList arguments = QProcess::splitCommand(command);
  const QString program = arguments.isEmpty() ? command : arguments.takeFirst();
  if (!QProcess::startDetached(program, arguments)
      && !QDesktopServices::openUrl(QUrl::fromUserInput(command, QDir::currentPath())))
    QProcess::startDetached(QStringLiteral("sh"), {QStringLiteral("-c"), command});

  accept();
}
