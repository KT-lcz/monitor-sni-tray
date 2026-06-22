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
    QString elidedText(const QString &text, const QFont &font, int width) const;
    QString frequencyText(double mhz) const;
    QString packageText() const;
    QString percentText(double value) const;
    QString speedText(double bytesPerSecond) const;
    QString diskText() const;

    CpuMemStats m_stats;
};

#endif // QUICKPANELWIDGET_H
