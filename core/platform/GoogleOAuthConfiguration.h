#pragma once

#include <QRegularExpression>
#include <QSettings>
#include <QString>

// Desktop OAuth Client IDs are public identifiers. User tokens remain in the
// credential store; the local preferences file must never contain them.
namespace GoogleOAuthConfiguration {
inline bool validClientId(const QString &clientId)
{
    static const QRegularExpression format(
        QStringLiteral("^[0-9]+-[A-Za-z0-9_-]+\\.apps\\.googleusercontent\\.com$"));
    return format.match(clientId.trimmed()).hasMatch();
}

inline QString resolveClientId(const QString &preferencesPath, const QString &applicationClientId,
                               const QString &environmentOverride)
{
    const QSettings preferences(preferencesPath, QSettings::IniFormat);
    const QString localClientId = preferences.value(QStringLiteral("oauth/googleClientId")).toString().trimmed();
    for (const QString &candidate : {environmentOverride.trimmed(), localClientId,
                                     applicationClientId.trimmed()}) {
        if (validClientId(candidate)) return candidate;
    }
    return {};
}
}
