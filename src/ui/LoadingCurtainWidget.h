#pragma once

#include <QColor>
#include <QWidget>

class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QTimer;

class LoadingCurtainWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit LoadingCurtainWidget(QWidget *parent = nullptr);

    void applyTheme(const QColor &backgroundColor);
    void setLoading(bool loading);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void syncGeometry();

    QGraphicsOpacityEffect *m_opacityEffect = nullptr;
    QPropertyAnimation *m_opacityAnimation = nullptr;
    QTimer *m_maxVisibleTimer = nullptr;
    QColor m_backgroundColor;
};