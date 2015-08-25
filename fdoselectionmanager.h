/***************************************************************************
 *   fdoselectionmanager.h                                                 *
 *                                                                         *
 *   Copyright (C) 2008 Jason Stubbs <jasonbstubbs@gmail.com>              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#ifndef FDOSELECTIONMANAGER_H
#define FDOSELECTIONMANAGER_H

#include <QWidget>
#include <QAbstractNativeEventFilter>

#include <xcb/xcb.h>

class FdoSelectionManagerPrivate;

class FdoSelectionManager : public QWidget, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    static FdoSelectionManager *manager();

    FdoSelectionManager();
    ~FdoSelectionManager();

    void addDamageWatch(WId client);

    void undock(xcb_window_t client);

protected:
    bool nativeEventFilter(const QByteArray & eventType, void * message, long * result) Q_DECL_OVERRIDE;

private slots:
    void initSelection();
//     void cleanupTask(WId winId);

private:
    friend class FdoSelectionManagerPrivate;
    FdoSelectionManagerPrivate* const d;
};


#endif
