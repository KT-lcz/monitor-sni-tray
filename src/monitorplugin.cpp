#include "monitorplugin.h"

#include <QJsonDocument>
#include <QMap>
#include <QPainter>
#include <QVariant>

const QString CpuMemDockPlugin::ItemKey = QStringLiteral("cpu-memory-monitor");

CpuMemDockPlugin::CpuMemDockPlugin(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &CpuMemDockPlugin::updateStats);
}

const QString CpuMemDockPlugin::pluginName() const
{
    return ItemKey;
}

const QString CpuMemDockPlugin::pluginDisplayName() const
{
    return QStringLiteral("CPU、内存与封装");
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

    QMap<QString, QVariant> menu;
    menu.insert(QStringLiteral("items"), QVariantList { refreshItem });
    menu.insert(QStringLiteral("checkableMenu"), false);
    menu.insert(QStringLiteral("singleCheck"), false);
    return QString::fromUtf8(QJsonDocument::fromVariant(menu).toJson(QJsonDocument::Compact));
}

void CpuMemDockPlugin::invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked)
{
    Q_UNUSED(checked)

    if (itemKey == ItemKey && menuId == QStringLiteral("refresh")) {
        updateStats();
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

void CpuMemDockPlugin::updateStats()
{
    m_stats = m_statsReader.sample();

    if (m_itemWidget) {
        m_itemWidget->setStats(m_stats);
    }

    if (m_quickPanelWidget) {
        m_quickPanelWidget->setStats(m_stats);
    }

    if (m_tipsWidget) {
        m_tipsWidget->setStats(m_stats);
    }

    if (m_proxyInter) {
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
    m_quickPanelWidget = new CpuMemQuickPanelWidget;
    m_tipsWidget.reset(new CpuMemTipsWidget);
    updateStats();
    m_timer.start();
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
