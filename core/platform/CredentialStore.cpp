#include "CredentialStore.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#endif

bool InMemoryCredentialStore::writeSecret(const QString &key, const QString &value, QString *)
{
    secrets_.insert(key, value);
    return true;
}

QString InMemoryCredentialStore::readSecret(const QString &key, QString *) const
{
    return secrets_.value(key);
}

bool InMemoryCredentialStore::deleteSecret(const QString &key, QString *)
{
    secrets_.remove(key);
    return true;
}

bool WindowsCredentialStore::writeSecret(const QString &key, const QString &value, QString *errorMessage)
{
#ifdef Q_OS_WIN
    const QString target = QStringLiteral("StaxStudio/") + key;
    const QByteArray blob = value.toUtf8();
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(target.utf16()));
    credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(blob.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (CredWriteW(&credential, 0)) return true;
    if (errorMessage) *errorMessage = QStringLiteral("Windows credential write failed.");
    return false;
#else
    Q_UNUSED(key); Q_UNUSED(value);
    if (errorMessage) *errorMessage = QStringLiteral("Credential storage is unavailable on this platform.");
    return false;
#endif
}

QString WindowsCredentialStore::readSecret(const QString &key, QString *errorMessage) const
{
#ifdef Q_OS_WIN
    const QString target = QStringLiteral("StaxStudio/") + key;
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &credential)) {
        if (errorMessage) *errorMessage = QStringLiteral("Windows credential not found.");
        return {};
    }
    const QByteArray bytes(reinterpret_cast<const char *>(credential->CredentialBlob),
                           static_cast<int>(credential->CredentialBlobSize));
    CredFree(credential);
    return QString::fromUtf8(bytes);
#else
    Q_UNUSED(key);
    if (errorMessage) *errorMessage = QStringLiteral("Credential storage is unavailable on this platform.");
    return {};
#endif
}

bool WindowsCredentialStore::deleteSecret(const QString &key, QString *errorMessage)
{
#ifdef Q_OS_WIN
    const QString target = QStringLiteral("StaxStudio/") + key;
    if (CredDeleteW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND)
        return true;
    if (errorMessage) *errorMessage = QStringLiteral("Windows credential delete failed.");
    return false;
#else
    Q_UNUSED(key);
    if (errorMessage) *errorMessage = QStringLiteral("Credential storage is unavailable on this platform.");
    return false;
#endif
}

