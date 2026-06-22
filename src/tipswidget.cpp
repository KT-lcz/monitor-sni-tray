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
    const QStringList labels = {
        QStringLiteral("CPU"),
        QStringLiteral("MEM"),
    };
    const QStringList values = {
        QStringLiteral("%1%").arg(m_stats.cpuPercent, 0, 'f', 1),
        QStringLiteral("%1%  %2").arg(m_stats.memory.usedPercent(), 0, 'f', 1).arg(memory),
    };

    const int labelWidth = fontMetrics().horizontalAdvance(QStringLiteral("MEM")) + 16;
    const int rowHeight = fontMetrics().height() + 8;
    const int top = 10;
    const int left = 12;

    for (int i = 0; i < labels.size(); ++i) {
        const QRect labelRect(left, top + i * rowHeight, labelWidth, rowHeight);
        const QRect valueRect(left + labelWidth, top + i * rowHeight, width() - labelWidth - left * 2, rowHeight);
        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, labels.at(i));
        painter.drawText(valueRect, Qt::AlignLeft | Qt::AlignVCenter, values.at(i));
    }
}

QString CpuMemTipsWidget::formatMemory(quint64 kb) const
{
    const double mib = kb / 1024.0;
    if (mib < 1024.0) {
        return QStringLiteral("%1 MiB").arg(mib, 0, 'f', 0);
    }

    return QStringLiteral("%1 GiB").arg(mib / 1024.0, 0, 'f', 1);
}

void CpuMemTipsWidget::updateSize()
{
    const QString memory = QStringLiteral("100.0%  %1 / %2")
                               .arg(formatMemory(m_stats.memory.usedKb()))
                               .arg(formatMemory(m_stats.memory.totalKb));
    const int width = qMax(190, fontMetrics().horizontalAdvance(memory) + 64);
    const int height = (fontMetrics().height() + 8) * 2 + 20;
    setFixedSize(width, height);
}
