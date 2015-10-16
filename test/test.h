#ifndef TEST_H
#define TEST_H

#include <QObject>
#include "../snidbus.h"

class FakeWatcher;
class QSystemTrayIcon;
class QProcess;
class OrgKdeStatusNotifierItem;

class XembedSniTest : public QObject
{
    Q_OBJECT
public:
    XembedSniTest();
private slots:
    void initTestCase();
    void propertyValues();
    void damage();
    void activate();

private:
    QProcess *m_xembedSniProxyProcess;
    OrgKdeStatusNotifierItem *m_item;
    QSystemTrayIcon *m_systemTray;
    FakeWatcher *m_watcher;
};

#endif