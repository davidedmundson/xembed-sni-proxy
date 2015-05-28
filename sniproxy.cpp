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
#include <../../opt/qt5/include/QtCore/QTimer>

#include <QWidget>
#include <QDebug>
#include <QX11Info>
#include <QScreen>
#include <QGuiApplication>

#include <kstatusnotifieritem.h>

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
    m_sni(new KStatusNotifierItem(this))
{
    m_sni->setStatus(KStatusNotifierItem::Active);
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
    QTimer *t = new QTimer(this);
    t->setInterval(1000);
    connect(t, &QTimer::timeout, this, &SNIProxy::update);
    t->start();
}

void SNIProxy::update()
{
    QPixmap image = qApp->primaryScreen()->grabWindow(m_windowId);
    m_sni->setIconByPixmap(image);
}
