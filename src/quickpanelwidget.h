#ifndef QUICKPANELWIDGET_H
#define QUICKPANELWIDGET_H

#include "systemstats.h"

#include <QWidget>

class CpuMemQuickPanelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CpuMemQuickPanelWidget(QWidget *parent = nullptr);

    void setStats(const CpuMemStats &stats);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString percentText(double value) const;

    CpuMemStats m_stats;
};

#endif // QUICKPANELWIDGET_H
