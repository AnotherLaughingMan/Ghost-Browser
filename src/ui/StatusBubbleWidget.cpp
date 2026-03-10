#include "ui/StatusBubbleWidget.h"

#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QTimer>

namespace {

constexpr int kBubbleMargin = 12;
constexpr int kBubbleVerticalMargin = 10;
constexpr int kBubbleAvoidPadding = 16;
constexpr int kBubbleMaxLift = 96;
constexpr int kShowFadeDurationMs = 120;
constexpr int kHideDelayMs = 250;
constexpr int kHideFadeDurationMs = 200;
constexpr int kExpandHoverDelayMs = 1600;

}

StatusBubbleWidget::StatusBubbleWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("statusBubble"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    hide();

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(0);

    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("statusBubbleLabel"));
    m_label->setTextInteractionFlags(Qt::NoTextInteraction);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_label);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);

    m_opacityAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    connect(m_opacityAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_opacityEffect->opacity() <= 0.0 && m_fullText.isEmpty())
            hide();
    });

    m_geometryAnimation = new QPropertyAnimation(this, "geometry", this);
    m_geometryAnimation->setDuration(140);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &StatusBubbleWidget::beginFadeOut);

    m_expandTimer = new QTimer(this);
    m_expandTimer->setSingleShot(true);
    connect(m_expandTimer, &QTimer::timeout, this, &StatusBubbleWidget::applyExpandedLayout);
}

void StatusBubbleWidget::setHoveredUrl(const QString &url)
{
    const QString normalized = url.trimmed();
    if (normalized.isEmpty()) {
        clear();
        return;
    }

    m_fullText = normalized;
    m_expanded = false;

    m_hideTimer->stop();
    m_expandTimer->stop();
    updateDisplayedText();
    syncGeometry(isVisible());
    ensureVisible();
    scheduleExpansionIfNeeded();
}

void StatusBubbleWidget::clear()
{
    m_expandTimer->stop();
    m_fullText.clear();
    m_expanded = false;
    m_hideTimer->start(kHideDelayMs);
}

void StatusBubbleWidget::refreshPosition()
{
    if (!isVisible() && m_fullText.isEmpty())
        return;

    syncGeometry(false);
}

void StatusBubbleWidget::updateCursorPosition(const QPoint &contentPos, bool insideContent)
{
    m_cursorPos = contentPos;
    m_cursorInsideContent = insideContent;

    if (isVisible())
        syncGeometry(true);
}

void StatusBubbleWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateDisplayedText();
}

void StatusBubbleWidget::ensureVisible()
{
    m_opacityAnimation->stop();
    show();
    raise();

    m_opacityAnimation->setDuration(kShowFadeDurationMs);
    m_opacityAnimation->setStartValue(m_opacityEffect->opacity());
    m_opacityAnimation->setEndValue(1.0);
    m_opacityAnimation->start();
}

void StatusBubbleWidget::beginFadeOut()
{
    if (isHidden())
        return;

    m_opacityAnimation->stop();
    m_geometryAnimation->stop();
    m_opacityAnimation->setDuration(kHideFadeDurationMs);
    m_opacityAnimation->setStartValue(m_opacityEffect->opacity());
    m_opacityAnimation->setEndValue(0.0);
    m_opacityAnimation->start();
}

void StatusBubbleWidget::scheduleExpansionIfNeeded()
{
    const QFontMetrics metrics(m_label->font());
    const QMargins margins = layout()->contentsMargins();
    const int textWidth = standardWidth() - margins.left() - margins.right();
    if (metrics.horizontalAdvance(m_fullText) > textWidth)
        m_expandTimer->start(kExpandHoverDelayMs);
}

void StatusBubbleWidget::applyExpandedLayout()
{
    if (m_fullText.isEmpty())
        return;

    m_expanded = true;
    updateDisplayedText();
    syncGeometry(true);
}

void StatusBubbleWidget::updateDisplayedText()
{
    if (!m_label)
        return;

    const QMargins margins = layout()->contentsMargins();
    const int textWidth = qMax(64, width() - margins.left() - margins.right());
    m_label->setText(fontMetrics().elidedText(m_fullText, Qt::ElideRight, textWidth));
}

void StatusBubbleWidget::syncGeometry(bool animate)
{
    QWidget *parent = parentWidget();
    if (!parent)
        return;

    const int targetWidth = m_expanded ? expandedWidth() : standardWidth();
    const Placement placement = preferredPlacement(targetWidth);
    const QRect target = targetGeometryForWidth(targetWidth, placement.corner, placement.verticalLift);
    m_corner = placement.corner;
    m_verticalLift = placement.verticalLift;

    if (animate && isVisible()) {
        m_geometryAnimation->stop();
        m_geometryAnimation->setStartValue(geometry());
        m_geometryAnimation->setEndValue(target);
        m_geometryAnimation->start();
    } else {
        setGeometry(target);
    }
}

QRect StatusBubbleWidget::targetGeometryForWidth(int width, AnchorCorner corner, int verticalLift) const
{
    const QRect bounds = parentWidget() ? parentWidget()->contentsRect() : QRect();
    const QSize hint = sizeHint();
    const int height = qMax(hint.height(), 28);
    const int x = (corner == AnchorCorner::BottomLeft)
        ? bounds.left() + kBubbleMargin
        : bounds.right() - width - kBubbleMargin + 1;
    const int baseY = bounds.bottom() - height - kBubbleVerticalMargin + 1;
    const int topLimit = bounds.top() + kBubbleMargin;
    const int y = qMax(topLimit, baseY - verticalLift);
    return QRect(x, y, width, height);
}

StatusBubbleWidget::Placement StatusBubbleWidget::preferredPlacement(int width) const
{
    const int leftLift = preferredVerticalLift(width, AnchorCorner::BottomLeft);
    const QRect leftRect = targetGeometryForWidth(width, AnchorCorner::BottomLeft, leftLift);
    if (!cursorOverlaps(leftRect))
        return { AnchorCorner::BottomLeft, leftLift };

    const int rightLift = preferredVerticalLift(width, AnchorCorner::BottomRight);
    const QRect rightRect = targetGeometryForWidth(width, AnchorCorner::BottomRight, rightLift);
    if (!cursorOverlaps(rightRect))
        return { AnchorCorner::BottomRight, rightLift };

    if (leftLift >= rightLift)
        return { AnchorCorner::BottomLeft, leftLift };

    return { AnchorCorner::BottomRight, rightLift };
}

int StatusBubbleWidget::preferredVerticalLift(int width, AnchorCorner corner) const
{
    if (!m_cursorInsideContent || !parentWidget())
        return 0;

    const QRect baseRect = targetGeometryForWidth(width, corner, 0);
    const QRect paddedRect = baseRect.adjusted(-kBubbleAvoidPadding, -kBubbleAvoidPadding,
                                               kBubbleAvoidPadding, kBubbleAvoidPadding);
    if (!paddedRect.contains(m_cursorPos))
        return 0;

    const QRect bounds = parentWidget()->contentsRect();
    const int maxLift = qMin(kBubbleMaxLift, qMax(0, baseRect.top() - bounds.top() - kBubbleMargin));
    const int requiredLift = qMax(0, paddedRect.bottom() - m_cursorPos.y() + 1);
    return qMin(requiredLift, maxLift);
}

bool StatusBubbleWidget::cursorOverlaps(const QRect &rect) const
{
    if (!m_cursorInsideContent)
        return false;

    return rect.adjusted(-kBubbleAvoidPadding, -kBubbleAvoidPadding,
                         kBubbleAvoidPadding, kBubbleAvoidPadding).contains(m_cursorPos);
}

int StatusBubbleWidget::availableWidth() const
{
    const QWidget *parent = parentWidget();
    if (!parent)
        return 240;

    return qMax(200, parent->contentsRect().width() - (kBubbleMargin * 2));
}

int StatusBubbleWidget::standardWidth() const
{
    const int available = availableWidth();
    return qBound(220, available / 3, qMin(420, available));
}

int StatusBubbleWidget::expandedWidth() const
{
    const int available = availableWidth();
    const int target = qMin(760, static_cast<int>(available * 0.62));
    return qBound(standardWidth(), target, available);
}