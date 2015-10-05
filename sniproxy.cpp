/*
 * Holds one embedded window, registers as DBus entry
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

#include "sniproxy.h"

#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_image.h>

#include "xcbutils.h"

#include <QDebug>
#include <QX11Info>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <QTimer>

#include <QPainter>

#include <KWindowSystem>
#include <netwm.h>

#include "statusnotifieritemadaptor.h"
#include "statusnotifierwatcher_interface.h"

static const char s_statusNotifierWatcherServiceName[] = "org.kde.StatusNotifierWatcher";
static int s_embedSize = 48; //max size of window to embed. We no longer resize the embedded window as Chromium acts stupidly.

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

SNIProxy::SNIProxy(WId wid, QWindow* parent): SNIProxy(wid, parent, parent->devicePixelRatio()) {}

SNIProxy::SNIProxy(WId wid, QObject* parent, qreal devicePixelRatio):
    QObject(parent),
    m_dbus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QString("XembedSniProxy%1").arg(s_serviceCount++))),
    m_windowId(wid),
    m_devicePixelRatio(devicePixelRatio)
    // in order to have 2 SNIs we need to have 2 connections to DBus.. Do not simply use QDbusConnnection::sessionBus here
    //Ideally we should change the spec to pass a Path name along with a service name in RegisterItem as this is silly
{
    //create new SNI
    new StatusNotifierItemAdaptor(this);
    m_dbus.registerObject("/StatusNotifierItem", this);

    auto statusNotifierWatcher = new org::kde::StatusNotifierWatcher(s_statusNotifierWatcherServiceName, "/StatusNotifierWatcher", QDBusConnection::sessionBus(), this);
    statusNotifierWatcher->RegisterStatusNotifierItem(m_dbus.baseService());

    auto c = QX11Info::connection();

    auto cookie = xcb_get_geometry(c, m_windowId);
    QScopedPointer<xcb_get_geometry_reply_t> clientGeom(xcb_get_geometry_reply(c, cookie, Q_NULLPTR));

    //create a container window
    auto screen = xcb_setup_roots_iterator (xcb_get_setup (c)).data;
    m_containerWid = xcb_generate_id(c);
    uint32_t             values[2];
    auto mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT;
    values[0] = screen->black_pixel; //draw a solid background so the embeded icon doesn't get garbage in it
    values[1] = true; //bypass wM
    xcb_create_window (c,                          /* connection    */
                    XCB_COPY_FROM_PARENT,          /* depth         */
                     m_containerWid,               /* window Id     */
                     screen->root,                 /* parent window */
                     -500, 0,                       /* x, y          */
                     s_embedSize, s_embedSize,     /* width, height */
                     0,                           /* border_width  */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT,/* class         */
                     screen->root_visual,          /* visual        */
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

    //move window we're embedding
    const uint32_t windowMoveConfigVals[2] = { 0, 0 };

    xcb_configure_window(c, wid,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                             windowMoveConfigVals);


    //if the window is a clearly stupid size resize to be something sensible
    //this is needed as chormium and such when resized just fill the icon with transparent space and only draw in the middle
    //however spotify does need this as by default the window size is 900px wide.
    //use an artbitrary heuristic to make sure icons are always sensible
    if (clientGeom->width < 12 || clientGeom->width > s_embedSize ||
        clientGeom->height < 12 || clientGeom->height > s_embedSize)
    {
        const uint32_t windowMoveConfigVals[2] = { s_embedSize, s_embedSize };
        xcb_configure_window(c, wid,
                                XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                                windowMoveConfigVals);
    }


    xcb_clear_area(c, 0, wid, 0, 0, s_embedSize, s_embedSize);

    //show the embedded window otherwise nothing happens
    xcb_map_window(c, wid);

    xcb_clear_area(c, 0, wid, 0, 0, s_embedSize, s_embedSize);

    xcb_flush(c);

    //there's no damage event for the first paint, and sometimes it's not drawn immediately
    //not ideal, but it works better than nothing
    //test with xchat before changing
    QTimer::singleShot(500, this, &SNIProxy::update);
}

SNIProxy::~SNIProxy()
{
    QDBusConnection::disconnectFromBus(m_dbus.name());
}

void SNIProxy::update()
{

    //get pixmap (xcb_drawable)
    m_pixmap = QPixmap::fromImage(getImageNonComposite());
    emit NewIcon();
}

void sni_cleanup_xcb_image(void *data) {
    xcb_image_destroy(static_cast<xcb_image_t*>(data));
}

QImage SNIProxy::getImageNonComposite()
{
    auto c = QX11Info::connection();
    auto cookie = xcb_get_geometry(c, m_windowId);
    QScopedPointer<xcb_get_geometry_reply_t> geom(xcb_get_geometry_reply(c, cookie, Q_NULLPTR));

    xcb_image_t *image = xcb_image_get(c, m_windowId, 0, 0, geom->width, geom->height, 0xFFFFFF, XCB_IMAGE_FORMAT_Z_PIXMAP);

    QImage qimage(image->data, image->width, image->height, image->stride, QImage::Format_ARGB32, sni_cleanup_xcb_image, image);

    return qimage.scaled(qimage.size() * m_devicePixelRatio);
}

// QImage SNIProxy::getImageComposite()
// {
//
// //     xcb_composite_name_window_pixmap(QX11Info::connection(), m_windowId, )
// }

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
        event->event_x = 0;
        event->event_y = 0;
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
        event->event_x = 0;
        event->event_y = 0;
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
