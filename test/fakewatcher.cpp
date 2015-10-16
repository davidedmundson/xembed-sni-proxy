#include "fakewatcher.h"
#include "statusnotifierwatcheradaptor.h"

#include <QDBusConnection>

FakeWatcher::FakeWatcher(QObject* parent): QObject(parent)
{
    new StatusNotifierWatcherAdaptor(this);

    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.StatusNotifierWatcher"));
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/StatusNotifierWatcher"), this);
}


void FakeWatcher::RegisterStatusNotifierHost(const QString& service)
{

}

void FakeWatcher::RegisterStatusNotifierItem(const QString &service)
{
    emit StatusNotifierItemRegistered(service);
}
