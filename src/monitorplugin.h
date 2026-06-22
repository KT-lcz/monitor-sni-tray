#ifndef MONITORPLUGIN_H
#define MONITORPLUGIN_H

#include "monitorwidget.h"
#include "quickpanelwidget.h"
#include "systemstats.h"
#include "tipswidget.h"

#include <dde-dock/constants.h>
#include <dde-dock/pluginsiteminterface_v2.h>

#include <QIcon>
#include <QObject>
#include <QScopedPointer>
#include <QTimer>

class CpuMemDockPlugin : public QObject, public PluginsItemInterfaceV2
{
    Q_OBJECT
    Q_INTERFACES(PluginsItemInterfaceV2)
    Q_PLUGIN_METADATA(IID ModuleInterface_iid_V2 FILE "../monitor-dock-plugin.json")

public:
    explicit CpuMemDockPlugin(QObject *parent = nullptr);

    const QString pluginName() const override;
    const QString pluginDisplayName() const override;
    void init(PluginProxyInterface *proxyInter) override;
    QWidget *itemWidget(const QString &itemKey) override;
    QWidget *itemTipsWidget(const QString &itemKey) override;
    const QString itemContextMenu(const QString &itemKey) override;
    void invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked) override;
    int itemSortKey(const QString &itemKey) override;
    void setSortKey(const QString &itemKey, const int order) override;
    bool pluginIsAllowDisable() override;
    bool pluginIsDisable() override;
    void pluginStateSwitched() override;
    void displayModeChanged(const Dock::DisplayMode displayMode) override;
    Dock::PluginFlags flags() const override;
    QIcon icon(Dock::IconType iconType, Dock::ThemeType themeType) const override;

private Q_SLOTS:
    void launchCpuX();
    void updateStats();

private:
    void loadPlugin();
    QIcon buildIcon(Dock::ThemeType themeType) const;

    static const QString ItemKey;

    bool m_loaded = false;
    CpuMemMonitorWidget *m_itemWidget = nullptr;
    CpuMemQuickPanelWidget *m_quickPanelWidget = nullptr;
    QScopedPointer<CpuMemTipsWidget> m_tipsWidget;
    QTimer m_timer;
    CpuMemStatsReader m_statsReader;
    CpuMemStats m_stats;
};

#endif // MONITORPLUGIN_H
