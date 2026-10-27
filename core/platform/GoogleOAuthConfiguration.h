#pragma once

#include "CredentialStore.h"

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

inline bool validClientSecret(const QString &clientSecret)
{
    // Google Desktop OAuth secrets currently use an URL-safe token. Keep the
    // accepted format deliberately narrow so arbitrary local data is never
    // added to an OAuth request.
    static const QRegularExpression format(QStringLiteral("^[A-Za-z0-9._-]{8,512}$"));
    return format.match(clientSecret.trimmed()).hasMatch();
}

inline QString clientSecretCredentialKey(const QString &clientId)
{
    return QStringLiteral("oauth/google-client-secret/") + clientId.trimmed();
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

inline QString resolveClientSecret(const CredentialStore &credentials, const QString &clientId,
                                   const QString &applicationClientSecret,
                                   const QString &environmentOverride)
{
    const QString storedSecret = validClientId(clientId)
        ? credentials.readSecret(clientSecretCredentialKey(clientId)).trimmed()
        : QString{};
    for (const QString &candidate : {environmentOverride.trimmed(), storedSecret,
                                     applicationClientSecret.trimmed()}) {
        if (validClientSecret(candidate)) return candidate;
    }
    return {};
}
}
