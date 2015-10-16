#ifndef FAKEWATCHER_H
#define FAKEWATCHER_H

#include <QObject>

class FakeWatcher : public QObject
{
    Q_OBJECT
public:
    FakeWatcher(QObject *parent=0);

    //DBUS
    void RegisterStatusNotifierItem(const QString &service);
    void RegisterStatusNotifierHost(const QString &service);

Q_SIGNALS:
     void StatusNotifierItemRegistered(const QString &notifierItemId);
};

#endif