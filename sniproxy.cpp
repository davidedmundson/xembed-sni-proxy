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

#include "sniproxy.h"

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_event.h>

#include "xcbutils.h"

#include <QWidget>
#include <QDebug>
#include <QX11Info>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>

#include <KWindowSystem>

#include "statusnotifieritemadaptor.h"
#include "statusnotifierwatcher_interface.h"

static const char s_statusNotifierWatcherServiceName[] = "org.kde.StatusNotifierWatcher";

int SNIProxy::s_serviceCount = 0;

void
xembed_message_send(xcb_window_t towin,
                    long message, long d1, long d2, long d3)
{
    xcb_client_message_event_t ev;

    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = towin;
    ev.format = 32;
    ev.data.data32[0] = XCB_CURRENT_TIME;
    ev.data.data32[1] = message;
    ev.data.data32[2] = d1;
    ev.data.data32[3] = d2;
    ev.data.data32[4] = d3;
    ev.type = Xcb::atoms->xembedAtom;
    auto cookie = xcb_send_event(QX11Info::connection(), false, towin, XCB_EVENT_MASK_NO_EVENT, (char *) &ev);

    auto err = xcb_request_check(QX11Info::connection(), cookie);
    qDebug() << "xembed error " << err;

    xcb_flush(QX11Info::connection());
}

SNIProxy::SNIProxy(WId wid, QObject* parent):
    QObject(parent),
    m_windowId(wid),
    m_service(QString("org.kde.StatusNotifierItem-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(++s_serviceCount)),
    m_dbus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QString("DaveTray%1").arg(s_serviceCount)))
    // in order to have 2 SNIs we need to have 2 connections to DBus.. Do not simply use QDbusConnnection::sessionBus here
    //Ideally we should change the spec to pass a Path name along with a service name in RegisterItem as this is silly
{

    //create new SNI
    new StatusNotifierItemAdaptor(this);
    m_dbus.registerObject("/StatusNotifierItem", this);

    auto statusNotifierWatcher = new org::kde::StatusNotifierWatcher(s_statusNotifierWatcherServiceName, "/StatusNotifierWatcher", QDBusConnection::sessionBus(), this);
    statusNotifierWatcher->RegisterStatusNotifierItem(m_dbus.baseService());

    auto c = QX11Info::connection();

    WId parentWinId = qobject_cast< QWidget* >(parent)->winId();

    const uint32_t select_input_val[] =
    {
        XCB_EVENT_MASK_STRUCTURE_NOTIFY
            | XCB_EVENT_MASK_PROPERTY_CHANGE
            | XCB_EVENT_MASK_ENTER_WINDOW
    };
    xcb_change_window_attributes(c, wid, XCB_CW_EVENT_MASK,
                                 select_input_val);
    xcb_reparent_window(c, wid,
                        parentWinId,
                        0, 0);

    /* we grab the window, but also make sure it's automatically reparented back
     * to the root window if we should die.
    */
    xcb_change_save_set(c, XCB_SET_MODE_INSERT, wid);

    //tell client we're embedding it
    xembed_message_send(wid, XEMBED_EMBEDDED_NOTIFY, 0, parentWinId, 0);

    //resize window we're embedding
    const int baseSize = 48;
    /*const*/ uint32_t config_vals[4] = { 0, 0 , baseSize, baseSize };

    //set an X and Y for viewing in the debug UI.
    //so they don't overlap
    //TODO this is going to run out of space we need to relayout everything in the main view properly
    config_vals[0] = baseSize * s_serviceCount % 480;
    config_vals[1] = baseSize * qFloor(s_serviceCount / 10);

    auto cookie = xcb_configure_window(c, wid,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                             config_vals);

    //show the embedded window otherwise nothing happens
    xcb_map_window(c, wid);

    //awesome's system trays has a flush here...so we should too
    xcb_flush(c);

    update();
}

SNIProxy::~SNIProxy()
{
    QDBusConnection::disconnectFromBus(m_dbus.name());
}

void SNIProxy::update()
{
    //get pixmap (xcb_drawable)
    auto getImageCookie = xcb_get_image(QX11Info::connection(), XCB_IMAGE_FORMAT_Z_PIXMAP, m_windowId, 0, 0, 48, 48, 0xFFFFFF);

    //get image from that
    auto reply = xcb_get_image_reply(QX11Info::connection(), getImageCookie, Q_NULLPTR);
    if (!reply) {
        qDebug() << "no image fetched from embedded client :(";
        return;
    }

    auto t = xcb_get_image_data(reply);

    QImage image(xcb_get_image_data(reply), 48, 48, 48*4, QImage::Format_ARGB32);
    qDebug() << image.isNull() << image.width();
    image.save("/tmp/foo.png");
    //turn that into something usable

    //FIXME reply leaks

    m_pixmap = QPixmap::fromImage(image);
    emit NewIcon();
}

//____________properties__________

QString SNIProxy::Category() const
{
    return "ApplicationStatus";
}

QString SNIProxy::Id() const
{
    return QString::number(m_windowId); //TODO
}

KDbusImageVector SNIProxy::IconPixmap() const
{
    KDbusImageStruct dbusImage(m_pixmap.toImage());
    return KDbusImageVector() << dbusImage;
}

bool SNIProxy::ItemIsMenu() const
{
    return false;
}

QString SNIProxy::Status() const
{
    return "Active";
}

QString SNIProxy::Title() const
{
    KWindowInfo window (m_windowId, NET::WMName);
    return window.name();
}

int SNIProxy::WindowId() const
{
    return m_windowId;
}

//____________actions_____________

void SNIProxy::Activate(int x, int y)
{
    Xcb::XCBEventDispatcher::instance()->mouseClick(m_windowId, false, x, y);
}

void SNIProxy::ContextMenu(int x, int y)
{
    Xcb::XCBEventDispatcher::instance()->mouseClick(m_windowId, true, x, y);
}

void SNIProxy::SecondaryActivate(int x, int y)
{
//     mouseClick(false);
}

void SNIProxy::Scroll(int delta, const QString& orientation)
{

}



//
