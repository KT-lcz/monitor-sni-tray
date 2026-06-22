#ifndef MONITORWIDGET_H
#define MONITORWIDGET_H

#include "systemstats.h"

#include <QColor>
#include <QWidget>

class CpuMemMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CpuMemMonitorWidget(QWidget *parent = nullptr);

    void setStats(const CpuMemStats &stats);

Q_SIGNALS:
    void activated();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QColor foregroundColor() const;
    QColor temperatureColor() const;
    QRectF contentRect() const;

    CpuMemStats m_stats;
    bool m_hover = false;
    bool m_pressed = false;
};

#endif // MONITORWIDGET_H
