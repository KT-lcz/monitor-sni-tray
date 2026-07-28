#ifndef TRAYAPPLICATION_H
#define TRAYAPPLICATION_H

#include "systemstats.h"

#include <QObject>
#include <QScopedPointer>
#include <QThread>
#include <QTimer>

#include <dbus/dbus.h>

class QSocketNotifier;

class MonitorStatsWorker : public QObject
{
    Q_OBJECT

public:
    explicit MonitorStatsWorker(QObject *parent = nullptr);

public Q_SLOTS:
    void sample();

Q_SIGNALS:
    void statsReady(const CpuMemStats &stats);

private:
    QScopedPointer<CpuMemStatsReader> m_statsReader;
};

class MonitorTrayApplication : public QObject
{
    Q_OBJECT

public:
    explicit MonitorTrayApplication(QObject *parent = nullptr);
    ~MonitorTrayApplication() override;

    bool isAvailable() const;

private Q_SLOTS:
    void processDBus();
    void requestSample();
    void applyStats(const CpuMemStats &stats);

private:
    static DBusHandlerResult handleObjectMessage(DBusConnection *connection, DBusMessage *message, void *userData);
    static DBusHandlerResult handleMenuObjectMessage(DBusConnection *connection, DBusMessage *message, void *userData);
    static DBusHandlerResult handleBusMessage(DBusConnection *connection, DBusMessage *message, void *userData);

    DBusHandlerResult handleMessage(DBusMessage *message);
    DBusHandlerResult handleMenuMessage(DBusMessage *message);
    DBusHandlerResult handlePropertiesMessage(DBusMessage *message);
    bool setupDBus();
    void registerWithWatcher();
    void sendSignal(const char *name, const QString &argument = QString());
    void appendProperty(DBusMessageIter *iterator, const QString &name) const;
    void appendPropertyEntry(DBusMessageIter *iterator, const char *name) const;
    void appendIconPixmaps(DBusMessageIter *iterator) const;
    void appendMenuLayout(DBusMessageIter *iterator, int itemId, int recursionDepth) const;
    void appendMenuProperties(DBusMessageIter *iterator, int itemId) const;
    bool appendMenuPropertyValue(DBusMessageIter *iterator, int itemId, const QString &name) const;
    QVector<int> menuChildren(int itemId) const;
    QString menuLabel(int itemId) const;
    bool menuItemEnabled(int itemId) const;
    int selectedGpuFanModeItem() const;
    void setGpuFanMode(int itemId);
    void showLocalMenu(int x, int y);
    void sendMenuItemsUpdated();
    void sendMenuLayoutUpdated();
    QString toolTipText() const;
    void stopWorker();

    DBusConnection *m_connection = nullptr;
    QSocketNotifier *m_busNotifier = nullptr;
    QString m_serviceName;
    QTimer m_sampleTimer;
    QTimer m_dispatchTimer;
    QThread m_statsThread;
    MonitorStatsWorker *m_statsWorker = nullptr;
    bool m_samplePending = false;
    bool m_available = false;
    dbus_uint32_t m_menuRevision = 1;
    int m_diskItemCount = 1;
    int m_requestedGpuFanModeItem = 0;
    bool m_gpuFanSettingPending = false;
    CpuMemStats m_stats;
};

#endif // TRAYAPPLICATION_H
