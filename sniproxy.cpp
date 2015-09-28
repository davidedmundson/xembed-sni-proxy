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

#include <QPainter>

#include <KWindowSystem>
#include <netwm.h>

#include "statusnotifieritemadaptor.h"
#include "statusnotifierwatcher_interface.h"

static const char s_statusNotifierWatcherServiceName[] = "org.kde.StatusNotifierWatcher";
static int s_embedSize = 48;

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
    m_dbus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QString("DaveTray%1").arg(s_serviceCount++)))
    // in order to have 2 SNIs we need to have 2 connections to DBus.. Do not simply use QDbusConnnection::sessionBus here
    //Ideally we should change the spec to pass a Path name along with a service name in RegisterItem as this is silly
{
    //create new SNI
    new StatusNotifierItemAdaptor(this);
    m_dbus.registerObject("/StatusNotifierItem", this);

    auto statusNotifierWatcher = new org::kde::StatusNotifierWatcher(s_statusNotifierWatcherServiceName, "/StatusNotifierWatcher", QDBusConnection::sessionBus(), this);
    statusNotifierWatcher->RegisterStatusNotifierItem(m_dbus.baseService());

    auto c = QX11Info::connection();

    //get the visual of the window we're embedding as it can differ from the root window
    auto cookie = xcb_get_window_attributes(c, wid);
    auto clientAttributes = xcb_get_window_attributes_reply(c, cookie, 0); //LEAKS

    //create a container window
    auto screen = xcb_setup_roots_iterator (xcb_get_setup (c)).data;
    m_containerWid = xcb_generate_id(c);
    uint32_t             values[2];
    auto mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT;
    values[0] = screen->white_pixel; //draw a solid background so the embeded icon doesn't get garbage in it
    values[1] = true; //bypass wM
    xcb_create_window (c,                          /* connection    */
                    XCB_COPY_FROM_PARENT,          /* depth         */
                     m_containerWid,                  /* window Id     */
                     screen->root,                 /* parent window */
                     500, 0,                      /* x, y          */
                     s_embedSize, s_embedSize,     /* width, height */
                     0,                           /* border_width  */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT,/* class         */
                     clientAttributes->visual,     /* visual        */
                     mask, values);                /* masks         */

    /*
        We need the window to exist and be mapped otherwise the child won't render it's contents

        We also need it to exist in the right place to get the clicks working as GTK will check sendEvent locations to see if our window is in the right place. So even though our contents are drawn via compositing we still put this window in the right place

        We can't composite it away anything parented owned by the root window (apparently)
        Stack Under works in the non composited case, but it doesn't seem to work in kwin's composited case (probably need set relevant NETWM hint)

        As a last resort set opacity to 0 just to make sure this container never appears
    */

#ifndef VISUAL_DEBUG
    const uint32_t stackBelowData[] = {XCB_STACK_MODE_BELOW};
    xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_STACK_MODE, stackBelowData);

    NETWinInfo wm(c, m_containerWid, screen->root, 0);
    wm.setOpacity(0);
#endif

    xcb_flush(c);

    xcb_map_window(c, m_containerWid);



//errors with bad attribute...no idea why
//     const uint32_t select_input_val[] =
//     {
//         XCB_EVENT_MASK_STRUCTURE_NOTIFY
//             | XCB_EVENT_MASK_PROPERTY_CHANGE
//             | XCB_EVENT_MASK_ENTER_WINDOW
//             | XCB_EVENT_MASK_BUTTON_PRESS
//             | XCB_EVENT_MASK_BUTTON_RELEASE
//     };
//     xcb_change_window_attributes(c, wid, XCB_CW_EVENT_MASK,
//                                  select_input_val);

    xcb_reparent_window(c, wid,
                        m_containerWid,
                        0, 0);

    /*
     * Render the embedded window offscreen
     */
    xcb_composite_redirect_window(c, wid, XCB_COMPOSITE_REDIRECT_MANUAL);


    /* we grab the window, but also make sure it's automatically reparented back
     * to the root window if we should die.
    */
    xcb_change_save_set(c, XCB_SET_MODE_INSERT, wid);

    //tell client we're embedding it
    xembed_message_send(wid, XEMBED_EMBEDDED_NOTIFY, m_containerWid, 0, 0);

    //resize window we're embedding
    const uint32_t config_vals[4] = { 0, 0 , s_embedSize, s_embedSize };

    xcb_configure_window(c, wid,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                             config_vals);

//     xcb_clear_area(c, 0, wid, 0, 0, s_embedSize, s_embedSize);

    //show the embedded window otherwise nothing happens
    xcb_map_window(c, wid);

    //awesome's system trays has a flush here...so we should too
    xcb_flush(c);

    //there's no damage event for the first paint, and sometimes it's not drawn immediately
    //not ideal, but it works better than nothing
    //test with xchat before changing
    QTimer::singleShot(500, this, &SNIProxy::update);
}

SNIProxy::~SNIProxy()
{
    QDBusConnection::disconnectFromBus(m_dbus.name());
    //we created a window, we should remove it
}

void SNIProxy::update()
{
    //get pixmap (xcb_drawable)
    auto getImageCookie = xcb_get_image(QX11Info::connection(), XCB_IMAGE_FORMAT_Z_PIXMAP, m_windowId, 0, 0, s_embedSize, s_embedSize, 0xFFFFFF);

    //get image from that
    auto reply = xcb_get_image_reply(QX11Info::connection(), getImageCookie, Q_NULLPTR);
    if (!reply) {
        qDebug() << "no image fetched from embedded client :(";
        return;
    }

    auto t = xcb_get_image_data(reply);

    QImage image(xcb_get_image_data(reply), s_embedSize, s_embedSize, s_embedSize*4, QImage::Format_ARGB32);

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
    sendClick(XCB_BUTTON_INDEX_1, x, y);
}

void SNIProxy::ContextMenu(int x, int y)
{
    sendClick(XCB_BUTTON_INDEX_3, x, y);
}

void SNIProxy::SecondaryActivate(int x, int y)
{
    sendClick(XCB_BUTTON_INDEX_1, x, y);
}

void SNIProxy::Scroll(int delta, const QString& orientation)
{
    if (orientation == "vertical") {
        sendClick(delta > 0 ? XCB_BUTTON_INDEX_4: XCB_BUTTON_INDEX_5, 0, 0);
    } else {
    }
}

void SNIProxy::sendClick(uint8_t mouseButton, int x, int y)
{
    //it's best not to look at this code
    //GTK doesn't like send_events and double checks the mouse position matches where the window is and is top level
    //in order to solve this we move the embed container over to where the mouse is then replay the event using send_event
    //if patching, test with xchat + xchat context menus

    //note x,y are not actually where the mouse is, but the plasmoid
    //ideally we should make this match the plasmoid hit area

    auto c = QX11Info::connection();

    //set our window so the middle is where the mouse is
    const uint32_t stackAboveData[] = {XCB_STACK_MODE_ABOVE};
    xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_STACK_MODE, stackAboveData);

    const uint32_t config_vals[4] = {x, y, s_embedSize, s_embedSize };
    xcb_configure_window(c, m_containerWid,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                             config_vals);
    //mouse down
    {
        xcb_button_press_event_t* event = new xcb_button_press_event_t;
        memset(event, 0x00, sizeof(xcb_button_press_event_t));
        event->response_type = XCB_BUTTON_PRESS;
        event->event = m_windowId;
        event->time = QX11Info::getTimestamp();
        event->same_screen = 1;
        event->root = QX11Info::appRootWindow();
        event->root_x = x;
        event->root_y = y;
        event->event_x = s_embedSize / 2;
        event->event_y = s_embedSize / 2;
        event->child = 0;
        event->state = 0;
        event->detail = mouseButton;

        xcb_send_event(c, false, m_windowId, XCB_EVENT_MASK_BUTTON_PRESS, (char *) event);
        free(event);
    }

    //mouse up
    {
        xcb_button_release_event_t* event = new xcb_button_release_event_t;
        memset(event, 0x00, sizeof(xcb_button_release_event_t));
        event->response_type = XCB_BUTTON_RELEASE;
        event->event = m_windowId;
        event->time = QX11Info::getTimestamp();
        event->same_screen = 1;
        event->root = QX11Info::appRootWindow();
        event->root_x = x;
        event->root_y = y;
        event->event_x = s_embedSize / 2;
        event->event_y = s_embedSize / 2;
        event->child = 0;
        event->state = 0;
        event->detail = mouseButton;

        xcb_send_event(c, false, m_windowId, XCB_EVENT_MASK_BUTTON_RELEASE, (char *) event);
        free(event);
    }
#ifndef VISUAL_DEBUG
    const uint32_t stackBelowData[] = {XCB_STACK_MODE_BELOW};
    xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_STACK_MODE, stackBelowData);
#endif
    }




//
