#include <QApplication>
#include "taskmanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TaskManager taskManager;
    taskManager.show();
    return app.exec();
}
