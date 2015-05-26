#include <QApplication>
#include "fdoselectionmanager.h"

int main(int argc, char ** argv)
{
    QApplication app(argc, argv);
    FdoSelectionManager manager;
    app.exec();
    return 0;
}