#include <QApplication>
#include "fdoselectionmanager.h"

#include "xcbutils.h"
#include "snidbus.h"

#include <QtDBus/QtDBus>

namespace Xcb {
    Xcb::Atoms* atoms;
}

int main(int argc, char ** argv)
{
    QApplication app(argc, argv);

    qDBusRegisterMetaType<KDbusImageStruct>();
    qDBusRegisterMetaType<KDbusImageVector>();
    qDBusRegisterMetaType<KDbusToolTipStruct>();

    Xcb::atoms = new Xcb::Atoms();

    FdoSelectionManager manager;

    app.exec();
    return 0;
}