#include "xcbutils.h"

#include <QDebug>

using namespace Xcb;


void XCBEventDispatcher::flush(xcb_timestamp_t timestamp)
{
    while(!m_pendingEvents.isEmpty()) {
        xcb_generic_event_t *ev = m_pendingEvents.takeFirst();
        if (ev->response_type == XCB_BUTTON_PRESS) {
            auto event = reinterpret_cast<xcb_button_press_event_t*>(ev);
            event->time = timestamp;
            xcb_send_event(QX11Info::connection(), false, event->child, XCB_EVENT_MASK_BUTTON_PRESS, (char *) event);
        } else if (ev->response_type == XCB_BUTTON_RELEASE) {
            auto windowId = reinterpret_cast<xcb_button_release_event_t*>(ev)->child;
            reinterpret_cast<xcb_button_release_event_t*>(ev)->time = timestamp;
            qDebug() << "actual send up to " << windowId << reinterpret_cast<xcb_button_press_event_t*>(ev)->time;
            xcb_send_event(QX11Info::connection(), false, windowId, XCB_EVENT_MASK_BUTTON_RELEASE, (char *) ev);
        }

        delete ev;
    }
}

void XCBEventDispatcher::mouseClick(WindowId windowId, bool right, int x, int y)
{
    uint16_t click = right ? XCB_BUTTON_MASK_3 : XCB_BUTTON_MASK_1;

    qDebug() << "sending some clicks to " << windowId;

    //mouse down
    {
        xcb_button_press_event_t* event = new xcb_button_press_event_t;
        memset(event, 0x00, sizeof(xcb_button_press_event_t));
        event->response_type = XCB_BUTTON_PRESS;
        event->event = windowId;
        event->same_screen = 1;
        event->root = QX11Info::appRootWindow();
        event->root_x = x;
        event->root_y = y;
        event->event_x = 5;
        event->event_y = 5;
        event->child = windowId;
        event->state = click;

        m_pendingEvents.append(reinterpret_cast<xcb_generic_event_t*>(event));
    }

    //mouse up
    {
        xcb_button_release_event_t* event = new xcb_button_release_event_t;
        memset(event, 0x00, sizeof(xcb_button_release_event_t));
        event->response_type = XCB_BUTTON_RELEASE;
        event->event = windowId;
        event->same_screen = 1;
        event->root = QX11Info::appRootWindow();
        event->root_x = x;
        event->root_y = y;
        event->event_x = 5;
        event->event_y = 5;
        event->child = windowId;
        event->state = click;
        m_pendingEvents.append(reinterpret_cast<xcb_generic_event_t*>(event));
    }

    //we need a timestamp from X
    //easiest solution is to send a dummy request to X and use the timestamp from that reply
    xcb_atom_t tmp = XCB_ATOM_ATOM;
    xcb_change_property(QX11Info::connection(), XCB_PROP_MODE_REPLACE, QX11Info::appRootWindow(), XCB_ATOM_ATOM, XCB_ATOM_ATOM, 32, 1, (const void *) &tmp);
}

Q_GLOBAL_STATIC(XCBEventDispatcher, s_instance);

XCBEventDispatcher* Xcb::XCBEventDispatcher::instance()
{
    return s_instance;
}
