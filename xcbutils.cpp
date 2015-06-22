#include "xcbutils.h"

#include <QDebug>

using namespace Xcb;

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
        event->time = QX11Info::getTimestamp();
        event->same_screen = 1;
        event->root = QX11Info::appRootWindow();
        event->root_x = x;
        event->root_y = y;
        event->event_x = 5;
        event->event_y = 5;
        event->child = windowId;
        event->state = click;

        xcb_send_event(QX11Info::connection(), false, event->child, XCB_EVENT_MASK_BUTTON_PRESS, (char *) event);
    }

    //mouse up
    {
        xcb_button_release_event_t* event = new xcb_button_release_event_t;
        memset(event, 0x00, sizeof(xcb_button_release_event_t));
        event->response_type = XCB_BUTTON_RELEASE;
        event->event = windowId;
        event->time = QX11Info::getTimestamp();
        event->same_screen = 1;
        event->root = QX11Info::appRootWindow();
        event->root_x = x;
        event->root_y = y;
        event->event_x = 5;
        event->event_y = 5;
        event->child = windowId;
        event->state = 0;
        
        xcb_send_event(QX11Info::connection(), false, windowId, XCB_EVENT_MASK_BUTTON_RELEASE, (char *) event);
    }
}

Q_GLOBAL_STATIC(XCBEventDispatcher, s_instance);

XCBEventDispatcher* Xcb::XCBEventDispatcher::instance()
{
    return s_instance;
}
