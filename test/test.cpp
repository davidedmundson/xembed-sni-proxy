#include "test.h"

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtDBus/QtDBus>

#include <QSystemTrayIcon>
#include <QProcess>
#include <QVariant>

#include "statusnotifieritem_interface.h"

#include "../snidbus.h"
#include "fakewatcher.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setDesktopSettingsAware(false); //disable KDE's SNI integration
    XembedSniTest tc;
    return QTest::qExec(&tc, argc, argv);
}


XembedSniTest::XembedSniTest():
    QObject(),
    m_xembedSniProxyProcess(new QProcess(this)),
    m_watcher(new FakeWatcher(this))
{
    qDBusRegisterMetaType<KDbusImageStruct>();
    qDBusRegisterMetaType<KDbusImageVector>();
    qDBusRegisterMetaType<KDbusToolTipStruct>();
}


void XembedSniTest::initTestCase()
{
    qDebug() << "starting xembed sni proxy test. Be sure to close any other system tray users, and any SNI watchers";

    m_xembedSniProxyProcess->start(QStringLiteral("xembedsniproxy")); //TODO find the one from the build dir

    m_xembedSniProxyProcess->waitForStarted();
    QTest::qSleep(1000); //slight grace time for our proxy to be full started

    QVERIFY(QSystemTrayIcon::isSystemTrayAvailable());

    QSignalSpy spy(m_watcher, SIGNAL(StatusNotifierItemRegistered(QString)));

    m_systemTray = new QSystemTrayIcon(QIcon::fromTheme(QStringLiteral("document-edit")), this);
    m_systemTray->show();

    spy.wait();

    QCOMPARE(spy.count(), 1);
    qDebug() << spy;

    QString service = spy.at(0).first().toString();
    m_item = new OrgKdeStatusNotifierItem(service, QStringLiteral("/StatusNotifierItem"), QDBusConnection::sessionBus(), this);
}

void XembedSniTest::propertyValues()
{
    //most the good stuff happens in init if we get this far with have an SNI

    //if we make a normal blocking Property call we get a deadlock...as it is still interacting with our systray
    QDBusMessage message = QDBusMessage::createMethodCall(m_item->service(),
                                                          m_item->path(), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));

    message << m_item->interface();
    QDBusPendingCall call = m_item->connection().asyncCall(message);
    QDBusPendingCallWatcher dbusCallWatcher(call, this);
    QSignalSpy spy(&dbusCallWatcher, SIGNAL(finished(QDBusPendingCallWatcher*)));
    spy.wait();
    QDBusPendingReply<QVariantMap> reply = dbusCallWatcher;
    QCOMPARE(spy.count(), 1);

    QVariantMap itemProperties = reply.value();
    QCOMPARE(itemProperties[QStringLiteral("Category")].toString(), QLatin1String("ApplicationStatus"));
    QCOMPARE(itemProperties[QStringLiteral("Id")].toString(), QLatin1String("xembedsniproxy_test"));
    QCOMPARE(itemProperties[QStringLiteral("Title")].toString(), QLatin1String("xembedsniproxy_test"));
    QCOMPARE(itemProperties[QStringLiteral("Status")].toString(), QLatin1String("Passive"));
}

void XembedSniTest::damage()
{
    QSignalSpy spy(m_item, SIGNAL(NewIcon()));
    m_systemTray->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    spy.wait();
    QCOMPARE(spy.count(), 1);
}

void XembedSniTest::activate()
{
    QSignalSpy spy(m_systemTray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)));
    m_item->Activate(20,20);
    spy.wait();
    QCOMPARE(spy.count(), 1);
}


