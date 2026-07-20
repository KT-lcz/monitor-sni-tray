#include "monitorplugin.h"

#include <QJsonDocument>
#include <QMap>
#include <QPainter>
#include <QProcess>
#include <QVariant>

const QString CpuMemDockPlugin::ItemKey = QStringLiteral("cpu-memory-monitor");

CpuMemStatsWorker::CpuMemStatsWorker(QObject *parent)
    : QObject(parent)
{
}

void CpuMemStatsWorker::sample()
{
    if (QThread::currentThread()->isInterruptionRequested()) {
        return;
    }

    if (!m_statsReader) {
        m_statsReader.reset(new CpuMemStatsReader);
    }

    Q_EMIT statsReady(m_statsReader->sample());
}

void CpuMemStatsWorker::setGpuFanMode(const QString &mode)
{
    QProcess::execute(QStringLiteral("/usr/lib/monitor-dock-plugin/gpu-fan-helper"), { mode });
    sample();
}

CpuMemDockPlugin::CpuMemDockPlugin(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<CpuMemStats>("CpuMemStats");

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &CpuMemDockPlugin::updateStats);
}

CpuMemDockPlugin::~CpuMemDockPlugin()
{
    m_timer.stop();

    if (!m_statsThread.isRunning()) {
        return;
    }

    m_statsThread.requestInterruption();
    m_statsThread.quit();
    if (!m_statsThread.wait(1000)) {
        m_statsThread.terminate();
        m_statsThread.wait();
    }
}

const QString CpuMemDockPlugin::pluginName() const
{
    return ItemKey;
}

const QString CpuMemDockPlugin::pluginDisplayName() const
{
    return QStringLiteral("硬件监控");
}

void CpuMemDockPlugin::init(PluginProxyInterface *proxyInter)
{
    m_proxyInter = proxyInter;

    if (!pluginIsDisable()) {
        loadPlugin();
        m_proxyInter->itemAdded(this, ItemKey);
    }
}

QWidget *CpuMemDockPlugin::itemWidget(const QString &itemKey)
{
    if (itemKey == Dock::QUICK_ITEM_KEY) {
        return m_quickPanelWidget;
    }

    if (itemKey == ItemKey) {
        return m_itemWidget;
    }

    return nullptr;
}

QWidget *CpuMemDockPlugin::itemTipsWidget(const QString &itemKey)
{
    if ((itemKey != ItemKey && itemKey != Dock::QUICK_ITEM_KEY) || !m_tipsWidget) {
        return nullptr;
    }

    m_tipsWidget->setStats(m_stats);
    return m_tipsWidget.data();
}

const QString CpuMemDockPlugin::itemContextMenu(const QString &itemKey)
{
    if (itemKey != ItemKey) {
        return {};
    }

    QMap<QString, QVariant> refreshItem;
    refreshItem.insert(QStringLiteral("itemId"), QStringLiteral("refresh"));
    refreshItem.insert(QStringLiteral("itemText"), QStringLiteral("刷新"));
    refreshItem.insert(QStringLiteral("isActive"), true);

    QMap<QString, QVariant> autoFanItem;
    autoFanItem.insert(QStringLiteral("itemId"), QStringLiteral("gpu-fan-auto"));
    autoFanItem.insert(QStringLiteral("itemText"), QStringLiteral("GPU 风扇自动"));
    autoFanItem.insert(QStringLiteral("isActive"), true);

    QMap<QString, QVariant> mediumFanItem;
    mediumFanItem.insert(QStringLiteral("itemId"), QStringLiteral("gpu-fan-medium"));
    mediumFanItem.insert(QStringLiteral("itemText"), QStringLiteral("GPU 风扇中等"));
    mediumFanItem.insert(QStringLiteral("isActive"), true);

    QMap<QString, QVariant> fullFanItem;
    fullFanItem.insert(QStringLiteral("itemId"), QStringLiteral("gpu-fan-full"));
    fullFanItem.insert(QStringLiteral("itemText"), QStringLiteral("GPU 风扇满速"));
    fullFanItem.insert(QStringLiteral("isActive"), true);

    QMap<QString, QVariant> menu;
    menu.insert(QStringLiteral("items"), QVariantList { refreshItem, autoFanItem, mediumFanItem, fullFanItem });
    menu.insert(QStringLiteral("checkableMenu"), false);
    menu.insert(QStringLiteral("singleCheck"), false);
    return QString::fromUtf8(QJsonDocument::fromVariant(menu).toJson(QJsonDocument::Compact));
}

void CpuMemDockPlugin::invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked)
{
    Q_UNUSED(checked)

    if (itemKey == ItemKey && menuId == QStringLiteral("refresh")) {
        updateStats();
    } else if (itemKey == ItemKey && menuId == QStringLiteral("gpu-fan-auto")) {
        setGpuFanMode(QStringLiteral("auto"));
    } else if (itemKey == ItemKey && menuId == QStringLiteral("gpu-fan-medium")) {
        setGpuFanMode(QStringLiteral("medium"));
    } else if (itemKey == ItemKey && menuId == QStringLiteral("gpu-fan-full")) {
        setGpuFanMode(QStringLiteral("full"));
    }
}

int CpuMemDockPlugin::itemSortKey(const QString &itemKey)
{
    const QString key = QStringLiteral("pos_%1_%2").arg(itemKey).arg(displayMode());
    return m_proxyInter->getValue(this, key, -1).toInt();
}

void CpuMemDockPlugin::setSortKey(const QString &itemKey, const int order)
{
    const QString key = QStringLiteral("pos_%1_%2").arg(itemKey).arg(displayMode());
    m_proxyInter->saveValue(this, key, order);
}

bool CpuMemDockPlugin::pluginIsAllowDisable()
{
    return true;
}

bool CpuMemDockPlugin::pluginIsDisable()
{
    if (!m_proxyInter) {
        return false;
    }

    return m_proxyInter->getValue(this, QStringLiteral("disabled"), false).toBool();
}

void CpuMemDockPlugin::pluginStateSwitched()
{
    const bool disabled = !pluginIsDisable();
    m_proxyInter->saveValue(this, QStringLiteral("disabled"), disabled);

    if (disabled) {
        m_timer.stop();
        m_proxyInter->itemRemoved(this, ItemKey);
        return;
    }

    loadPlugin();
    m_proxyInter->itemAdded(this, ItemKey);
}

void CpuMemDockPlugin::displayModeChanged(const Dock::DisplayMode displayMode)
{
    Q_UNUSED(displayMode)

    if (m_itemWidget) {
        m_itemWidget->update();
    }

    if (m_quickPanelWidget) {
        m_quickPanelWidget->update();
    }
}

Dock::PluginFlags CpuMemDockPlugin::flags() const
{
    return Dock::Type_Quick | Dock::Quick_Panel_Single | Dock::Attribute_Normal;
}

QIcon CpuMemDockPlugin::icon(Dock::IconType iconType, Dock::ThemeType themeType) const
{
    Q_UNUSED(iconType)
    return buildIcon(themeType);
}

void CpuMemDockPlugin::launchCpuX()
{
    QProcess::startDetached(QStringLiteral("cpu-x"));
}

void CpuMemDockPlugin::updateStats()
{
    ensureStatsWorker();
    if (!m_statsWorker || m_samplePending) {
        return;
    }

    m_samplePending = true;
    QMetaObject::invokeMethod(m_statsWorker, "sample", Qt::QueuedConnection);
}

void CpuMemDockPlugin::applyStats(const CpuMemStats &stats)
{
    m_samplePending = false;
    m_stats = stats;

    if (m_itemWidget) {
        m_itemWidget->setStats(m_stats);
    }

    if (m_quickPanelWidget) {
        m_quickPanelWidget->setStats(m_stats);
    }

    if (m_tipsWidget) {
        m_tipsWidget->setStats(m_stats);
    }

    if (m_proxyInter && !pluginIsDisable()) {
        m_proxyInter->itemUpdate(this, ItemKey);
    }
}

void CpuMemDockPlugin::loadPlugin()
{
    if (m_loaded) {
        if (!m_timer.isActive()) {
            m_timer.start();
        }
        return;
    }

    m_loaded = true;
    m_itemWidget = new CpuMemMonitorWidget;
    connect(m_itemWidget, &CpuMemMonitorWidget::activated, this, &CpuMemDockPlugin::launchCpuX);
    m_quickPanelWidget = new CpuMemQuickPanelWidget;
    m_tipsWidget.reset(new CpuMemTipsWidget);
    ensureStatsWorker();
    updateStats();
    m_timer.start();
}

void CpuMemDockPlugin::ensureStatsWorker()
{
    if (m_statsWorker) {
        return;
    }

    m_statsWorker = new CpuMemStatsWorker;
    m_statsWorker->moveToThread(&m_statsThread);
    connect(&m_statsThread, &QThread::finished, m_statsWorker, &QObject::deleteLater);
    connect(m_statsWorker, &CpuMemStatsWorker::statsReady, this, &CpuMemDockPlugin::applyStats);
    m_statsThread.start();
}

void CpuMemDockPlugin::setGpuFanMode(const QString &mode)
{
    ensureStatsWorker();
    if (!m_statsWorker) {
        return;
    }

    m_samplePending = true;
    QMetaObject::invokeMethod(m_statsWorker, "setGpuFanMode", Qt::QueuedConnection, Q_ARG(QString, mode));
}

QIcon CpuMemDockPlugin::buildIcon(Dock::ThemeType themeType) const
{
    const QColor fg = themeType == Dock::ThemeType_Light ? QColor(40, 40, 40) : QColor(245, 245, 245);
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fg);
    painter.drawRoundedRect(QRectF(4, 5, 16, 3), 1.5, 1.5);
    painter.drawRoundedRect(QRectF(4, 11, 11, 3), 1.5, 1.5);
    painter.drawRoundedRect(QRectF(4, 17, 14, 3), 1.5, 1.5);
    painter.end();

    return QIcon(pixmap);
}
