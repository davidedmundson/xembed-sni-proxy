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

#include "statusnotifieritemadaptor.h"
#include "statusnotifierwatcher_interface.h"

static const char s_statusNotifierWatcherServiceName[] = "org.kde.StatusNotifierWatcher";

// __inline int toInt(WId wid)
// {
//     return (int)wid;
// }



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
    xcb_send_event(QX11Info::connection(), false, towin, XCB_EVENT_MASK_NO_EVENT, (char *) &ev);
}

SNIProxy::SNIProxy(WId wid, QObject* parent):
    QObject(parent),
    m_windowId(wid),
    m_service(QString("org.kde.StatusNotifierItem-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(++s_serviceCount)),
    m_dbus(QDBusConnection::sessionBus())
{
    //TODO only do once
    qDBusRegisterMetaType<KDbusImageStruct>();
    qDBusRegisterMetaType<KDbusImageVector>();
    qDBusRegisterMetaType<KDbusToolTipStruct>();

    //create new SNI
    new StatusNotifierItemAdaptor(this);
    qDebug() << "service is" << m_service;
    m_dbus.registerService(m_service);
    m_dbus.registerObject("/StatusNotifierItem", this);

    auto statusNotifierWatcher = new org::kde::StatusNotifierWatcher(s_statusNotifierWatcherServiceName, "/StatusNotifierWatcher", QDBusConnection::sessionBus());
    statusNotifierWatcher->RegisterStatusNotifierItem(m_service);
    //LEAK

    auto window = new QWidget;
    window->show();

    xcb_get_property_cookie_t em_cookie;
    const uint32_t select_input_val[] =
    {
        XCB_EVENT_MASK_STRUCTURE_NOTIFY
            | XCB_EVENT_MASK_PROPERTY_CHANGE
            | XCB_EVENT_MASK_ENTER_WINDOW
    };

    //set a background (ideally I want this transparent)
    const uint32_t backgroundPixel[4] = {0,0,0,0};
    xcb_change_window_attributes(QX11Info::connection(), wid, XCB_CW_BACK_PIXEL,
                                 backgroundPixel);

    xcb_change_window_attributes(QX11Info::connection(), wid, XCB_CW_EVENT_MASK,
                                 select_input_val);

    /* we grab the window, but also make sure it's automatically reparented back
     * to the root window if we should die.
    */
    xcb_change_save_set(QX11Info::connection(), XCB_SET_MODE_INSERT, wid);
    xcb_reparent_window(QX11Info::connection(), wid,
                        window->winId(),
                        0, 0);

    //tell client we're embedding it
    xembed_message_send(wid, XEMBED_EMBEDDED_NOTIFY, 0, window->winId(), 0);

    //resize window we're embedding
    const int baseSize = 48;
    const uint32_t config_vals[4] = { 0, 0 , baseSize, baseSize };
    xcb_configure_window(QX11Info::connection(), wid,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                             config_vals);

    update();
}

SNIProxy::~SNIProxy()
{
    m_dbus.unregisterObject("/StatusNotifierItem");
    m_dbus.unregisterService(m_service);
    m_dbus.disconnectFromBus(m_service);
}

void SNIProxy::update()
{
    realUpdate();
    //bodge to remove
    QTimer::singleShot(100, this, SLOT(realUpdate()));
}

void SNIProxy::realUpdate()
{
    QPixmap image = qApp->primaryScreen()->grabWindow(m_windowId);
    m_pixmap = image;
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
    return "title"; //KWinInfo on windowId
}

int SNIProxy::WindowId() const
{
    return m_windowId;
}

//____________actions_____________

void SNIProxy::Activate(int x, int y)
{
    mouseClick(false, x, y);
}

void SNIProxy::ContextMenu(int x, int y)
{
    qDebug() << "Context menu" << x << y;
    mouseClick(true, x, y);
}

void SNIProxy::SecondaryActivate(int x, int y)
{
//     mouseClick(false);
}

void SNIProxy::Scroll(int delta, const QString& orientation)
{

}

void SNIProxy::mouseClick(bool right, int x, int y)
{
    qDebug() << "activate requested";
    uint16_t click = right ? XCB_BUTTON_MASK_3 : XCB_BUTTON_MASK_1;

    //mouse down
    {
    xcb_button_press_event_t event;
    memset(&event, 0x00, sizeof(event));
    event.time = XCB_CURRENT_TIME;
    event.response_type = XCB_BUTTON_PRESS;
    event.event = m_windowId;
    event.same_screen = 1;
    event.root = QX11Info::appRootWindow();
    event.root_x = x;
    event.root_y = y;
    event.event_x = 5;
    event.event_y = 5;
    event.child = m_windowId;
    event.state = click;
    xcb_send_event(QX11Info::connection(), false, m_windowId, XCB_EVENT_MASK_BUTTON_PRESS, (char *) &event);
    }

    //mouse up
    {
    xcb_button_release_event_t event;
    memset(&event, 0x00, sizeof(event));
    event.time = XCB_CURRENT_TIME;
    event.response_type = XCB_BUTTON_RELEASE;
    event.event = m_windowId;
    event.same_screen = 1;
    event.root = QX11Info::appRootWindow();
    event.root_x = x;
    event.root_y = y;
    event.event_x = 5;
    event.event_y = 5;
    event.child = m_windowId;
    event.state = click;
    xcb_send_event(QX11Info::connection(), false, m_windowId, XCB_EVENT_MASK_BUTTON_RELEASE, (char *) &event);
    }
}


//
