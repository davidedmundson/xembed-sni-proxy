/*
 * Main
 * Copyright (C) 2015 <davidedmundson@kde.org> David Edmundson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <QLockFile>
#include <QGuiApplication>
#include "fdoselectionmanager.h"

#include "xcbutils.h"
#include "snidbus.h"

#include <QtDBus/QtDBus>

namespace Xcb {
    Xcb::Atoms* atoms;
}

Q_LOGGING_CATEGORY(SNIPROXY, "kde.xembedsniproxy", QtDebugMsg) //change to QtInfoMsg near release

int main(int argc, char ** argv)
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString user = env.value(QStringLiteral("USER"));
    const QString lockFilePath = QDir::temp().filePath(QStringLiteral("xembedsniproxy-%1.lock").arg(user));
    QLockFile lockFile(lockFilePath);
    lockFile.setStaleLockTime(0);
    lockFile.removeStaleLockFile();
    if (!lockFile.tryLock(1000))
    {
        QLockFile::LockError error = lockFile.error();
        switch (error)
        {
            case QLockFile::NoError:
                qCCritical(SNIPROXY) << "uh-oh... this should never happen. Quitting";
                break;

            case QLockFile::LockFailedError:
                qCCritical(SNIPROXY) << "another instance is running. Quitting";
                break;

            case QLockFile::PermissionError:
                qCCritical(SNIPROXY) << QStringLiteral(
                    "can't create lock file: problem with permissions (check permissions for dir '%1' and file '%2'). Quitting"
                ).arg(QDir::tempPath(), lockFilePath);
                break;

            case QLockFile::UnknownError:
                qCCritical(SNIPROXY) << QStringLiteral(
                    "unknown error occured when trying to create lock file at '%1'. Quitting"
                ).arg(lockFilePath);
                break;
        }
        return 1;
    }

    //the whole point of this is to interact with X, if we are in any other session, force trying to connect to X
    //if the QPA can't load xcb, this app is useless anyway.
    qputenv("QT_QPA_PLATFORM", "xcb");

    QGuiApplication app(argc, argv);
    app.setDesktopSettingsAware(false);
    app.setQuitOnLastWindowClosed(false);

    qDBusRegisterMetaType<KDbusImageStruct>();
    qDBusRegisterMetaType<KDbusImageVector>();
    qDBusRegisterMetaType<KDbusToolTipStruct>();

    Xcb::atoms = new Xcb::Atoms();

    FdoSelectionManager manager;

    app.exec();

    delete Xcb::atoms;
    return 0;
}