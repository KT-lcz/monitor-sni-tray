#ifndef TIPSWIDGET_H
#define TIPSWIDGET_H

#include "systemstats.h"

#include <QFrame>

class CpuMemTipsWidget : public QFrame
{
    Q_OBJECT

public:
    explicit CpuMemTipsWidget(QWidget *parent = nullptr);

    void setStats(const CpuMemStats &stats);

Q_SIGNALS:
    void visibleChanged(bool visible);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString formatMemory(quint64 kb) const;
    void updateSize();

    CpuMemStats m_stats;
};

#endif // TIPSWIDGET_H
