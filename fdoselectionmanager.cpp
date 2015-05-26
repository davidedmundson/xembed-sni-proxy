/***************************************************************************
 *   fdoselectionmanager.cpp                                               *
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

#include "fdoselectionmanager.h"
#include "x11embedpainter.h"

#include <QDebug>

#include <QCoreApplication>
#include <QHash>
#include <QTimer>

#include <QTextDocument>
#include <QX11Info>

// #include <KIconLoader>

// #include <Plasma/DataEngine>
// #include <Plasma/DataEngineManager>
// #include <Plasma/ServiceJob>

// #include <config-X11.h>

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_event.h>
// #include <X11/Xlib.h>
// #include <X11/Xatom.h>
// #include <X11/extensions/Xrender.h>

#include "xcbutils.h"

#ifdef HAVE_XFIXES
#  include <X11/extensions/Xfixes.h>
#endif

#ifdef HAVE_XDAMAGE
#  include <X11/extensions/Xdamage.h>
#endif

#ifdef HAVE_XCOMPOSITE
#  include <X11/extensions/Xcomposite.h>
#endif

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

static FdoSelectionManager *s_manager = 0;
static X11EmbedPainter *s_painter = 0;

#if defined(HAVE_XFIXES) && defined(HAVE_XDAMAGE) && defined(HAVE_XCOMPOSITE)
struct DamageWatch
{
    QWidget *container;
    Damage damage;
};

static int damageEventBase = 0;
static QMap<WId, DamageWatch*> damageWatches;
static QCoreApplication::EventFilter oldEventFilter;

// Global event filter for intercepting damage events
static bool x11EventFilter(void *message, long int *result)
{
    XEvent *event = reinterpret_cast<XEvent*>(message);
    if (event->type == damageEventBase + XDamageNotify) {
        XDamageNotifyEvent *e = reinterpret_cast<XDamageNotifyEvent*>(event);
        if (DamageWatch *damageWatch = damageWatches.value(e->drawable)) {
            // Create a new region and empty the damage region into it.
            // The window is small enough that we don't really care about the region;
            // we'll just throw it away and schedule a full repaint of the container.
            XserverRegion region = XFixesCreateRegion(e->display, 0, 0);
            XDamageSubtract(e->display, e->damage, None, region);
            XFixesDestroyRegion(e->display, region);
            damageWatch->container->update();
        }
    }

    if (oldEventFilter && oldEventFilter != x11EventFilter) {
        return oldEventFilter(message, result);
    } else {
        return false;
    }
}
#endif


struct MessageRequest
{
    long messageId;
    long timeout;
    long bytesRemaining;
    QByteArray message;
};


class FdoSelectionManagerPrivate
{
public:
    FdoSelectionManagerPrivate(FdoSelectionManager *q)
        : q(q),
          haveComposite(false)
    {
        display = QX11Info::display();
        char* selectionAtomName = xcb_atom_name_by_screen("_NET_SYSTEM_TRAY", QX11Info::appScreen());
        selectionAtom = Xcb::Atom(selectionAtomName);
        //TODO delete atom_name?
        
//         selectionAtom = XInternAtom(display, "_NET_SYSTEM_TRAY_S" + , false);
        opcodeAtom = Xcb::Atom("_NET_SYSTEM_TRAY_OPCODE");
        qDebug() << "op code atom is " << opcodeAtom;
//         messageAtom = XInternAtom(display, "_NET_SYSTEM_TRAY_MESSAGE_DATA", false);
//         visualAtom = XInternAtom(display, "_NET_SYSTEM_TRAY_VISUAL", false);

#if defined(HAVE_XFIXES) && defined(HAVE_XDAMAGE) && defined(HAVE_XCOMPOSITE)
        int eventBase, errorBase;
        bool haveXfixes = XFixesQueryExtension(display, &eventBase, &errorBase);
        bool haveXdamage = XDamageQueryExtension(display, &damageEventBase, &errorBase);
        bool haveXComposite = XCompositeQueryExtension(display, &eventBase, &errorBase);

        if (haveXfixes && haveXdamage && haveXComposite) {
            haveComposite = true;
            oldEventFilter = QCoreApplication::instance()->setEventFilter(x11EventFilter);
        }
#endif
    }

    void createNotification(WId winId);

    void handleRequestDock(xcb_window_t embed_win,  void *info);
//     void handleBeginMessage(const XClientMessageEvent &event);
//     void handleMessageData(const XClientMessageEvent &event);
//     void handleCancelMessage(const XClientMessageEvent &event);

    Display *display;
    xcb_atom_t selectionAtom;
    xcb_atom_t opcodeAtom;
    xcb_atom_t messageAtom;
    xcb_atom_t visualAtom;

//     QHash<WId, MessageRequest> messageRequests;
//     QHash<WId, FdoTask*> tasks;

    FdoSelectionManager *q;
//     Plasma::DataEngine *notificationsEngine;

    bool haveComposite;
};

FdoSelectionManager::FdoSelectionManager()
    : d(new FdoSelectionManagerPrivate(this))
{
    // Init the selection later just to ensure that no signals are sent
    // until after construction is done and the creating object has a
    // chance to connect.
    QTimer::singleShot(0, this, SLOT(initSelection()));
}


FdoSelectionManager::~FdoSelectionManager()
{
#if defined(HAVE_XFIXES) && defined(HAVE_XDAMAGE) && defined(HAVE_XCOMPOSITE)
    if (d->haveComposite && QCoreApplication::instance()) {
        QCoreApplication::instance()->setEventFilter(oldEventFilter);
    }
#endif

    if (s_manager == this) {
        s_manager = 0;
        delete s_painter;
        s_painter = 0;
    }

    delete d;
}

FdoSelectionManager *FdoSelectionManager::manager()
{
    return s_manager;
}

X11EmbedPainter *FdoSelectionManager::painter()
{
    return s_painter;
}

void FdoSelectionManager::addDamageWatch(QWidget *container, WId client)
{
#if defined(HAVE_XFIXES) && defined(HAVE_XDAMAGE) && defined(HAVE_XCOMPOSITE)
    DamageWatch *damage = new DamageWatch;
    damage->container = container;
    damage->damage = XDamageCreate(QX11Info::display(), client, XDamageReportNonEmpty);
    damageWatches.insert(client, damage);
#endif
}

void FdoSelectionManager::removeDamageWatch(QWidget *container)
{
#if defined(HAVE_XFIXES) && defined(HAVE_XDAMAGE) && defined(HAVE_XCOMPOSITE)
    for (QMap<WId, DamageWatch*>::Iterator it = damageWatches.begin(); it != damageWatches.end(); ++it)
    {
        DamageWatch *damage = *(it);
        if (damage->container == container) {
             XDamageDestroy(QX11Info::display(), damage->damage);
             damageWatches.erase(it);
             delete damage;
             break;
        } 
    }
#endif
}


bool FdoSelectionManager::haveComposite() const
{
    return d->haveComposite;
}


bool FdoSelectionManager::nativeEventFilter(const QByteArray& eventType, void* message, long int* result)
{
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t* ev = static_cast<xcb_generic_event_t *>(message);
        if (XCB_EVENT_RESPONSE_TYPE(ev) == XCB_CLIENT_MESSAGE) {
            auto ce = reinterpret_cast<xcb_client_message_event_t *>(ev);
            qDebug() << "client message";
            if (ce->type == d->opcodeAtom) {
                qDebug() << "YAY";
                return true;
            }
        }
    }

    
//     int ret = 0;
//     xcb_get_geometry_cookie_t geom_c;
//     xcb_get_geometry_reply_t *geom_r;

//     switch(ev->data.data32[1])
//     {
//       case SYSTEM_TRAY_REQUEST_DOCK:
//         geom_c = xcb_get_geometry_unchecked(QX11Info::connection(), ev->window);
// 
//         if(!(geom_r = xcb_get_geometry_reply(QX11Info::connection(), geom_c, NULL)))
//             return -1;
// 
//         if(globalconf.screen->root == geom_r->root)
//             ret = systray_request_handle(ev->data.data32[2], NULL);
// 
//         p_delete(&geom_r);
//         break;
//     }
// 
//     return ret;
// }
    
    
    return false;
}

// bool FdoSelectionManager::x11Event(XEvent *event)
// {
//     if (event->type == ClientMessage) {
//         if (event->xclient.message_type == d->opcodeAtom) {
//             switch (event->xclient.data.l[1]) {
//             case SYSTEM_TRAY_REQUEST_DOCK:
//                 d->handleRequestDock(event->xclient);
//                 return true;
//             case SYSTEM_TRAY_BEGIN_MESSAGE:
//                 d->handleBeginMessage(event->xclient);
//                 return true;
//             case SYSTEM_TRAY_CANCEL_MESSAGE:
//                 d->handleCancelMessage(event->xclient);
//                 return true;
//             }
//         } else if (event->xclient.message_type == d->messageAtom) {
//             d->handleMessageData(event->xclient);
//             return true;
//         }
//     }
// 
//     return QWidget::x11Event(event);
// }


void FdoSelectionManager::initSelection()
{
    if (!s_painter) {
//         s_painter = new X11EmbedPainter;
    }
    s_manager = this;
    
    xcb_client_message_event_t ev;

    auto cookie = xcb_intern_atom(QX11Info::connection(), false, strlen("MANAGER"), "MANAGER");    
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = QX11Info::appRootWindow();
    ev.format = 32;
    ev.type = xcb_intern_atom_reply(QX11Info::connection(), cookie, NULL)->atom;
    ev.data.data32[0] = XCB_CURRENT_TIME;
    ev.data.data32[1] = d->selectionAtom;
    ev.data.data32[2] = winId();
    ev.data.data32[3] = 0;
    ev.data.data32[4] = 0;

    xcb_set_selection_owner(QX11Info::connection(),
                            winId(),
                            d->selectionAtom,
                            XCB_CURRENT_TIME);

    xcb_send_event(QX11Info::connection(), false, QX11Info::appRootWindow(), 0xFFFFFF, (char *) &ev);
    
    qApp->installNativeEventFilter(this);
}
    
    
// 
//     WId selectionOwner = XGetSelectionOwner(d->display, d->selectionAtom);
//     if (selectionOwner != winId()) {
//         // FIXME: Hmmm... Reading the docs on XSetSelectionOwner,
//         // this should not be possible.
//         qDebug() << "Tried to set selection owner to" << winId() << "but it is set to" << selectionOwner;
//         return;
//     }
// 
//     // Prefer the ARGB32 visual if available
//     int nvi;
//     VisualID visual = 0;//     XVisualIDFromVisual((Visual*)QX11Info::appVisual());
//     XVisualInfo templ;
//     templ.visualid = visual;
//     XVisualInfo *xvi = XGetVisualInfo(d->display, VisualIDMask, &templ, &nvi);
//     if (xvi && xvi[0].depth > 16) {
//         templ.screen  = xvi[0].screen;
//         templ.depth   = 32;
//         templ.c_class = TrueColor;
//         XFree(xvi);
//         xvi = XGetVisualInfo(d->display, VisualScreenMask | VisualDepthMask | VisualClassMask,
//                              &templ, &nvi);
//         for (int i = 0; i < nvi; i++) {
//             XRenderPictFormat *format = XRenderFindVisualFormat(d->display, xvi[i].visual);
//             if (format && format->type == PictTypeDirect && format->direct.alphaMask) {
//                 visual = xvi[i].visualid;
//                 break;
//             }
//         }
//         XFree(xvi);
//     }
//     XChangeProperty(d->display, winId(), d->visualAtom, XA_VISUALID, 32,
//                     PropModeReplace, (const unsigned char*)&visual, 1);
// 
//     if (!s_painter) {
//         s_painter = new X11EmbedPainter;
//     }
//     s_manager = this;
// 
//     WId root = QX11Info::appRootWindow();
//     XClientMessageEvent xev;
// 
//     xev.type = ClientMessage;
//     xev.window = root;
//     xev.message_type = XInternAtom(d->display, "MANAGER", false);
//     xev.format = 32;
//     xev.data.l[0] = CurrentTime;
//     xev.data.l[1] = d->selectionAtom;
//     xev.data.l[2] = winId();
//     xev.data.l[3] = 0;
//     xev.data.l[4] = 0;
// 
//     XSendEvent(d->display, root, false, StructureNotifyMask, (XEvent*)&xev);
// }


void FdoSelectionManagerPrivate::handleRequestDock(xcb_window_t wid, void*)
{
//     const WId winId = (WId)event.data.l[2];
    
    qDebug() << "DOCK REQUESTED!";

//     if (tasks.contains(winId)) {
//         qDebug() << "got a dock request from an already existing task";
//         return;
//     }

//     FdoTask *task = new FdoTask(winId, q);

//     tasks[winId] = task;
//     q->connect(task, SIGNAL(taskDeleted(WId)), q, SLOT(cleanupTask(WId)));

//     emit q->taskCreated(task);

}

// void FdoSelectionManager::cleanupTask(WId winId)
// {
//     d->tasks.remove(winId);
// }


// void FdoSelectionManagerPrivate::handleBeginMessage(const XClientMessageEvent &event)
// {
//     const WId winId = event.window;
// 
//     MessageRequest request;
//     request.messageId = event.data.l[4];
//     request.timeout = event.data.l[2];
//     request.bytesRemaining = event.data.l[3];
// 
//     if (request.bytesRemaining) {
//         messageRequests[winId] = request;
//     }
// }

/*
void FdoSelectionManagerPrivate::handleMessageData(const XClientMessageEvent &event)
{
    const WId winId = event.window;
    const char *messageData = event.data.b;

    if (!messageRequests.contains(winId)) {
        qDebug() << "Unexpected message data from" << winId;
        return;
    }

    MessageRequest &request = messageRequests[winId];
    const int messageSize = qMin(request.bytesRemaining, 20l);
    request.bytesRemaining -= messageSize;
    request.message += QByteArray(messageData, messageSize);

    if (request.bytesRemaining == 0) {
        createNotification(winId);
        messageRequests.remove(winId);
    }
}*/
/*

void FdoSelectionManagerPrivate::createNotification(WId winId)
{
    if (!tasks.contains(winId)) {
        qDebug() << "message request from unknown task" << winId;
        return;
    }

    MessageRequest &request = messageRequests[winId];
    Task *task = tasks[winId];

    QString message = QString::fromUtf8(request.message);
    message = QTextDocument(message).toHtml();

    if (!notificationsEngine) {
        notificationsEngine = Plasma::DataEngineManager::self()->loadEngine("notifications");
    }
    //FIXME: who is the source in this case?
    Plasma::Service *service = notificationsEngine->serviceForSource("notification");
    KConfigGroup op = service->operationDescription("createNotification");

    if (op.isValid()) {
        op.writeEntry("appName", task->name());
        //FIXME: find a way to pass icons trough here
        op.writeEntry("appIcon", task->name());

        //op.writeEntry("summary", task->name());
        op.writeEntry("body", message);
        op.writeEntry("timeout", (int)request.timeout);
        KJob *job = service->startOperationCall(op);
        QObject::connect(job, SIGNAL(finished(KJob*)), service, SLOT(deleteLater()));
    } else {
        delete service;
        qDebug() << "invalid operation";
    }
}*/


//     void FdoSelectionManagerPrivate::handleCancelMessage(const XClientMessageEvent &event)
//     {
//         const WId winId = event.window;
//         const long messageId = event.data.l[2];
// 
//         if (messageRequests.contains(winId) && messageRequests[winId].messageId == messageId) {
//             messageRequests.remove(winId);
//         }
//     }

// }
