#include "quickpanelwidget.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QStringList>

CpuMemQuickPanelWidget::CpuMemQuickPanelWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(140, 146);
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
    labelFont.setPixelSize(9);
    labelFont.setBold(true);
    QFont valueFont = font();
    valueFont.setPixelSize(10);

    const QRect area = rect().adjusted(8, 6, -8, -6);
    const int rowHeight = area.height() / 7;

    const QStringList labels = {
        QStringLiteral("NAME"),
        QStringLiteral("CPU"),
        QStringLiteral("P/E"),
        QStringLiteral("P-F"),
        QStringLiteral("PKG"),
        QStringLiteral("MEM"),
        QStringLiteral("DL"),
    };
    const QStringList values = {
        m_stats.cpu.name.isEmpty() ? QStringLiteral("--") : m_stats.cpu.name,
        percentText(m_stats.cpuPercent),
        QStringLiteral("%1/%2").arg(m_stats.cpu.performanceCoreCount).arg(m_stats.cpu.efficiencyCoreCount),
        frequencyText(m_stats.cpu.performanceAverageMHz),
        packageText(),
        percentText(m_stats.memory.usedPercent()),
        m_stats.network.valid ? speedText(m_stats.network.downloadBytesPerSecond) : QStringLiteral("--"),
    };

    painter.setFont(labelFont);
    for (int i = 0; i < labels.size(); ++i) {
        painter.drawText(QRect(area.left(), area.top() + rowHeight * i, 34, rowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         labels.at(i));
    }

    painter.setFont(valueFont);
    for (int i = 0; i < values.size(); ++i) {
        painter.drawText(QRect(area.left() + 36, area.top() + rowHeight * i, area.width() - 36, rowHeight),
                         Qt::AlignRight | Qt::AlignVCenter,
                         elidedText(values.at(i), valueFont, area.width() - 36));
    }
}

QString CpuMemQuickPanelWidget::elidedText(const QString &text, const QFont &font, int width) const
{
    return QFontMetrics(font).elidedText(text, Qt::ElideRight, width);
}

QString CpuMemQuickPanelWidget::frequencyText(double mhz) const
{
    if (mhz <= 0.0) {
        return QStringLiteral("--");
    }

    if (mhz >= 1000.0) {
        return QStringLiteral("%1G").arg(mhz / 1000.0, 0, 'f', 2);
    }

    return QStringLiteral("%1M").arg(mhz, 0, 'f', 0);
}

QString CpuMemQuickPanelWidget::packageText() const
{
    if (!m_stats.package.valid) {
        return QStringLiteral("--");
    }

    return QStringLiteral("%1C %2W")
        .arg(m_stats.package.temperatureC, 0, 'f', 0)
        .arg(m_stats.package.powerW, 0, 'f', 0);
}

QString CpuMemQuickPanelWidget::percentText(double value) const
{
    return QStringLiteral("%1%").arg(value, 0, 'f', value >= 99.95 ? 0 : 1);
}

QString CpuMemQuickPanelWidget::speedText(double bytesPerSecond) const
{
    const double kib = bytesPerSecond / 1024.0;
    if (kib < 1024.0) {
        return QStringLiteral("%1K/s").arg(kib, 0, 'f', kib >= 100.0 ? 0 : 1);
    }

    const double mib = kib / 1024.0;
    if (mib < 1024.0) {
        return QStringLiteral("%1M/s").arg(mib, 0, 'f', mib >= 100.0 ? 0 : 1);
    }

    return QStringLiteral("%1G/s").arg(mib / 1024.0, 0, 'f', 2);
}
