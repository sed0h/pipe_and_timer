#include "Tester_UD.h"
#include <QtWidgets/QApplication>
#include <Windows.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Tester_UD w;
    w.show();
    return a.exec();
}
