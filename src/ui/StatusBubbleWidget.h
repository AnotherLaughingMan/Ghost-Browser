#pragma once

#include <QPoint>
#include <QWidget>

class QLabel;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QTimer;

class StatusBubbleWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit StatusBubbleWidget(QWidget *parent = nullptr);

    void setHoveredUrl(const QString &url);
    void clear();
    void refreshPosition();
    void updateCursorPosition(const QPoint &contentPos, bool insideContent);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class AnchorCorner {
        BottomLeft,
        BottomRight,
    };

    struct Placement {
        AnchorCorner corner;
        int verticalLift;
    };

    void ensureVisible();
    void beginFadeOut();
    void scheduleExpansionIfNeeded();
    void applyExpandedLayout();
    void updateDisplayedText();
    void syncGeometry(bool animate);
    QRect targetGeometryForWidth(int width, AnchorCorner corner, int verticalLift) const;
    Placement preferredPlacement(int width) const;
    int preferredVerticalLift(int width, AnchorCorner corner) const;
    bool cursorOverlaps(const QRect &rect) const;
    int availableWidth() const;
    int standardWidth() const;
    int expandedWidth() const;

    QLabel *m_label = nullptr;
    QGraphicsOpacityEffect *m_opacityEffect = nullptr;
    QPropertyAnimation *m_opacityAnimation = nullptr;
    QPropertyAnimation *m_geometryAnimation = nullptr;
    QTimer *m_hideTimer = nullptr;
    QTimer *m_expandTimer = nullptr;

    QString m_fullText;
    QPoint m_cursorPos;
    AnchorCorner m_corner = AnchorCorner::BottomLeft;
    int m_verticalLift = 0;
    bool m_cursorInsideContent = false;
    bool m_expanded = false;
};