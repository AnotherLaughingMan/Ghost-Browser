#include "core/BookmarkManager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextDocumentFragment>
#include <QUuid>

namespace {

QWidget *activeDialogParent()
{
    return QApplication::activeWindow();
}

QUrl normalizedBookmarkUrl(const QUrl &url)
{
    return url.adjusted(QUrl::NormalizePathSegments | QUrl::StripTrailingSlash | QUrl::RemoveFragment);
}

QString bookmarkKey(const QUrl &url)
{
    return normalizedBookmarkUrl(url).toString(QUrl::FullyDecoded);
}

QString decodeBookmarkTitle(const QString &html)
{
    return QTextDocumentFragment::fromHtml(html).toPlainText().trimmed();
}

QString bookmarksToHtml(const QVector<BookmarkEntry> &entries)
{
    QString html;
    html += QStringLiteral("<!DOCTYPE NETSCAPE-Bookmark-file-1>\n");
    html += QStringLiteral("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n");
    html += QStringLiteral("<TITLE>Ghost Bookmarks</TITLE>\n");
    html += QStringLiteral("<H1>Ghost Bookmarks</H1>\n");
    html += QStringLiteral("<DL><p>\n");
    for (const BookmarkEntry &entry : entries) {
        html += QStringLiteral("    <DT><A HREF=\"") + entry.url.toString().toHtmlEscaped() + QStringLiteral("\"");
        if (entry.createdAt.isValid()) {
            html += QStringLiteral(" ADD_DATE=\"")
                + QString::number(entry.createdAt.toSecsSinceEpoch())
                + QStringLiteral("\"");
        }
        html += QStringLiteral(">")
            + entry.title.toHtmlEscaped()
            + QStringLiteral("</A>\n");
    }
    html += QStringLiteral("</DL><p>\n");
    return html;
}

}

BookmarkManager::BookmarkManager(QObject *parent)
    : QObject(parent)
{
    load();
}

BookmarkManager::~BookmarkManager()
{
    if (m_dirty)
        save();
}

QJsonArray BookmarkManager::toJsonArray() const
{
    QJsonArray array;
    for (const BookmarkEntry &entry : m_entries) {
        array.append(QJsonObject {
            { QStringLiteral("id"), entry.id },
            { QStringLiteral("title"), entry.title },
            { QStringLiteral("url"), entry.url.toString() },
            { QStringLiteral("createdAt"), entry.createdAt.toString(Qt::ISODate) },
        });
    }
    return array;
}

bool BookmarkManager::containsUrl(const QUrl &url) const
{
    return indexOfUrl(url) >= 0;
}

QString BookmarkManager::bookmarkIdForUrl(const QUrl &url) const
{
    const int index = indexOfUrl(url);
    return index >= 0 ? m_entries.at(index).id : QString();
}

QString BookmarkManager::getBookmarksJson() const
{
    return QString::fromUtf8(QJsonDocument(toJsonArray()).toJson(QJsonDocument::Compact));
}

bool BookmarkManager::addBookmark(const QString &title, const QString &url)
{
    const QUrl parsedUrl = QUrl::fromUserInput(url.trimmed());
    if (!parsedUrl.isValid() || parsedUrl.scheme().isEmpty())
        return false;

    if (containsUrl(parsedUrl))
        return false;

    return upsertBookmark(title, parsedUrl, false);
}

bool BookmarkManager::updateBookmark(const QString &id, const QString &title, const QString &url)
{
    const QString trimmedId = id.trimmed();
    const QUrl parsedUrl = QUrl::fromUserInput(url.trimmed());
    const QString trimmedTitle = title.trimmed();
    if (trimmedId.isEmpty() || !parsedUrl.isValid() || parsedUrl.scheme().isEmpty())
        return false;

    const int targetIndex = indexOfId(trimmedId);
    if (targetIndex < 0)
        return false;

    const int existingUrlIndex = indexOfUrl(parsedUrl);
    if (existingUrlIndex >= 0 && existingUrlIndex != targetIndex)
        return false;

    BookmarkEntry &entry = m_entries[targetIndex];
    entry.title = trimmedTitle.isEmpty() ? parsedUrl.toString() : trimmedTitle;
    entry.url = normalizedBookmarkUrl(parsedUrl);
    m_dirty = true;
    save();
    emit bookmarksChanged();
    return true;
}

bool BookmarkManager::deleteBookmark(const QString &id)
{
    const QString trimmedId = id.trimmed();
    if (trimmedId.isEmpty())
        return false;

    for (int index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).id != trimmedId)
            continue;

        m_entries.removeAt(index);
        m_dirty = true;
        save();
        emit bookmarksChanged();
        return true;
    }

    return false;
}

QString BookmarkManager::importBookmarksFromFile()
{
    QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (startDir.isEmpty())
        startDir = QDir::homePath();

    const QString selectedFile = QFileDialog::getOpenFileName(
        activeDialogParent(),
        QStringLiteral("Import Bookmarks"),
        startDir,
        QStringLiteral("Bookmark Files (*.html *.htm *.json);;HTML Files (*.html *.htm);;JSON Files (*.json);;All Files (*.*)"));

    if (selectedFile.isEmpty())
        return {};

    QFile file(selectedFile);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(activeDialogParent(),
                             QStringLiteral("Import Failed"),
                             QStringLiteral("Ghost could not read the selected bookmark file."));
        return {};
    }

    const QByteArray payload = file.readAll();
    const QString trimmedContent = QString::fromUtf8(payload).trimmed();
    const bool looksJson = selectedFile.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)
        || trimmedContent.startsWith(QLatin1Char('{'))
        || trimmedContent.startsWith(QLatin1Char('['));

    int addedCount = 0;
    int updatedCount = 0;
    auto importBookmark = [&](const QString &title, const QString &urlText, bool overwriteTitle) {
        const QUrl url = QUrl::fromUserInput(urlText.trimmed());
        if (!url.isValid() || url.scheme().isEmpty())
            return;

        const bool existedBefore = containsUrl(url);
        bool changed = false;
        if (upsertBookmark(title, url, overwriteTitle, &changed)) {
            if (!changed)
                return;

            if (existedBefore)
                updatedCount += 1;
            else
                addedCount += 1;
        }
    };

    if (looksJson) {
        const QJsonDocument document = QJsonDocument::fromJson(payload);
        QJsonArray array;
        if (document.isArray()) {
            array = document.array();
        } else if (document.isObject()) {
            array = document.object().value(QStringLiteral("bookmarks")).toArray();
        }

        for (const QJsonValue &value : array) {
            const QJsonObject object = value.toObject();
            importBookmark(object.value(QStringLiteral("title")).toString(),
                           object.value(QStringLiteral("url")).toString(),
                           true);
        }
    } else {
        const QRegularExpression linkPattern(
            QStringLiteral("<A\\b[^>]*HREF\\s*=\\s*\"([^\"]+)\"[^>]*>(.*?)</A>"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        auto iterator = linkPattern.globalMatch(QString::fromUtf8(payload));
        while (iterator.hasNext()) {
            const QRegularExpressionMatch match = iterator.next();
            importBookmark(decodeBookmarkTitle(match.captured(2)), match.captured(1), false);
        }
    }

    if (addedCount == 0 && updatedCount == 0) {
        QMessageBox::warning(activeDialogParent(),
                             QStringLiteral("Import Failed"),
                             QStringLiteral("Ghost did not find any bookmark entries it could import from that file."));
        return {};
    }

    save();
    emit bookmarksChanged();
    QMessageBox::information(activeDialogParent(),
                             QStringLiteral("Bookmarks Imported"),
                             QStringLiteral("Ghost imported %1 bookmark(s) and updated %2 existing bookmark(s).").arg(addedCount).arg(updatedCount));
    return selectedFile;
}

QString BookmarkManager::exportBookmarksToFile() const
{
    if (m_entries.isEmpty()) {
        QMessageBox::information(activeDialogParent(),
                                 QStringLiteral("No Bookmarks"),
                                 QStringLiteral("There are no bookmarks to export yet."));
        return {};
    }

    QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (startDir.isEmpty())
        startDir = QDir::homePath();

    QString selectedFilter = QStringLiteral("HTML Files (*.html)");
    const QString selectedFile = QFileDialog::getSaveFileName(
        activeDialogParent(),
        QStringLiteral("Export Bookmarks"),
        QDir(startDir).filePath(QStringLiteral("Ghost Bookmarks.html")),
        QStringLiteral("HTML Files (*.html);;JSON Files (*.json)"),
        &selectedFilter);

    if (selectedFile.isEmpty())
        return {};

    QString outputPath = selectedFile;
    const bool exportJson = selectedFilter.contains(QStringLiteral("*.json"), Qt::CaseInsensitive)
        || outputPath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive);
    if (!exportJson && !outputPath.endsWith(QStringLiteral(".html"), Qt::CaseInsensitive)
        && !outputPath.endsWith(QStringLiteral(".htm"), Qt::CaseInsensitive)) {
        outputPath += QStringLiteral(".html");
    }
    if (exportJson && !outputPath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        outputPath += QStringLiteral(".json");

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(activeDialogParent(),
                             QStringLiteral("Export Failed"),
                             QStringLiteral("Ghost could not write the bookmark export file."));
        return {};
    }

    const QByteArray content = exportJson
        ? QJsonDocument(toJsonArray()).toJson(QJsonDocument::Indented)
        : bookmarksToHtml(m_entries).toUtf8();
    file.write(content);
    return outputPath;
}

int BookmarkManager::indexOfId(const QString &id) const
{
    const QString trimmedId = id.trimmed();
    for (int index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).id == trimmedId)
            return index;
    }
    return -1;
}

int BookmarkManager::indexOfUrl(const QUrl &url) const
{
    const QString key = bookmarkKey(url);
    if (key.isEmpty())
        return -1;

    for (int index = 0; index < m_entries.size(); ++index) {
        if (bookmarkKey(m_entries.at(index).url) == key)
            return index;
    }
    return -1;
}

bool BookmarkManager::upsertBookmark(const QString &title, const QUrl &url, bool overwriteTitle, bool *changed)
{
    const QUrl normalizedUrl = normalizedBookmarkUrl(url);
    const QString resolvedTitle = title.trimmed().isEmpty() ? normalizedUrl.toString() : title.trimmed();
    const int existingIndex = indexOfUrl(normalizedUrl);
    if (existingIndex >= 0) {
        BookmarkEntry &entry = m_entries[existingIndex];
        const QString nextTitle = overwriteTitle || entry.title.trimmed().isEmpty() ? resolvedTitle : entry.title;
        const bool titleChanged = entry.title != nextTitle;
        const bool urlChanged = entry.url != normalizedUrl;
        if (titleChanged)
            entry.title = nextTitle;
        if (urlChanged)
            entry.url = normalizedUrl;
        if (changed)
            *changed = titleChanged || urlChanged;
        if (titleChanged || urlChanged)
            m_dirty = true;
        return true;
    }

    BookmarkEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.title = resolvedTitle;
    entry.url = normalizedUrl;
    entry.createdAt = QDateTime::currentDateTimeUtc();
    m_entries.append(entry);
    m_dirty = true;
    if (changed)
        *changed = true;
    return true;
}

void BookmarkManager::load()
{
    QFile file(storagePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray())
        return;

    m_entries.clear();
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        const QUrl url = QUrl::fromUserInput(object.value(QStringLiteral("url")).toString());
        if (!url.isValid())
            continue;

        BookmarkEntry entry;
        entry.id = object.value(QStringLiteral("id")).toString();
        entry.title = object.value(QStringLiteral("title")).toString();
        entry.url = normalizedBookmarkUrl(url);
        entry.createdAt = QDateTime::fromString(object.value(QStringLiteral("createdAt")).toString(), Qt::ISODate);
        if (entry.id.isEmpty())
            entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (entry.title.isEmpty())
            entry.title = entry.url.toString();
        m_entries.append(entry);
    }
}

void BookmarkManager::save() const
{
    const QString path = storagePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(QJsonDocument(toJsonArray()).toJson(QJsonDocument::Compact));
    m_dirty = false;
}

QString BookmarkManager::storagePath() const
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(configDir).filePath(QStringLiteral("bookmarks.json"));
}