#include "core/ProtectionDiagnostics.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QThread>

namespace {

constexpr int kMaxProtectionEvents = 200;

}

ProtectionDiagnostics::ProtectionDiagnostics(QObject *parent)
    : QObject(parent)
{
}

QString ProtectionDiagnostics::getEventsJson() const
{
    QJsonArray array;
    for (const QJsonObject &event : m_events)
        array.append(event);

    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void ProtectionDiagnostics::clear()
{
    if (m_events.isEmpty())
        return;

    m_events.clear();
    emit eventsChanged();
}

void ProtectionDiagnostics::recordEvent(const QString &action,
                                        const QString &category,
                                        const QUrl &requestUrl,
                                        const QUrl &contextUrl,
                                        const QString &detail)
{
    QJsonObject event {
        { QStringLiteral("action"), action },
        { QStringLiteral("category"), category },
        { QStringLiteral("url"), requestUrl.toString() },
        { QStringLiteral("host"), requestUrl.host() },
        { QStringLiteral("page"), contextUrl.toString() },
        { QStringLiteral("detail"), detail },
        { QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
    };

    if (thread() == QThread::currentThread()) {
        appendEvent(event);
        return;
    }

    QMetaObject::invokeMethod(this, [this, event]() {
        appendEvent(event);
    }, Qt::QueuedConnection);
}

void ProtectionDiagnostics::appendEvent(const QJsonObject &event)
{
    m_events.prepend(event);
    while (m_events.size() > kMaxProtectionEvents)
        m_events.removeLast();

    emit eventsChanged();
}