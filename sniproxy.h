/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright (C) 2015  <copyright holder> <email>
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

#ifndef SNIPROXY_H
#define SNIPROXY_H

#include <QObject>
#include <QWindow>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QPixmap>

#include "snidbus.h"

class SNIProxy : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString Category READ Category)
    Q_PROPERTY(QString Id READ Id)
    Q_PROPERTY(QString Title READ Title)
    Q_PROPERTY(QString Status READ Status)
    Q_PROPERTY(int WindowId READ WindowId)
    Q_PROPERTY(bool ItemIsMenu READ ItemIsMenu)
    Q_PROPERTY(KDbusImageVector IconPixmap READ IconPixmap)
//     Q_PROPERTY(QString IconName READ IconName)
//     Q_PROPERTY(QString OverlayIconName READ OverlayIconName)
//     Q_PROPERTY(KDbusImageVector OverlayIconPixmap READ OverlayIconPixmap)
//     Q_PROPERTY(QString AttentionIconName READ AttentionIconName)
//     Q_PROPERTY(KDbusImageVector AttentionIconPixmap READ AttentionIconPixmap)
//     Q_PROPERTY(QString AttentionMovieName READ AttentionMovieName)
//     Q_PROPERTY(KDbusToolTipStruct ToolTip READ ToolTip)
//     Q_PROPERTY(QString IconThemePath READ IconThemePath)
//     Q_PROPERTY(QDBusObjectPath Menu READ Menu)
public:
    SNIProxy(WId wid, QObject *parent=0);
    ~SNIProxy();

    void update();

    /**
     * @return the category of the application associated to this item
     * @see Category
     */
    QString Category() const;

    /**
     * @return the id of this item
     */
    QString Id() const;

    /**
     * @return the title of this item
     */
    QString Title() const;

    /**
     * @return The status of this item
     * @see Status
     */
    QString Status() const;

    /**
     * @return The id of the main window of the application that controls the item
     */
    int WindowId() const;

    /**
     * @return The item only support the context menu, the visualization should prefer sending ContextMenu() instead of Activate()
     */
    bool ItemIsMenu() const;

    /**
     * @return a serialization of the icon data
     */
    KDbusImageVector IconPixmap() const;


    /**
     * all the data needed for a tooltip
     */
//     KDbusToolTipStruct ToolTip() const;

public Q_SLOTS:
    //interaction
    /**
     * Shows the context menu associated to this item
     * at the desired screen position
     */
    void ContextMenu(int x, int y);

    /**
     * Shows the main widget and try to position it on top
     * of the other windows, if the widget is already visible, hide it.
     */
    void Activate(int x, int y);

    /**
     * The user activated the item in an alternate way (for instance with middle mouse button, this depends from the systray implementation)
     */
    void SecondaryActivate(int x, int y);

    /**
     * Inform this item that the mouse wheel was used on its representation
     */
    void Scroll(int delta, const QString &orientation);

Q_SIGNALS:
    /**
     * Inform the systemtray that the own main icon has been changed,
     * so should be reloaded
     */
    void NewIcon();

    /**
     * Inform the systemtray that there is a new icon to be used as overlay
     */
    void NewOverlayIcon();

    /**
     * Inform the systemtray that the requesting attention icon
     * has been changed, so should be reloaded
     */
    void NewAttentionIcon();

    /**
     * Inform the systemtray that something in the tooltip has been changed
     */
    void NewToolTip();

    /**
     * Signal the new status when it has been changed
     * @see Status
     */
    void NewStatus(const QString &status);

private:
    QString m_service;
    QDBusConnection m_dbus;
    WId m_windowId;
    static int s_serviceCount;
    QPixmap m_pixmap;
};

#endif // SNIPROXY_H
