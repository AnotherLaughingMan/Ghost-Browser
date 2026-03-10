#include "ui/LoadingCurtainWidget.h"

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>

namespace {

constexpr int kFadeInDurationMs = 80;
constexpr int kFadeOutDurationMs = 110;
constexpr int kMaxVisibleDurationMs = 4000;

}

LoadingCurtainWidget::LoadingCurtainWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("loadingCurtain"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    hide();

    if (parent)
        parent->installEventFilter(this);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);

    m_opacityAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    connect(m_opacityAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_opacityEffect->opacity() <= 0.0)
            hide();
    });

    m_maxVisibleTimer = new QTimer(this);
    m_maxVisibleTimer->setSingleShot(true);
    connect(m_maxVisibleTimer, &QTimer::timeout, this, [this]() {
        setLoading(false);
    });
}

void LoadingCurtainWidget::applyTheme(const QColor &backgroundColor)
{
    m_backgroundColor = backgroundColor;
    setStyleSheet(QStringLiteral("background: %1;").arg(m_backgroundColor.name(QColor::HexRgb)));
}

void LoadingCurtainWidget::setLoading(bool loading)
{
    syncGeometry();
    raise();
    m_opacityAnimation->stop();

    if (loading) {
        m_maxVisibleTimer->start(kMaxVisibleDurationMs);
        show();
        m_opacityAnimation->setDuration(kFadeInDurationMs);
        m_opacityAnimation->setStartValue(m_opacityEffect->opacity());
        m_opacityAnimation->setEndValue(1.0);
        m_opacityAnimation->start();
        return;
    }

    m_maxVisibleTimer->stop();

    if (isHidden())
        return;

    m_opacityAnimation->setDuration(kFadeOutDurationMs);
    m_opacityAnimation->setStartValue(m_opacityEffect->opacity());
    m_opacityAnimation->setEndValue(0.0);
    m_opacityAnimation->start();
}

bool LoadingCurtainWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget()) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Move:
        case QEvent::Show:
            syncGeometry();
            break;
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void LoadingCurtainWidget::syncGeometry()
{
    if (QWidget *parent = parentWidget())
        setGeometry(parent->rect());
}