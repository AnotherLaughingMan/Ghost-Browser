#pragma once

#include <QObject>
#include <QJsonObject>
#include <QList>
#include <QUrl>

class ProtectionDiagnostics : public QObject
{
    Q_OBJECT

public:
    explicit ProtectionDiagnostics(QObject *parent = nullptr);

    Q_INVOKABLE QString getEventsJson() const;
    Q_INVOKABLE void clear();

    void recordEvent(const QString &action,
                     const QString &category,
                     const QUrl &requestUrl,
                     const QUrl &contextUrl,
                     const QString &detail);

signals:
    void eventsChanged();

private:
    void appendEvent(const QJsonObject &event);

    QList<QJsonObject> m_events;
};