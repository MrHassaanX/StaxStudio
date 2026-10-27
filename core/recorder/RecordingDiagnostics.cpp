#include "RecordingDiagnostics.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
bool sensitiveKey(const QString &key)
{
    QString canonical = key.toLower();
    canonical.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    return canonical.contains(QStringLiteral("token"))
        || canonical.contains(QStringLiteral("secret"))
        || canonical.contains(QStringLiteral("streamkey"))
        || canonical.contains(QStringLiteral("authorization"))
        || canonical.contains(QStringLiteral("password"))
        || canonical.contains(QStringLiteral("oauthcode"))
        || canonical.contains(QStringLiteral("authcode"))
        || canonical == QStringLiteral("key");
}

QVariant sanitize(const QVariant &value, const QString &key = {})
{
    if (value.metaType().id() == QMetaType::QVariantMap) {
        QVariantMap result;
        const QVariantMap values = value.toMap();
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            if (!sensitiveKey(it.key())) result.insert(it.key(), sanitize(it.value(), it.key()));
        return result;
    }
    if (value.metaType().id() == QMetaType::QVariantList) {
        QVariantList result;
        for (const QVariant &item : value.toList()) result.append(sanitize(item));
        return result;
    }
    if (value.metaType().id() == QMetaType::QString) {
        // Diagnostic counters and timestamps are useful without free-form strings.
        // A provider error can contain an unlabelled credential, so never export it.
        if (key == QStringLiteral("event") &&
            (value == QStringLiteral("stop") || value == QStringLiteral("audio_source")
             || value == QStringLiteral("audio_mix"))) return value;
        return QStringLiteral("<redacted>");
    }
    return value;
}

QString selectedDirectory(const QString &directory)
{
    return directory.isEmpty() ? RecordingDiagnostics::logDirectory() : directory;
}
}

QString RecordingDiagnostics::logDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("logs"));
}

QString RecordingDiagnostics::logPath(const QString &mediaPath, const QString &directory)
{
    return QDir(selectedDirectory(directory)).filePath(QFileInfo(mediaPath).fileName()
                                                  + QStringLiteral(".diagnostics.jsonl"));
}

bool RecordingDiagnostics::writeEvent(const QString &mediaPath, const QVariantMap &event,
                                      const QString &directory, QString *error)
{
    const QString folder = selectedDirectory(directory);
    if (!QDir().mkpath(folder)) {
        if (error) *error = QStringLiteral("Cannot create the diagnostics folder.");
        return false;
    }
    QFile file(logPath(mediaPath, directory));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray line = QJsonDocument::fromVariant(sanitize(event)).toJson(QJsonDocument::Compact) + '\n';
    if (file.write(line) != line.size()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool RecordingDiagnostics::exportLatest(const QString &destination, const QString &directory, QString *error)
{
    const QFileInfoList logs = QDir(selectedDirectory(directory)).entryInfoList(
        {QStringLiteral("*.diagnostics.jsonl")}, QDir::Files, QDir::Time);
    if (logs.isEmpty()) {
        if (error) *error = QStringLiteral("No detailed recording diagnostics are available yet.");
        return false;
    }
    QFile source(logs.first().absoluteFilePath());
    QSaveFile output(destination);
    if (!source.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Unable to open diagnostics for export.");
        return false;
    }
    while (!source.atEnd()) {
        const QJsonDocument event = QJsonDocument::fromJson(source.readLine());
        if (!event.isObject()) continue;
        const QByteArray line = QJsonDocument::fromVariant(sanitize(event.object().toVariantMap()))
                                    .toJson(QJsonDocument::Compact) + '\n';
        if (output.write(line) != line.size()) {
            if (error) *error = output.errorString();
            return false;
        }
    }
    if (!output.commit()) {
        if (error) *error = output.errorString();
        return false;
    }
    return true;
}
