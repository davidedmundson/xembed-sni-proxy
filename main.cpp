#include <QApplication>
#include "fdoselectionmanager.h"

#include "xcbutils.h"

namespace Xcb {
    Xcb::Atoms* atoms;
}

int main(int argc, char ** argv)
{
    QApplication app(argc, argv);
    Xcb::atoms = new Xcb::Atoms();
    FdoSelectionManager manager;
    app.exec();
    return 0;
}