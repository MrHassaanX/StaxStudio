#pragma once

#include <QString>
#include <QVariantMap>

class RecordingDiagnostics final
{
public:
    static QString logDirectory();
    static QString logPath(const QString &mediaPath, const QString &directory = {});
    static bool writeEvent(const QString &mediaPath, const QVariantMap &event,
                           const QString &directory = {}, QString *error = nullptr);
    static bool exportLatest(const QString &destination, const QString &directory = {}, QString *error = nullptr);
};
