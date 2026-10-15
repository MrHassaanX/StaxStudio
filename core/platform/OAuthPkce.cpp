#include "OAuthPkce.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QUrlQuery>

namespace {
QString base64Url(const QByteArray &bytes)
{
    QByteArray encoded = bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QString::fromLatin1(encoded);
}

QString randomToken(const int byteCount)
{
    QByteArray bytes;
    bytes.resize(qMax(16, byteCount));
    QRandomGenerator *rng = QRandomGenerator::system();
    for (char &byte : bytes) byte = static_cast<char>(rng->bounded(256));
    return base64Url(bytes);
}
}

QString OAuthPkce::generateVerifier(const int byteCount)
{
    return randomToken(qMax(32, byteCount));
}

QString OAuthPkce::generateState(const int byteCount)
{
    return randomToken(qMax(16, byteCount));
}

QString OAuthPkce::challengeForVerifier(const QString &verifier)
{
    return base64Url(QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256));
}

QUrl OAuthPkce::authorizationUrl(const OAuthRequest &request)
{
    QUrl url(request.authorizationEndpoint);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("client_id"), request.clientId);
    query.addQueryItem(QStringLiteral("redirect_uri"), request.redirectUri);
    query.addQueryItem(QStringLiteral("scope"), request.scope);
    query.addQueryItem(QStringLiteral("state"), request.state);
    query.addQueryItem(QStringLiteral("code_challenge"), request.codeChallenge);
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    for (auto it = request.extraParameters.cbegin(); it != request.extraParameters.cend(); ++it)
        query.addQueryItem(it.key(), it.value().toString());
    url.setQuery(query);
    return url;
}

QString OAuthPkce::redact(QString text, const QStringList &secrets)
{
    for (const QString &secret : secrets) {
        if (!secret.isEmpty()) text.replace(secret, QStringLiteral("<redacted>"), Qt::CaseSensitive);
    }
    return text;
}

