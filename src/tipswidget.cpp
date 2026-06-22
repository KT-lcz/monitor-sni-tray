#include "tipswidget.h"

#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QStringList>

CpuMemTipsWidget::CpuMemTipsWidget(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);
    updateSize();
}

void CpuMemTipsWidget::setStats(const CpuMemStats &stats)
{
    m_stats = stats;
    updateSize();
    update();
}

bool CpuMemTipsWidget::event(QEvent *event)
{
    if (event->type() == QEvent::Show) {
        Q_EMIT visibleChanged(true);
    } else if (event->type() == QEvent::Hide) {
        Q_EMIT visibleChanged(false);
    } else if (event->type() == QEvent::FontChange) {
        updateSize();
    }

    return QFrame::event(event);
}

void CpuMemTipsWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(palette().color(QPalette::BrightText));

    const QString memory = QStringLiteral("%1 / %2")
                               .arg(formatMemory(m_stats.memory.usedKb()))
                               .arg(formatMemory(m_stats.memory.totalKb));
    QStringList labels = {
        QStringLiteral("NAME"),
        QStringLiteral("CPU"),
        QStringLiteral("P/E"),
        QStringLiteral("P-FREQ"),
        QStringLiteral("PKG"),
        QStringLiteral("MEM"),
    };
    QStringList values = {
        m_stats.cpu.name.isEmpty() ? QStringLiteral("--") : m_stats.cpu.name,
        QStringLiteral("%1%").arg(m_stats.cpuPercent, 0, 'f', 1),
        QStringLiteral("%1 / %2").arg(m_stats.cpu.performanceCoreCount).arg(m_stats.cpu.efficiencyCoreCount),
        formatFrequency(m_stats.cpu.performanceAverageMHz),
        m_stats.package.valid ? formatPackageStats() : QStringLiteral("--"),
        QStringLiteral("%1%  %2").arg(m_stats.memory.usedPercent(), 0, 'f', 1).arg(memory),
    };

    if (m_stats.disks.valid()) {
        for (const CpuMemDiskSnapshot &disk : m_stats.disks.volumes) {
            labels.append(labels.contains(QStringLiteral("DISK")) ? QString() : QStringLiteral("DISK"));
            values.append(formatDisk(disk));
        }
    } else {
        labels.append(QStringLiteral("DISK"));
        values.append(QStringLiteral("--"));
    }

    labels.append(QStringLiteral("DOWN"));
    values.append(m_stats.network.valid ? formatSpeed(m_stats.network.downloadBytesPerSecond) : QStringLiteral("--"));

    int labelWidth = 0;
    for (const QString &label : labels) {
        labelWidth = qMax(labelWidth, fontMetrics().horizontalAdvance(label));
    }
    labelWidth += 16;
    const int rowHeight = fontMetrics().height() + 8;
    const int top = 10;
    const int left = 12;

    for (int i = 0; i < labels.size(); ++i) {
        const QRect labelRect(left, top + i * rowHeight, labelWidth, rowHeight);
        const QRect valueRect(left + labelWidth, top + i * rowHeight, width() - labelWidth - left * 2, rowHeight);
        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, labels.at(i));
        painter.drawText(valueRect, Qt::AlignLeft | Qt::AlignVCenter, elidedText(values.value(i), valueRect.width()));
    }
}

QString CpuMemTipsWidget::formatFrequency(double mhz) const
{
    if (mhz <= 0.0) {
        return QStringLiteral("--");
    }

    if (mhz >= 1000.0) {
        return QStringLiteral("%1 GHz").arg(mhz / 1000.0, 0, 'f', 2);
    }

    return QStringLiteral("%1 MHz").arg(mhz, 0, 'f', 0);
}

QString CpuMemTipsWidget::formatMemory(quint64 kb) const
{
    const double mib = kb / 1024.0;
    if (mib < 1024.0) {
        return QStringLiteral("%1 MiB").arg(mib, 0, 'f', 0);
    }

    return QStringLiteral("%1 GiB").arg(mib / 1024.0, 0, 'f', 1);
}

QString CpuMemTipsWidget::formatBytes(quint64 bytes) const
{
    const double mib = bytes / 1024.0 / 1024.0;
    if (mib < 1024.0) {
        return QStringLiteral("%1 MiB").arg(mib, 0, 'f', 0);
    }

    const double gib = mib / 1024.0;
    if (gib < 1024.0) {
        return QStringLiteral("%1 GiB").arg(gib, 0, 'f', 1);
    }

    return QStringLiteral("%1 TiB").arg(gib / 1024.0, 0, 'f', 1);
}

QString CpuMemTipsWidget::formatDisk(const CpuMemDiskSnapshot &disk) const
{
    return QStringLiteral("%1  %2%  %3 / %4")
        .arg(disk.name.isEmpty() ? QStringLiteral("--") : disk.name)
        .arg(disk.usedPercent(), 0, 'f', 1)
        .arg(formatBytes(disk.usedBytes()))
        .arg(formatBytes(disk.totalBytes));
}

QString CpuMemTipsWidget::formatSpeed(double bytesPerSecond) const
{
    const double kib = bytesPerSecond / 1024.0;
    if (kib < 1024.0) {
        return QStringLiteral("%1 KiB/s").arg(kib, 0, 'f', kib >= 100.0 ? 0 : 1);
    }

    const double mib = kib / 1024.0;
    if (mib < 1024.0) {
        return QStringLiteral("%1 MiB/s").arg(mib, 0, 'f', mib >= 100.0 ? 0 : 1);
    }

    return QStringLiteral("%1 GiB/s").arg(mib / 1024.0, 0, 'f', 2);
}

QString CpuMemTipsWidget::formatPackageStats() const
{
    return QStringLiteral("%1 C  %2 W")
        .arg(m_stats.package.temperatureC, 0, 'f', 0)
        .arg(m_stats.package.powerW, 0, 'f', 1);
}

QString CpuMemTipsWidget::elidedText(const QString &text, int width) const
{
    if (width <= 0) {
        return {};
    }

    return fontMetrics().elidedText(text, Qt::ElideMiddle, width);
}

void CpuMemTipsWidget::updateSize()
{
    const QString memory = QStringLiteral("100.0%  %1 / %2")
                               .arg(formatMemory(m_stats.memory.usedKb()))
                               .arg(formatMemory(m_stats.memory.totalKb));
    const QString cpuName = m_stats.cpu.name.isEmpty() ? QString() : m_stats.cpu.name;
    int diskWidth = 0;
    for (const CpuMemDiskSnapshot &disk : m_stats.disks.volumes) {
        diskWidth = qMax(diskWidth, fontMetrics().horizontalAdvance(formatDisk(disk)) + 64);
    }

    const int memoryWidth = fontMetrics().horizontalAdvance(memory) + 64;
    const int packageWidth = fontMetrics().horizontalAdvance(formatPackageStats()) + 64;
    const int cpuNameWidth = fontMetrics().horizontalAdvance(cpuName) + 96;
    const int width = qMin(520, qMax(260, qMax(qMax(qMax(memoryWidth, packageWidth), cpuNameWidth), diskWidth)));
    const int diskRowCount = qMax(1, m_stats.disks.volumes.size());
    const int rowCount = 7 + diskRowCount;
    const int height = (fontMetrics().height() + 8) * rowCount + 20;
    setFixedSize(width, height);
}
