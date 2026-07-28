#include "monitorwidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPalette>

CpuMemMonitorWidget::CpuMemMonitorWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFixedSize(16, 16);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void CpuMemMonitorWidget::setStats(const CpuMemStats &stats)
{
    m_stats = stats;
    update();
}

void CpuMemMonitorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bgRect = rect().adjusted(2, 2, -2, -2);
    const QColor fg = foregroundColor();
    QColor bg = fg;
    bg.setAlpha(m_pressed ? 38 : (m_hover ? 30 : 18));

    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRoundedRect(bgRect, 4, 4);

    const QRectF area = contentRect();
    QColor pinColor = fg;
    pinColor.setAlpha((m_stats.package.temperatureValid || m_stats.gpu.valid) ? 150 : 105);

    painter.setPen(QPen(pinColor, 1.2, Qt::SolidLine, Qt::RoundCap));
    const qreal left = area.left();
    const qreal right = area.right();
    const qreal top = area.top();
    const qreal bottom = area.bottom();
    const qreal midX = area.center().x();
    const qreal midY = area.center().y();
    painter.drawLine(QPointF(left, top + 2), QPointF(left - 1.5, top + 2));
    painter.drawLine(QPointF(left, midY), QPointF(left - 1.5, midY));
    painter.drawLine(QPointF(left, bottom - 2), QPointF(left - 1.5, bottom - 2));
    painter.drawLine(QPointF(right, top + 2), QPointF(right + 1.5, top + 2));
    painter.drawLine(QPointF(right, midY), QPointF(right + 1.5, midY));
    painter.drawLine(QPointF(right, bottom - 2), QPointF(right + 1.5, bottom - 2));
    painter.drawLine(QPointF(midX, top), QPointF(midX, top - 1.5));
    painter.drawLine(QPointF(midX, bottom), QPointF(midX, bottom + 1.5));

    const QColor heatColor = temperatureColor();
    QColor borderColor = fg;
    borderColor.setAlpha(180);
    painter.setPen(QPen(borderColor, 1));
    painter.setBrush(heatColor);
    painter.drawRoundedRect(area, 2, 2);

    QColor coreColor = heatColor.lighter((m_stats.package.temperatureValid || m_stats.gpu.valid) ? 132 : 115);
    coreColor.setAlpha((m_stats.package.temperatureValid || m_stats.gpu.valid) ? 220 : 90);
    painter.setPen(Qt::NoPen);
    painter.setBrush(coreColor);
    painter.drawRoundedRect(area.adjusted(3, 3, -3, -3), 1.2, 1.2);
}

void CpuMemMonitorWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void CpuMemMonitorWidget::enterEvent(QEnterEvent *event)
#else
void CpuMemMonitorWidget::enterEvent(QEvent *event)
#endif
{
    m_hover = true;
    update();
    QWidget::enterEvent(event);
}

void CpuMemMonitorWidget::leaveEvent(QEvent *event)
{
    m_hover = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(event);
}

void CpuMemMonitorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }

    QWidget::mousePressEvent(event);
}

void CpuMemMonitorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    const bool activate = m_pressed && event->button() == Qt::LeftButton && rect().contains(event->pos());
    m_pressed = false;
    update();
    if (activate) {
        Q_EMIT activated();
    }

    QWidget::mouseReleaseEvent(event);
}

QColor CpuMemMonitorWidget::foregroundColor() const
{
    return palette().color(QPalette::WindowText);
}

QColor CpuMemMonitorWidget::temperatureColor() const
{
    if (!m_stats.package.temperatureValid) {
        QColor color = foregroundColor();
        color.setAlpha(150);
        return color;
    }

    const double temperature = qBound(10.0, m_stats.package.temperatureC, 90.0);
    if (temperature >= 80.0) {
        return QColor(220, 0, 0);
    }

    const double ratio = (temperature - 10.0) / 70.0;
    const QColor cool(255, 248, 198);
    const QColor warm(255, 96, 24);
    return QColor(
        qRound(cool.red() + (warm.red() - cool.red()) * ratio),
        qRound(cool.green() + (warm.green() - cool.green()) * ratio),
        qRound(cool.blue() + (warm.blue() - cool.blue()) * ratio));
}

QRectF CpuMemMonitorWidget::contentRect() const
{
    return rect().adjusted(4, 4, -4, -4);
}
