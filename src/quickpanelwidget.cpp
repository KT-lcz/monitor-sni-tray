#include "quickpanelwidget.h"

#include <QPainter>
#include <QPalette>

CpuMemQuickPanelWidget::CpuMemQuickPanelWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(70, 60);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void CpuMemQuickPanelWidget::setStats(const CpuMemStats &stats)
{
    m_stats = stats;
    update();
}

void CpuMemQuickPanelWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.setPen(palette().color(QPalette::WindowText));

    QFont labelFont = font();
    labelFont.setPixelSize(10);
    labelFont.setBold(true);
    QFont valueFont = font();
    valueFont.setPixelSize(11);

    const QRect area = rect().adjusted(8, 6, -8, -6);
    const int rowHeight = area.height() / 2;

    painter.setFont(labelFont);
    painter.drawText(QRect(area.left(), area.top(), 28, rowHeight), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("CPU"));
    painter.drawText(QRect(area.left(), area.top() + rowHeight, 28, rowHeight), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("MEM"));

    painter.setFont(valueFont);
    painter.drawText(QRect(area.left() + 30, area.top(), area.width() - 30, rowHeight),
                     Qt::AlignRight | Qt::AlignVCenter,
                     percentText(m_stats.cpuPercent));
    painter.drawText(QRect(area.left() + 30, area.top() + rowHeight, area.width() - 30, rowHeight),
                     Qt::AlignRight | Qt::AlignVCenter,
                     percentText(m_stats.memory.usedPercent()));
}

QString CpuMemQuickPanelWidget::percentText(double value) const
{
    return QStringLiteral("%1%").arg(value, 0, 'f', value >= 99.95 ? 0 : 1);
}
