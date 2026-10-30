#include "trayapplication.h"

#include <QActionGroup>
#include <QByteArray>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMenu>
#include <QPainter>
#include <QPoint>
#include <QProcess>
#include <QSocketNotifier>
#include <QStringList>
#include <QtEndian>

#include <unistd.h>

namespace {

constexpr const char kItemPath[] = "/StatusNotifierItem";
constexpr const char kItemInterface[] = "org.kde.StatusNotifierItem";
constexpr const char kMenuPath[] = "/MenuBar";
constexpr const char kMenuInterface[] = "com.canonical.dbusmenu";
constexpr const char kWatcherService[] = "org.kde.StatusNotifierWatcher";
constexpr const char kWatcherPath[] = "/StatusNotifierWatcher";
constexpr const char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";
constexpr int kGpuFanAutoItem = 20;
constexpr int kGpuFanLowItem = 21;
constexpr int kGpuFanHighItem = 22;
constexpr int kGpuFanFullItem = 23;

#ifndef MONITOR_GPU_FAN_HELPER_PATH
#define MONITOR_GPU_FAN_HELPER_PATH "monitor-gpu-fan-helper"
#endif

bool isGpuFanModeItem(int itemId)
{
    return itemId >= kGpuFanAutoItem && itemId <= kGpuFanFullItem;
}

QString gpuFanModeArgument(int itemId)
{
    switch (itemId) {
    case kGpuFanAutoItem:
        return QStringLiteral("auto");
    case kGpuFanLowItem:
        return QStringLiteral("low");
    case kGpuFanHighItem:
        return QStringLiteral("high");
    case kGpuFanFullItem:
        return QStringLiteral("full");
    default:
        return {};
    }
}

int gpuFanModeItem(const CpuMemStats::GpuStats &stats)
{
    if (!stats.fanValid) {
        return 0;
    }
    if (stats.automaticFan) {
        return kGpuFanAutoItem;
    }
    if (stats.fanPercent >= 95) {
        return kGpuFanFullItem;
    }
    if (stats.fanPercent >= 65) {
        return kGpuFanHighItem;
    }
    return kGpuFanLowItem;
}

QString gpuFanHelperPath()
{
    const QString overridePath = qEnvironmentVariable("MONITOR_GPU_FAN_HELPER");
    if (!overridePath.isEmpty()) {
        return overridePath;
    }

    const QString adjacentPath = QCoreApplication::applicationDirPath()
        + QStringLiteral("/monitor-gpu-fan-helper");
    return QFileInfo::exists(adjacentPath)
        ? adjacentPath : QStringLiteral(MONITOR_GPU_FAN_HELPER_PATH);
}

const char kIntrospectionXml[] = R"XML(
<node>
  <interface name="org.kde.StatusNotifierItem">
    <property name="Category" type="s" access="read"/>
    <property name="Id" type="s" access="read"/>
    <property name="Title" type="s" access="read"/>
    <property name="Status" type="s" access="read"/>
    <property name="WindowId" type="i" access="read"/>
    <property name="IconName" type="s" access="read"/>
    <property name="IconPixmap" type="a(iiay)" access="read"/>
    <property name="OverlayIconName" type="s" access="read"/>
    <property name="OverlayIconPixmap" type="a(iiay)" access="read"/>
    <property name="AttentionIconName" type="s" access="read"/>
    <property name="AttentionIconPixmap" type="a(iiay)" access="read"/>
    <property name="AttentionMovieName" type="s" access="read"/>
    <property name="ToolTip" type="(sa(iiay)ss)" access="read"/>
    <property name="ItemIsMenu" type="b" access="read"/>
    <property name="Menu" type="o" access="read"/>
    <property name="IconThemePath" type="s" access="read"/>
    <method name="ContextMenu"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="Activate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="SecondaryActivate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="Scroll"><arg type="i" direction="in"/><arg type="s" direction="in"/></method>
    <signal name="NewTitle"/>
    <signal name="NewIcon"/>
    <signal name="NewAttentionIcon"/>
    <signal name="NewOverlayIcon"/>
    <signal name="NewToolTip"/>
    <signal name="NewStatus"><arg type="s"/></signal>
  </interface>
  <interface name="org.freedesktop.DBus.Properties">
    <method name="Get"><arg type="s" direction="in"/><arg type="s" direction="in"/><arg type="v" direction="out"/></method>
    <method name="GetAll"><arg type="s" direction="in"/><arg type="a{sv}" direction="out"/></method>
    <method name="Set"><arg type="s" direction="in"/><arg type="s" direction="in"/><arg type="v" direction="in"/></method>
  </interface>
  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect"><arg type="s" direction="out"/></method>
  </interface>
  <interface name="org.freedesktop.DBus.Peer">
    <method name="Ping"/>
  </interface>
</node>
)XML";

const char kMenuIntrospectionXml[] = R"XML(
<node>
  <interface name="com.canonical.dbusmenu">
    <property name="Version" type="u" access="read"/>
    <property name="TextDirection" type="s" access="read"/>
    <property name="Status" type="s" access="read"/>
    <property name="IconThemePath" type="as" access="read"/>
    <method name="GetLayout">
      <arg name="parentId" type="i" direction="in"/>
      <arg name="recursionDepth" type="i" direction="in"/>
      <arg name="propertyNames" type="as" direction="in"/>
      <arg name="revision" type="u" direction="out"/>
      <arg name="layout" type="(ia{sv}av)" direction="out"/>
    </method>
    <method name="GetGroupProperties">
      <arg name="ids" type="ai" direction="in"/>
      <arg name="propertyNames" type="as" direction="in"/>
      <arg name="properties" type="a(ia{sv})" direction="out"/>
    </method>
    <method name="GetProperty">
      <arg name="id" type="i" direction="in"/>
      <arg name="name" type="s" direction="in"/>
      <arg name="value" type="v" direction="out"/>
    </method>
    <method name="Event">
      <arg name="id" type="i" direction="in"/>
      <arg name="eventId" type="s" direction="in"/>
      <arg name="data" type="v" direction="in"/>
      <arg name="timestamp" type="u" direction="in"/>
    </method>
    <method name="AboutToShow">
      <arg name="id" type="i" direction="in"/>
      <arg name="needUpdate" type="b" direction="out"/>
    </method>
    <signal name="ItemsPropertiesUpdated">
      <arg name="updatedProps" type="a(ia{sv})"/>
      <arg name="removedProps" type="a(ias)"/>
    </signal>
    <signal name="LayoutUpdated"><arg name="revision" type="u"/><arg name="parent" type="i"/></signal>
  </interface>
  <interface name="org.freedesktop.DBus.Properties">
    <method name="Get"><arg type="s" direction="in"/><arg type="s" direction="in"/><arg type="v" direction="out"/></method>
    <method name="GetAll"><arg type="s" direction="in"/><arg type="a{sv}" direction="out"/></method>
  </interface>
  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect"><arg type="s" direction="out"/></method>
  </interface>
</node>
)XML";

const QStringList &propertyNames()
{
    static const QStringList names = {
        QStringLiteral("Category"), QStringLiteral("Id"), QStringLiteral("Title"),
        QStringLiteral("Status"), QStringLiteral("WindowId"), QStringLiteral("IconName"),
        QStringLiteral("IconPixmap"), QStringLiteral("OverlayIconName"),
        QStringLiteral("OverlayIconPixmap"), QStringLiteral("AttentionIconName"),
        QStringLiteral("AttentionIconPixmap"), QStringLiteral("AttentionMovieName"),
        QStringLiteral("ToolTip"), QStringLiteral("ItemIsMenu"), QStringLiteral("Menu"),
        QStringLiteral("IconThemePath")
    };
    return names;
}

const char *propertySignature(const QString &name)
{
    if (name == QStringLiteral("WindowId")) {
        return "i";
    }
    if (name == QStringLiteral("ItemIsMenu")) {
        return "b";
    }
    if (name == QStringLiteral("Menu")) {
        return "o";
    }
    if (name == QStringLiteral("IconPixmap") || name == QStringLiteral("OverlayIconPixmap")
        || name == QStringLiteral("AttentionIconPixmap")) {
        return "a(iiay)";
    }
    if (name == QStringLiteral("ToolTip")) {
        return "(sa(iiay)ss)";
    }
    return "s";
}

void appendString(DBusMessageIter *iterator, const QString &value, int type = DBUS_TYPE_STRING)
{
    const QByteArray utf8 = value.toUtf8();
    const char *data = utf8.constData();
    dbus_message_iter_append_basic(iterator, type, &data);
}

void appendStringProperty(DBusMessageIter *properties, const char *name, const QString &value)
{
    DBusMessageIter entry;
    DBusMessageIter variant;
    dbus_message_iter_open_container(properties, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    appendString(&variant, value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(properties, &entry);
}

void appendBooleanProperty(DBusMessageIter *properties, const char *name, bool value)
{
    DBusMessageIter entry;
    DBusMessageIter variant;
    dbus_message_iter_open_container(properties, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant);
    dbus_bool_t booleanValue = value;
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &booleanValue);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(properties, &entry);
}

void appendInt32Property(DBusMessageIter *properties, const char *name, int value)
{
    DBusMessageIter entry;
    DBusMessageIter variant;
    dbus_message_iter_open_container(properties, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "i", &variant);
    dbus_int32_t intValue = value;
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_INT32, &intValue);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(properties, &entry);
}

bool isMenuSeparator(int itemId)
{
    return itemId == 5 || itemId == 7 || itemId == 10 || itemId == 11;
}

QString formatMemory(quint64 kb)
{
    const double gib = static_cast<double>(kb) / 1024.0 / 1024.0;
    return QStringLiteral("%1 GiB").arg(gib, 0, 'f', 1);
}

QString formatBytes(quint64 bytes)
{
    const double gib = static_cast<double>(bytes) / 1024.0 / 1024.0 / 1024.0;
    if (gib < 1024.0) {
        return QStringLiteral("%1 GiB").arg(gib, 0, 'f', 1);
    }
    return QStringLiteral("%1 TiB").arg(gib / 1024.0, 0, 'f', 1);
}

void sendReply(DBusConnection *connection, DBusMessage *request, DBusMessage *reply)
{
    if (!reply) {
        return;
    }
    dbus_connection_send(connection, reply, nullptr);
    dbus_message_unref(reply);
    dbus_connection_flush(connection);
    Q_UNUSED(request)
}

void sendError(DBusConnection *connection, DBusMessage *request, const char *name, const char *text)
{
    sendReply(connection, request, dbus_message_new_error(request, name, text));
}

QString formatRate(double bytesPerSecond)
{
    const double kib = bytesPerSecond / 1024.0;
    if (kib < 1024.0) {
        return QStringLiteral("%1 KiB/s").arg(kib, 0, 'f', kib >= 100.0 ? 0 : 1);
    }
    const double mib = kib / 1024.0;
    return QStringLiteral("%1 MiB/s").arg(mib, 0, 'f', mib >= 100.0 ? 0 : 1);
}

QImage createTrayIcon(int size)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal scale = size / 32.0;
    painter.scale(scale, scale);
    painter.setPen(QPen(QColor(235, 235, 235), 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(QColor(55, 124, 214));
    painter.drawRoundedRect(QRectF(7, 7, 18, 18), 4, 4);
    painter.setPen(QPen(QColor(235, 235, 235), 1.7, Qt::SolidLine, Qt::RoundCap));
    for (int offset : { 10, 16, 22 }) {
        painter.drawLine(QPointF(4, offset), QPointF(7, offset));
        painter.drawLine(QPointF(25, offset), QPointF(28, offset));
        painter.drawLine(QPointF(offset, 4), QPointF(offset, 7));
        painter.drawLine(QPointF(offset, 25), QPointF(offset, 28));
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(235, 235, 235));
    painter.drawRoundedRect(QRectF(11, 11, 10, 10), 2, 2);
    return image;
}

} // namespace

MonitorStatsWorker::MonitorStatsWorker(QObject *parent)
    : QObject(parent)
{
}

void MonitorStatsWorker::sample()
{
    if (QThread::currentThread()->isInterruptionRequested()) {
        return;
    }
    if (!m_statsReader) {
        m_statsReader.reset(new CpuMemStatsReader);
    }
    Q_EMIT statsReady(m_statsReader->sample());
}

MonitorTrayApplication::MonitorTrayApplication(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<CpuMemStats>("CpuMemStats");

    m_statsWorker = new MonitorStatsWorker;
    m_statsWorker->moveToThread(&m_statsThread);
    connect(&m_statsThread, &QThread::finished, m_statsWorker, &QObject::deleteLater);
    connect(m_statsWorker, &MonitorStatsWorker::statsReady,
            this, &MonitorTrayApplication::applyStats);
    m_statsThread.start();

    m_sampleTimer.setInterval(1000);
    connect(&m_sampleTimer, &QTimer::timeout, this, &MonitorTrayApplication::requestSample);

    m_dispatchTimer.setInterval(1000);
    connect(&m_dispatchTimer, &QTimer::timeout, this, &MonitorTrayApplication::processDBus);

    m_available = setupDBus();
    if (m_available) {
        m_sampleTimer.start();
        m_dispatchTimer.start();
        requestSample();
    }
}

MonitorTrayApplication::~MonitorTrayApplication()
{
    m_sampleTimer.stop();
    m_dispatchTimer.stop();
    stopWorker();

    if (m_busNotifier) {
        m_busNotifier->setEnabled(false);
    }
    if (m_connection) {
        dbus_connection_remove_filter(m_connection, &MonitorTrayApplication::handleBusMessage, this);
        dbus_connection_unregister_object_path(m_connection, kItemPath);
        dbus_connection_unregister_object_path(m_connection, kMenuPath);
        if (!m_serviceName.isEmpty()) {
            DBusError error;
            dbus_error_init(&error);
            dbus_bus_release_name(m_connection, m_serviceName.toUtf8().constData(), &error);
            dbus_error_free(&error);
        }
        dbus_connection_close(m_connection);
        dbus_connection_unref(m_connection);
        m_connection = nullptr;
    }
}

bool MonitorTrayApplication::isAvailable() const
{
    return m_available;
}

bool MonitorTrayApplication::setupDBus()
{
    DBusError error;
    dbus_error_init(&error);
    m_connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (!m_connection) {
        qCritical("Unable to connect to the session D-Bus: %s", error.message ? error.message : "unknown error");
        dbus_error_free(&error);
        return false;
    }
    dbus_connection_set_exit_on_disconnect(m_connection, false);

    m_serviceName = QStringLiteral("org.kde.StatusNotifierItem-%1-1").arg(getpid());
    const QByteArray serviceName = m_serviceName.toUtf8();
    const int requestResult = dbus_bus_request_name(
        m_connection, serviceName.constData(), DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
    if (requestResult != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        qCritical("Unable to own SNI service %s: %s", serviceName.constData(),
                  error.message ? error.message : "name already owned");
        dbus_error_free(&error);
        return false;
    }

    static DBusObjectPathVTable objectVTable = {
        nullptr, &MonitorTrayApplication::handleObjectMessage, nullptr, nullptr, nullptr, nullptr
    };
    if (!dbus_connection_register_object_path(m_connection, kItemPath, &objectVTable, this)) {
        qCritical("Unable to export the StatusNotifierItem object");
        return false;
    }
    static DBusObjectPathVTable menuObjectVTable = {
        nullptr, &MonitorTrayApplication::handleMenuObjectMessage, nullptr, nullptr, nullptr, nullptr
    };
    if (!dbus_connection_register_object_path(m_connection, kMenuPath, &menuObjectVTable, this)) {
        qCritical("Unable to export the DBusMenu object");
        return false;
    }

    if (!dbus_connection_add_filter(m_connection, &MonitorTrayApplication::handleBusMessage, this, nullptr)) {
        qCritical("Unable to add the D-Bus watcher filter");
        return false;
    }
    dbus_bus_add_match(
        m_connection,
        "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',"
        "member='NameOwnerChanged',arg0='org.kde.StatusNotifierWatcher'",
        &error);
    if (dbus_error_is_set(&error)) {
        qWarning("Unable to monitor the SNI watcher: %s", error.message);
        dbus_error_free(&error);
    }

    int busFd = -1;
    if (dbus_connection_get_unix_fd(m_connection, &busFd) && busFd >= 0) {
        m_busNotifier = new QSocketNotifier(busFd, QSocketNotifier::Read, this);
        connect(m_busNotifier, &QSocketNotifier::activated, this,
                [this](auto...) { processDBus(); });
    }

    registerWithWatcher();
    return true;
}

void MonitorTrayApplication::registerWithWatcher()
{
    if (!m_connection) {
        return;
    }

    DBusError error;
    dbus_error_init(&error);
    const dbus_bool_t hasWatcher = dbus_bus_name_has_owner(m_connection, kWatcherService, &error);
    if (dbus_error_is_set(&error)) {
        dbus_error_free(&error);
        return;
    }
    if (!hasWatcher) {
        qWarning("No org.kde.StatusNotifierWatcher is currently available; waiting for one");
        return;
    }

    DBusMessage *message = dbus_message_new_method_call(
        kWatcherService, kWatcherPath, kWatcherService, "RegisterStatusNotifierItem");
    if (!message) {
        return;
    }
    const QByteArray serviceName = m_serviceName.toUtf8();
    const char *service = serviceName.constData();
    dbus_message_append_args(message, DBUS_TYPE_STRING, &service, DBUS_TYPE_INVALID);
    dbus_connection_send(m_connection, message, nullptr);
    dbus_message_unref(message);
    dbus_connection_flush(m_connection);
}

DBusHandlerResult MonitorTrayApplication::handleObjectMessage(
    DBusConnection *, DBusMessage *message, void *userData)
{
    return static_cast<MonitorTrayApplication *>(userData)->handleMessage(message);
}

DBusHandlerResult MonitorTrayApplication::handleMenuObjectMessage(
    DBusConnection *, DBusMessage *message, void *userData)
{
    return static_cast<MonitorTrayApplication *>(userData)->handleMenuMessage(message);
}

DBusHandlerResult MonitorTrayApplication::handleBusMessage(
    DBusConnection *, DBusMessage *message, void *userData)
{
    if (!dbus_message_is_signal(message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char *name = nullptr;
    const char *oldOwner = nullptr;
    const char *newOwner = nullptr;
    DBusError error;
    dbus_error_init(&error);
    if (dbus_message_get_args(message, &error,
                              DBUS_TYPE_STRING, &name,
                              DBUS_TYPE_STRING, &oldOwner,
                              DBUS_TYPE_STRING, &newOwner,
                              DBUS_TYPE_INVALID)
        && name && qstrcmp(name, kWatcherService) == 0 && newOwner && *newOwner) {
        static_cast<MonitorTrayApplication *>(userData)->registerWithWatcher();
    }
    dbus_error_free(&error);
    return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult MonitorTrayApplication::handleMessage(DBusMessage *message)
{
    if (dbus_message_is_method_call(message, DBUS_INTERFACE_INTROSPECTABLE, "Introspect")) {
        DBusMessage *reply = dbus_message_new_method_return(message);
        const char *xml = kIntrospectionXml;
        dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
        sendReply(m_connection, message, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(message, DBUS_INTERFACE_PEER, "Ping")) {
        sendReply(m_connection, message, dbus_message_new_method_return(message));
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_has_interface(message, kPropertiesInterface)) {
        return handlePropertiesMessage(message);
    }

    const bool menuRequest = dbus_message_is_method_call(message, kItemInterface, "Activate")
        || dbus_message_is_method_call(message, kItemInterface, "SecondaryActivate")
        || dbus_message_is_method_call(message, kItemInterface, "ContextMenu");
    if (menuRequest) {
        dbus_int32_t x = 0;
        dbus_int32_t y = 0;
        DBusError error;
        dbus_error_init(&error);
        if (dbus_message_get_args(message, &error,
                                  DBUS_TYPE_INT32, &x,
                                  DBUS_TYPE_INT32, &y,
                                  DBUS_TYPE_INVALID)) {
            showLocalMenu(x, y);
        }
        dbus_error_free(&error);
    } else if (!dbus_message_is_method_call(message, kItemInterface, "Scroll")) {
        sendError(m_connection, message, DBUS_ERROR_UNKNOWN_METHOD, "Unknown StatusNotifierItem method");
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    sendReply(m_connection, message, dbus_message_new_method_return(message));
    return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult MonitorTrayApplication::handleMenuMessage(DBusMessage *message)
{
    if (dbus_message_is_method_call(message, DBUS_INTERFACE_INTROSPECTABLE, "Introspect")) {
        DBusMessage *reply = dbus_message_new_method_return(message);
        const char *xml = kMenuIntrospectionXml;
        dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
        sendReply(m_connection, message, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(message, DBUS_INTERFACE_PEER, "Ping")) {
        sendReply(m_connection, message, dbus_message_new_method_return(message));
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(message, kPropertiesInterface, "Get")) {
        const char *interfaceName = nullptr;
        const char *propertyName = nullptr;
        DBusError error;
        dbus_error_init(&error);
        if (!dbus_message_get_args(message, &error,
                                   DBUS_TYPE_STRING, &interfaceName,
                                   DBUS_TYPE_STRING, &propertyName,
                                   DBUS_TYPE_INVALID)
            || !interfaceName || qstrcmp(interfaceName, kMenuInterface) != 0) {
            dbus_error_free(&error);
            sendError(m_connection, message, DBUS_ERROR_INVALID_ARGS, "Invalid DBusMenu property request");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_error_free(&error);

        DBusMessage *reply = dbus_message_new_method_return(message);
        DBusMessageIter iterator;
        DBusMessageIter variant;
        dbus_message_iter_init_append(reply, &iterator);
        if (qstrcmp(propertyName, "Version") == 0) {
            dbus_message_iter_open_container(&iterator, DBUS_TYPE_VARIANT, "u", &variant);
            dbus_uint32_t version = 3;
            dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &version);
        } else if (qstrcmp(propertyName, "TextDirection") == 0
                   || qstrcmp(propertyName, "Status") == 0) {
            dbus_message_iter_open_container(&iterator, DBUS_TYPE_VARIANT, "s", &variant);
            appendString(&variant, qstrcmp(propertyName, "Status") == 0
                                      ? QStringLiteral("normal") : QStringLiteral("ltr"));
        } else if (qstrcmp(propertyName, "IconThemePath") == 0) {
            dbus_message_iter_open_container(&iterator, DBUS_TYPE_VARIANT, "as", &variant);
            DBusMessageIter paths;
            dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "s", &paths);
            dbus_message_iter_close_container(&variant, &paths);
        } else {
            dbus_message_unref(reply);
            sendError(m_connection, message, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown DBusMenu property");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_message_iter_close_container(&iterator, &variant);
        sendReply(m_connection, message, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(message, kPropertiesInterface, "GetAll")) {
        DBusMessage *reply = dbus_message_new_method_return(message);
        DBusMessageIter iterator;
        DBusMessageIter properties;
        dbus_message_iter_init_append(reply, &iterator);
        dbus_message_iter_open_container(&iterator, DBUS_TYPE_ARRAY, "{sv}", &properties);

        const char *versionName = "Version";
        DBusMessageIter entry;
        DBusMessageIter variant;
        dbus_message_iter_open_container(&properties, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &versionName);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "u", &variant);
        dbus_uint32_t version = 3;
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &version);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&properties, &entry);
        appendStringProperty(&properties, "TextDirection", QStringLiteral("ltr"));
        appendStringProperty(&properties, "Status", QStringLiteral("normal"));

        dbus_message_iter_close_container(&iterator, &properties);
        sendReply(m_connection, message, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(message, kMenuInterface, "GetLayout")) {
        DBusMessageIter request;
        dbus_int32_t parentId = 0;
        dbus_int32_t recursionDepth = -1;
        if (!dbus_message_iter_init(message, &request)
            || dbus_message_iter_get_arg_type(&request) != DBUS_TYPE_INT32) {
            sendError(m_connection, message, DBUS_ERROR_INVALID_ARGS, "Expected a parent menu id");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_message_iter_get_basic(&request, &parentId);
        if (dbus_message_iter_next(&request)
            && dbus_message_iter_get_arg_type(&request) == DBUS_TYPE_INT32) {
            dbus_message_iter_get_basic(&request, &recursionDepth);
        }

        DBusMessage *reply = dbus_message_new_method_return(message);
        DBusMessageIter iterator;
        dbus_message_iter_init_append(reply, &iterator);
        dbus_message_iter_append_basic(&iterator, DBUS_TYPE_UINT32, &m_menuRevision);
        appendMenuLayout(&iterator, parentId, recursionDepth);
        sendReply(m_connection, message, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(message, kMenuInterface, "GetGroupProperties")) {
        DBusMessageIter request;
        DBusMessageIter ids;
        if (!dbus_message_iter_init(message, &request)
            || dbus_message_iter_get_arg_type(&request) != DBUS_TYPE_ARRAY) {
            sendError(m_connection, message, DBUS_ERROR_INVALID_ARGS, "Expected menu ids");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_message_iter_recurse(&request, &ids);

        DBusMessage *reply = dbus_message_new_method_return(message);
        DBusMessageIter iterator;
        DBusMessageIter results;
        dbus_message_iter_init_append(reply, &iterator);
        dbus_message_iter_open_container(&iterator, DBUS_TYPE_ARRAY, "(ia{sv})", &results);
        while (dbus_message_iter_get_arg_type(&ids) == DBUS_TYPE_INT32) {
            dbus_int32_t itemId = 0;
            dbus_message_iter_get_basic(&ids, &itemId);
            DBusMessageIter result;
            DBusMessageIter properties;
            dbus_message_iter_open_container(&results, DBUS_TYPE_STRUCT, nullptr, &result);
            dbus_message_iter_append_basic(&result, DBUS_TYPE_INT32, &itemId);
            dbus_message_iter_open_container(&result, DBUS_TYPE_ARRAY, "{sv}", &properties);
            appendMenuProperties(&properties, itemId);
            dbus_message_iter_close_container(&result, &properties);
            dbus_message_iter_close_container(&results, &result);
            dbus_message_iter_next(&ids);
        }
        dbus_message_iter_close_container(&iterator, &results);
        sendReply(m_connection, message, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(message, kMenuInterface, "GetProperty")) {
        dbus_int32_t itemId = 0;
        const char *propertyName = nullptr;
        DBusError error;
        dbus_error_init(&error);
        if (!dbus_message_get_args(message, &error,
                                   DBUS_TYPE_INT32, &itemId,
                                   DBUS_TYPE_STRING, &propertyName,
                                   DBUS_TYPE_INVALID)) {
            dbus_error_free(&error);
            sendError(m_connection, message, DBUS_ERROR_INVALID_ARGS, "Invalid menu property request");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_error_free(&error);
        DBusMessage *reply = dbus_message_new_method_return(message);
        DBusMessageIter iterator;
        dbus_message_iter_init_append(reply, &iterator);
        if (!appendMenuPropertyValue(&iterator, itemId, QString::fromUtf8(propertyName))) {
            dbus_message_unref(reply);
            sendError(m_connection, message, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown menu property");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        sendReply(m_connection, message, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(message, kMenuInterface, "Event")) {
        DBusMessageIter request;
        dbus_int32_t itemId = 0;
        const char *eventId = nullptr;
        if (dbus_message_iter_init(message, &request)
            && dbus_message_iter_get_arg_type(&request) == DBUS_TYPE_INT32) {
            dbus_message_iter_get_basic(&request, &itemId);
            if (dbus_message_iter_next(&request)
                && dbus_message_iter_get_arg_type(&request) == DBUS_TYPE_STRING) {
                dbus_message_iter_get_basic(&request, &eventId);
            }
        }
        if (eventId && qstrcmp(eventId, "clicked") == 0 && isGpuFanModeItem(itemId)) {
            setGpuFanMode(itemId);
        }
        sendReply(m_connection, message, dbus_message_new_method_return(message));
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(message, kMenuInterface, "AboutToShow")) {
        dbus_int32_t itemId = -1;
        DBusError error;
        dbus_error_init(&error);
        dbus_message_get_args(message, &error,
                              DBUS_TYPE_INT32, &itemId, DBUS_TYPE_INVALID);
        dbus_error_free(&error);
        if (itemId == 0) {
            requestSample();
        }
        DBusMessage *reply = dbus_message_new_method_return(message);
        dbus_bool_t needUpdate = false;
        dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &needUpdate, DBUS_TYPE_INVALID);
        sendReply(m_connection, message, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    sendError(m_connection, message, DBUS_ERROR_UNKNOWN_METHOD, "Unknown DBusMenu method");
    return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult MonitorTrayApplication::handlePropertiesMessage(DBusMessage *message)
{
    if (dbus_message_is_method_call(message, kPropertiesInterface, "Set")) {
        sendError(m_connection, message, DBUS_ERROR_PROPERTY_READ_ONLY, "All SNI properties are read-only");
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    DBusMessageIter requestIterator;
    if (!dbus_message_iter_init(message, &requestIterator)
        || dbus_message_iter_get_arg_type(&requestIterator) != DBUS_TYPE_STRING) {
        sendError(m_connection, message, DBUS_ERROR_INVALID_ARGS, "Expected an interface name");
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    const char *interfaceName = nullptr;
    dbus_message_iter_get_basic(&requestIterator, &interfaceName);
    if (!interfaceName || qstrcmp(interfaceName, kItemInterface) != 0) {
        sendError(m_connection, message, DBUS_ERROR_UNKNOWN_INTERFACE, "Unknown interface");
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(message, kPropertiesInterface, "Get")) {
        if (!dbus_message_iter_next(&requestIterator)
            || dbus_message_iter_get_arg_type(&requestIterator) != DBUS_TYPE_STRING) {
            sendError(m_connection, message, DBUS_ERROR_INVALID_ARGS, "Expected a property name");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        const char *property = nullptr;
        dbus_message_iter_get_basic(&requestIterator, &property);
        const QString propertyName = QString::fromUtf8(property ? property : "");
        if (!propertyNames().contains(propertyName)) {
            sendError(m_connection, message, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown SNI property");
            return DBUS_HANDLER_RESULT_HANDLED;
        }

        DBusMessage *reply = dbus_message_new_method_return(message);
        DBusMessageIter replyIterator;
        dbus_message_iter_init_append(reply, &replyIterator);
        appendProperty(&replyIterator, propertyName);
        sendReply(m_connection, message, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(message, kPropertiesInterface, "GetAll")) {
        DBusMessage *reply = dbus_message_new_method_return(message);
        DBusMessageIter replyIterator;
        DBusMessageIter properties;
        dbus_message_iter_init_append(reply, &replyIterator);
        dbus_message_iter_open_container(&replyIterator, DBUS_TYPE_ARRAY, "{sv}", &properties);
        for (const QString &property : propertyNames()) {
            appendPropertyEntry(&properties, property.toUtf8().constData());
        }
        dbus_message_iter_close_container(&replyIterator, &properties);
        sendReply(m_connection, message, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    sendError(m_connection, message, DBUS_ERROR_UNKNOWN_METHOD, "Unknown properties method");
    return DBUS_HANDLER_RESULT_HANDLED;
}

void MonitorTrayApplication::appendProperty(DBusMessageIter *iterator, const QString &name) const
{
    DBusMessageIter variant;
    dbus_message_iter_open_container(iterator, DBUS_TYPE_VARIANT, propertySignature(name), &variant);

    if (name == QStringLiteral("Category")) {
        appendString(&variant, QStringLiteral("Hardware"));
    } else if (name == QStringLiteral("Id")) {
        appendString(&variant, QStringLiteral("monitor-sni-tray"));
    } else if (name == QStringLiteral("Title")) {
        appendString(&variant, QStringLiteral("硬件监控"));
    } else if (name == QStringLiteral("Status")) {
        appendString(&variant, QStringLiteral("Active"));
    } else if (name == QStringLiteral("WindowId")) {
        dbus_int32_t windowId = 0;
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_INT32, &windowId);
    } else if (name == QStringLiteral("IconName") || name == QStringLiteral("OverlayIconName")
               || name == QStringLiteral("AttentionIconName") || name == QStringLiteral("AttentionMovieName")
               || name == QStringLiteral("IconThemePath")) {
        appendString(&variant, QString());
    } else if (name == QStringLiteral("IconPixmap")) {
        appendIconPixmaps(&variant);
    } else if (name == QStringLiteral("OverlayIconPixmap")
               || name == QStringLiteral("AttentionIconPixmap")) {
        DBusMessageIter array;
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "(iiay)", &array);
        dbus_message_iter_close_container(&variant, &array);
    } else if (name == QStringLiteral("ToolTip")) {
        DBusMessageIter toolTip;
        dbus_message_iter_open_container(&variant, DBUS_TYPE_STRUCT, nullptr, &toolTip);
        appendString(&toolTip, QString());
        appendIconPixmaps(&toolTip);
        appendString(&toolTip, QStringLiteral("硬件监控"));
        appendString(&toolTip, toolTipText());
        dbus_message_iter_close_container(&variant, &toolTip);
    } else if (name == QStringLiteral("ItemIsMenu")) {
        dbus_bool_t itemIsMenu = true;
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &itemIsMenu);
    } else if (name == QStringLiteral("Menu")) {
        appendString(&variant, QString::fromLatin1(kMenuPath), DBUS_TYPE_OBJECT_PATH);
    }

    dbus_message_iter_close_container(iterator, &variant);
}

void MonitorTrayApplication::appendPropertyEntry(DBusMessageIter *iterator, const char *name) const
{
    DBusMessageIter entry;
    dbus_message_iter_open_container(iterator, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name);
    appendProperty(&entry, QString::fromUtf8(name));
    dbus_message_iter_close_container(iterator, &entry);
}

void MonitorTrayApplication::appendIconPixmaps(DBusMessageIter *iterator) const
{
    DBusMessageIter pixmaps;
    dbus_message_iter_open_container(iterator, DBUS_TYPE_ARRAY, "(iiay)", &pixmaps);

    for (int size : { 22, 32, 64 }) {
        const QImage image = createTrayIcon(size).convertToFormat(QImage::Format_ARGB32);
        QByteArray pixels;
        pixels.reserve(size * size * 4);
        for (int y = 0; y < image.height(); ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const quint32 networkPixel = qToBigEndian<quint32>(line[x]);
                pixels.append(reinterpret_cast<const char *>(&networkPixel), sizeof(networkPixel));
            }
        }

        DBusMessageIter pixmap;
        DBusMessageIter byteArray;
        dbus_message_iter_open_container(&pixmaps, DBUS_TYPE_STRUCT, nullptr, &pixmap);
        dbus_int32_t width = image.width();
        dbus_int32_t height = image.height();
        dbus_message_iter_append_basic(&pixmap, DBUS_TYPE_INT32, &width);
        dbus_message_iter_append_basic(&pixmap, DBUS_TYPE_INT32, &height);
        dbus_message_iter_open_container(&pixmap, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &byteArray);
        const unsigned char *pixelData = reinterpret_cast<const unsigned char *>(pixels.constData());
        dbus_message_iter_append_fixed_array(
            &byteArray, DBUS_TYPE_BYTE, &pixelData, static_cast<int>(pixels.size()));
        dbus_message_iter_close_container(&pixmap, &byteArray);
        dbus_message_iter_close_container(&pixmaps, &pixmap);
    }

    dbus_message_iter_close_container(iterator, &pixmaps);
}

void MonitorTrayApplication::appendMenuLayout(
    DBusMessageIter *iterator, int itemId, int recursionDepth) const
{
    DBusMessageIter layout;
    DBusMessageIter properties;
    DBusMessageIter children;
    dbus_message_iter_open_container(iterator, DBUS_TYPE_STRUCT, nullptr, &layout);
    dbus_int32_t id = itemId;
    dbus_message_iter_append_basic(&layout, DBUS_TYPE_INT32, &id);
    dbus_message_iter_open_container(&layout, DBUS_TYPE_ARRAY, "{sv}", &properties);
    appendMenuProperties(&properties, itemId);
    dbus_message_iter_close_container(&layout, &properties);
    dbus_message_iter_open_container(&layout, DBUS_TYPE_ARRAY, "v", &children);

    if (recursionDepth != 0) {
        const int childDepth = recursionDepth < 0 ? -1 : recursionDepth - 1;
        for (int childId : menuChildren(itemId)) {
            DBusMessageIter child;
            dbus_message_iter_open_container(&children, DBUS_TYPE_VARIANT, "(ia{sv}av)", &child);
            appendMenuLayout(&child, childId, childDepth);
            dbus_message_iter_close_container(&children, &child);
        }
    }

    dbus_message_iter_close_container(&layout, &children);
    dbus_message_iter_close_container(iterator, &layout);
}

void MonitorTrayApplication::appendMenuProperties(DBusMessageIter *iterator, int itemId) const
{
    if (itemId == 0) {
        return;
    }
    if (isMenuSeparator(itemId)) {
        appendStringProperty(iterator, "type", QStringLiteral("separator"));
    } else {
        appendStringProperty(iterator, "label", menuLabel(itemId));
        appendBooleanProperty(iterator, "enabled", menuItemEnabled(itemId));
        if (isGpuFanModeItem(itemId)) {
            appendStringProperty(iterator, "toggle-type", QStringLiteral("radio"));
            appendInt32Property(iterator, "toggle-state",
                                selectedGpuFanModeItem() == itemId ? 1 : 0);
        }
    }
    appendBooleanProperty(iterator, "visible", true);
}

bool MonitorTrayApplication::appendMenuPropertyValue(
    DBusMessageIter *iterator, int itemId, const QString &name) const
{
    DBusMessageIter variant;
    if (name == QStringLiteral("label") && itemId != 0 && !isMenuSeparator(itemId)) {
        dbus_message_iter_open_container(iterator, DBUS_TYPE_VARIANT, "s", &variant);
        appendString(&variant, menuLabel(itemId));
    } else if (name == QStringLiteral("type") && isMenuSeparator(itemId)) {
        dbus_message_iter_open_container(iterator, DBUS_TYPE_VARIANT, "s", &variant);
        appendString(&variant, QStringLiteral("separator"));
    } else if (name == QStringLiteral("toggle-type") && isGpuFanModeItem(itemId)) {
        dbus_message_iter_open_container(iterator, DBUS_TYPE_VARIANT, "s", &variant);
        appendString(&variant, QStringLiteral("radio"));
    } else if (name == QStringLiteral("toggle-state") && isGpuFanModeItem(itemId)) {
        dbus_message_iter_open_container(iterator, DBUS_TYPE_VARIANT, "i", &variant);
        dbus_int32_t value = selectedGpuFanModeItem() == itemId ? 1 : 0;
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_INT32, &value);
    } else if (name == QStringLiteral("enabled") || name == QStringLiteral("visible")) {
        dbus_message_iter_open_container(iterator, DBUS_TYPE_VARIANT, "b", &variant);
        dbus_bool_t value = name == QStringLiteral("visible") || menuItemEnabled(itemId);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
    } else {
        return false;
    }
    dbus_message_iter_close_container(iterator, &variant);
    return true;
}

QVector<int> MonitorTrayApplication::menuChildren(int itemId) const
{
    if (itemId != 0) {
        return {};
    }

    QVector<int> children = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
        kGpuFanAutoItem, kGpuFanLowItem, kGpuFanHighItem, kGpuFanFullItem,
        10,
    };
    const int diskCount = qMax(1, m_stats.disks.volumes.size());
    for (int index = 0; index < diskCount; ++index) {
        children.append(100 + index);
    }
    children.append(11);
    children.append(12);
    return children;
}

QString MonitorTrayApplication::menuLabel(int itemId) const
{
    const QString unavailable = QStringLiteral("--");
    switch (itemId) {
    case 1:
        return QStringLiteral("CPU：%1").arg(m_stats.cpu.name.isEmpty() ? unavailable : m_stats.cpu.name);
    case 2:
        return QStringLiteral("CPU 使用率：%1 %").arg(m_stats.cpuPercent, 0, 'f', 1);
    case 3:
        if (m_stats.cpu.performanceCoreCount <= 0 && m_stats.cpu.efficiencyCoreCount <= 0) {
            return QStringLiteral("CPU 核心 / 频率：--");
        }
        return QStringLiteral("CPU 核心：P %1 / E %2    P 核均频：%3 MHz")
            .arg(m_stats.cpu.performanceCoreCount)
            .arg(m_stats.cpu.efficiencyCoreCount)
            .arg(m_stats.cpu.performanceAverageMHz, 0, 'f', 0);
    case 4:
        return QStringLiteral("CPU 温度：%1    功耗：%2")
            .arg(m_stats.package.temperatureValid
                     ? QStringLiteral("%1 °C").arg(m_stats.package.temperatureC, 0, 'f', 0)
                     : unavailable)
            .arg(m_stats.package.powerValid
                     ? QStringLiteral("%1 W").arg(m_stats.package.powerW, 0, 'f', 1)
                     : unavailable);
    case 6:
        return m_stats.memory.totalKb > 0
            ? QStringLiteral("内存：%1 %    %2 / %3")
                  .arg(m_stats.memory.usedPercent(), 0, 'f', 1)
                  .arg(formatMemory(m_stats.memory.usedKb()))
                  .arg(formatMemory(m_stats.memory.totalKb))
            : QStringLiteral("内存：--");
    case 8:
        return m_stats.gpu.valid
            ? QStringLiteral("GPU 温度：%1 °C").arg(m_stats.gpu.temperatureC, 0, 'f', 0)
            : QStringLiteral("GPU 温度：--");
    case 9:
        if (!m_stats.gpu.fanValid) {
            return QStringLiteral("GPU 风扇：--");
        }
        return QStringLiteral("GPU 风扇：%1    %2 %    %3")
            .arg(m_stats.gpu.fanLevel())
            .arg(m_stats.gpu.fanPercent)
            .arg(m_stats.gpu.rpmValid
                     ? QStringLiteral("%1 RPM").arg(m_stats.gpu.fanRpm)
                     : QStringLiteral("PWM %1/%2").arg(m_stats.gpu.fanPwm).arg(m_stats.gpu.fanMaxPwm));
    case kGpuFanAutoItem:
        return QStringLiteral("GPU 风扇：自动");
    case kGpuFanLowItem:
        return QStringLiteral("GPU 风扇：低速");
    case kGpuFanHighItem:
        return QStringLiteral("GPU 风扇：高速");
    case kGpuFanFullItem:
        return QStringLiteral("GPU 风扇：满速");
    case 12:
        return QStringLiteral("下载速度：%1")
            .arg(m_stats.network.valid
                     ? formatRate(m_stats.network.downloadBytesPerSecond) : unavailable);
    default:
        if (itemId >= 100) {
            const int index = itemId - 100;
            if (index >= 0 && index < m_stats.disks.volumes.size()) {
                const CpuMemDiskSnapshot &disk = m_stats.disks.volumes.at(index);
                return QStringLiteral("磁盘 %1：%2 %    %3 / %4")
                    .arg(disk.name)
                    .arg(disk.usedPercent(), 0, 'f', 1)
                    .arg(formatBytes(disk.usedBytes()))
                    .arg(formatBytes(disk.totalBytes));
            }
            return QStringLiteral("磁盘：--");
        }
        return {};
    }
}

bool MonitorTrayApplication::menuItemEnabled(int itemId) const
{
    if (itemId == 0) {
        return true;
    }
    return isGpuFanModeItem(itemId)
        && m_stats.gpu.fanValid
        && !m_gpuFanSettingPending;
}

int MonitorTrayApplication::selectedGpuFanModeItem() const
{
    return m_requestedGpuFanModeItem != 0
        ? m_requestedGpuFanModeItem : gpuFanModeItem(m_stats.gpu);
}

void MonitorTrayApplication::setGpuFanMode(int itemId)
{
    const QString mode = gpuFanModeArgument(itemId);
    if (mode.isEmpty() || !menuItemEnabled(itemId)) {
        return;
    }

    m_gpuFanSettingPending = true;
    sendMenuItemsUpdated();

    QProcess *process = new QProcess(this);
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart || !m_gpuFanSettingPending) {
                    return;
                }
                m_gpuFanSettingPending = false;
                qWarning().noquote() << "GPU fan helper failed to start:"
                                     << process->errorString();
                sendMenuItemsUpdated();
                process->deleteLater();
            });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process, itemId](int exitCode, QProcess::ExitStatus exitStatus) {
                if (!m_gpuFanSettingPending) {
                    process->deleteLater();
                    return;
                }
                m_gpuFanSettingPending = false;
                if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    m_requestedGpuFanModeItem = itemId;
                    requestSample();
                } else {
                    qWarning().noquote() << "GPU fan helper failed:"
                                         << QString::fromUtf8(process->readAllStandardError()).trimmed();
                }
                sendMenuItemsUpdated();
                process->deleteLater();
            });
    process->start(gpuFanHelperPath(), { mode });
    QTimer::singleShot(3000, process, [process] {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
    });
}

void MonitorTrayApplication::showLocalMenu(int x, int y)
{
    requestSample();

    if (m_localMenu) {
        m_localMenu->close();
    }

    QMenu *menu = new QMenu;
    m_localMenu = menu;
    m_localMenuActions.clear();
    QActionGroup *fanGroup = new QActionGroup(menu);
    fanGroup->setExclusive(true);
    for (int itemId : menuChildren(0)) {
        if (isMenuSeparator(itemId)) {
            menu->addSeparator();
            continue;
        }
        QAction *action = menu->addAction(menuLabel(itemId));
        action->setEnabled(menuItemEnabled(itemId));
        m_localMenuActions.insert(itemId, action);
        if (isGpuFanModeItem(itemId)) {
            action->setCheckable(true);
            action->setChecked(selectedGpuFanModeItem() == itemId);
            fanGroup->addAction(action);
            connect(action, &QAction::triggered, this, [this, itemId] {
                setGpuFanMode(itemId);
            });
        }
    }
    connect(menu, &QMenu::aboutToHide, this, [this, menu] {
        if (m_localMenu == menu) {
            m_localMenu = nullptr;
            m_localMenuActions.clear();
        }
        menu->deleteLater();
    });

    QPoint position(x, y);
    // Hosts such as dde-shell pass (0, 0) when invoking Activate; anchor at
    // the pointer so the menu opens next to the tray icon instead of the
    // top-left corner of the desktop.
    if ((x == 0 && y == 0) || !QGuiApplication::screenAt(position)) {
        position = QCursor::pos();
    }
    menu->popup(position);
}

void MonitorTrayApplication::refreshLocalMenu()
{
    if (!m_localMenu) {
        return;
    }
    for (auto it = m_localMenuActions.cbegin(); it != m_localMenuActions.cend(); ++it) {
        QAction *action = it.value();
        action->setText(menuLabel(it.key()));
        action->setEnabled(menuItemEnabled(it.key()));
        if (isGpuFanModeItem(it.key())) {
            action->setChecked(selectedGpuFanModeItem() == it.key());
        }
    }
}

void MonitorTrayApplication::processDBus()
{
    if (!m_connection) {
        return;
    }
    dbus_connection_read_write(m_connection, 0);
    while (dbus_connection_get_dispatch_status(m_connection) == DBUS_DISPATCH_DATA_REMAINS) {
        dbus_connection_dispatch(m_connection);
    }
}

void MonitorTrayApplication::requestSample()
{
    if (!m_statsWorker || m_samplePending) {
        return;
    }
    m_samplePending = true;
    QMetaObject::invokeMethod(m_statsWorker, "sample", Qt::QueuedConnection);
}

void MonitorTrayApplication::applyStats(const CpuMemStats &stats)
{
    m_samplePending = false;
    const int oldDiskItemCount = m_diskItemCount;
    m_stats = stats;
    if (m_requestedGpuFanModeItem != 0
        && gpuFanModeItem(m_stats.gpu) == m_requestedGpuFanModeItem) {
        m_requestedGpuFanModeItem = 0;
    }
    m_diskItemCount = qMax(1, m_stats.disks.volumes.size());
    refreshLocalMenu();
    sendSignal("NewToolTip");
    if (m_diskItemCount != oldDiskItemCount) {
        sendMenuLayoutUpdated();
    } else {
        sendMenuItemsUpdated();
    }
}

void MonitorTrayApplication::sendMenuItemsUpdated()
{
    if (!m_connection) {
        return;
    }
    DBusMessage *signal = dbus_message_new_signal(kMenuPath, kMenuInterface, "ItemsPropertiesUpdated");
    if (!signal) {
        return;
    }

    DBusMessageIter iterator;
    DBusMessageIter updated;
    DBusMessageIter removed;
    dbus_message_iter_init_append(signal, &iterator);
    dbus_message_iter_open_container(&iterator, DBUS_TYPE_ARRAY, "(ia{sv})", &updated);
    for (int itemId : menuChildren(0)) {
        if (isMenuSeparator(itemId)) {
            continue;
        }
        DBusMessageIter item;
        DBusMessageIter properties;
        dbus_int32_t id = itemId;
        dbus_message_iter_open_container(&updated, DBUS_TYPE_STRUCT, nullptr, &item);
        dbus_message_iter_append_basic(&item, DBUS_TYPE_INT32, &id);
        dbus_message_iter_open_container(&item, DBUS_TYPE_ARRAY, "{sv}", &properties);
        appendStringProperty(&properties, "label", menuLabel(itemId));
        appendBooleanProperty(&properties, "enabled", menuItemEnabled(itemId));
        if (isGpuFanModeItem(itemId)) {
            appendInt32Property(&properties, "toggle-state",
                                selectedGpuFanModeItem() == itemId ? 1 : 0);
        }
        dbus_message_iter_close_container(&item, &properties);
        dbus_message_iter_close_container(&updated, &item);
    }
    dbus_message_iter_close_container(&iterator, &updated);
    dbus_message_iter_open_container(&iterator, DBUS_TYPE_ARRAY, "(ias)", &removed);
    dbus_message_iter_close_container(&iterator, &removed);
    dbus_connection_send(m_connection, signal, nullptr);
    dbus_message_unref(signal);
}

void MonitorTrayApplication::sendMenuLayoutUpdated()
{
    if (!m_connection) {
        return;
    }
    ++m_menuRevision;
    DBusMessage *signal = dbus_message_new_signal(kMenuPath, kMenuInterface, "LayoutUpdated");
    if (!signal) {
        return;
    }
    dbus_int32_t parentId = 0;
    dbus_message_append_args(signal,
                             DBUS_TYPE_UINT32, &m_menuRevision,
                             DBUS_TYPE_INT32, &parentId,
                             DBUS_TYPE_INVALID);
    dbus_connection_send(m_connection, signal, nullptr);
    dbus_message_unref(signal);
}

void MonitorTrayApplication::sendSignal(const char *name, const QString &argument)
{
    if (!m_connection) {
        return;
    }
    DBusMessage *signal = dbus_message_new_signal(kItemPath, kItemInterface, name);
    if (!signal) {
        return;
    }
    if (!argument.isNull()) {
        const QByteArray utf8 = argument.toUtf8();
        const char *value = utf8.constData();
        dbus_message_append_args(signal, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID);
    }
    dbus_connection_send(m_connection, signal, nullptr);
    dbus_message_unref(signal);
}

QString MonitorTrayApplication::toolTipText() const
{
    QStringList lines;
    lines << QStringLiteral("CPU %1%    内存 %2%")
                 .arg(m_stats.cpuPercent, 0, 'f', 1)
                 .arg(m_stats.memory.usedPercent(), 0, 'f', 1);
    if (m_stats.package.valid()) {
        QStringList packageParts;
        if (m_stats.package.temperatureValid) {
            packageParts << QStringLiteral("%1 °C").arg(m_stats.package.temperatureC, 0, 'f', 0);
        }
        if (m_stats.package.powerValid) {
            packageParts << QStringLiteral("%1 W").arg(m_stats.package.powerW, 0, 'f', 1);
        }
        lines << QStringLiteral("CPU %1").arg(packageParts.join(QStringLiteral("    ")));
    }
    if (m_stats.gpu.valid) {
        QString gpu = QStringLiteral("GPU %1 °C").arg(m_stats.gpu.temperatureC, 0, 'f', 0);
        if (m_stats.gpu.fanValid) {
            gpu += QStringLiteral("    风扇 %1% (%2)")
                       .arg(m_stats.gpu.fanPercent)
                       .arg(m_stats.gpu.fanLevel());
        }
        lines << gpu;
    }
    if (m_stats.disks.valid()) {
        lines << QStringLiteral("磁盘 %1%").arg(m_stats.disks.usedPercent(), 0, 'f', 1);
    }
    if (m_stats.network.valid) {
        lines << QStringLiteral("下载 %1").arg(formatRate(m_stats.network.downloadBytesPerSecond));
    }
    return lines.join(QLatin1Char('\n'));
}

void MonitorTrayApplication::stopWorker()
{
    if (!m_statsThread.isRunning()) {
        return;
    }
    m_statsThread.requestInterruption();
    m_statsThread.quit();
    if (!m_statsThread.wait(1000)) {
        m_statsThread.terminate();
        m_statsThread.wait();
    }
    m_statsWorker = nullptr;
}
