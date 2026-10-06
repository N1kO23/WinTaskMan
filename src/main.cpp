#include "ui/mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/taskmgr.ico")));

  MainWindow window;
  window.show();
  return app.exec();
}
