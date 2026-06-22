#include "monitorwidget.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    const QRectF bgRect = rect().adjusted(2, 2, -2, -2);
    const QColor fg = foregroundColor();
    QColor bg = fg;
    bg.setAlpha(m_pressed ? 38 : (m_hover ? 30 : 18));

    QPainterPath path;
    path.addRoundedRect(bgRect, 6, 6);
    painter.fillPath(path, bg);

    const QRectF area = contentRect();
    if (width() < 64 || height() < 28) {
        const qreal cpuWidth = area.width() * qBound(0.0, m_stats.cpuPercent / 100.0, 1.0);
        const qreal memWidth = area.width() * qBound(0.0, m_stats.memory.usedPercent() / 100.0, 1.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fg);
        painter.drawRoundedRect(QRectF(area.left(), area.top() + 1, cpuWidth, 4), 2, 2);
        painter.drawRoundedRect(QRectF(area.left(), area.bottom() - 5, memWidth, 4), 2, 2);
        return;
    }

    const qreal rowHeight = area.height() / 2.0;
    QFont labelFont = font();
    labelFont.setPixelSize(qMax(8, qMin(10, height() / 4)));
    labelFont.setBold(true);

    QFont valueFont = font();
    valueFont.setPixelSize(qMax(8, qMin(11, height() / 3)));

    const QString cpuText = compactPercent(m_stats.cpuPercent);
    const QString memText = compactPercent(m_stats.memory.usedPercent());

    painter.setPen(fg);
    painter.setFont(labelFont);
    painter.drawText(QRectF(area.left(), area.top(), 28, rowHeight), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("CPU"));
    painter.drawText(QRectF(area.left(), area.top() + rowHeight, 28, rowHeight), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("MEM"));

    painter.setFont(valueFont);
    painter.drawText(QRectF(area.left() + 30, area.top(), area.width() - 30, rowHeight), Qt::AlignRight | Qt::AlignVCenter, cpuText);
    painter.drawText(QRectF(area.left() + 30, area.top() + rowHeight, area.width() - 30, rowHeight), Qt::AlignRight | Qt::AlignVCenter, memText);
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

QRectF CpuMemMonitorWidget::contentRect() const
{
    if (width() < 64 || height() < 28) {
        return rect().adjusted(3, 3, -3, -3);
    }

    return rect().adjusted(8, 4, -8, -4);
}

QString CpuMemMonitorWidget::compactPercent(double value) const
{
    return QStringLiteral("%1%").arg(value, 0, 'f', value >= 99.95 ? 0 : 1);
}
