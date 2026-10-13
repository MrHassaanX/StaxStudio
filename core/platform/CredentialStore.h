#pragma once

#include <QHash>
#include <QString>

class CredentialStore
{
public:
    virtual ~CredentialStore() = default;
    virtual bool writeSecret(const QString &key, const QString &value, QString *errorMessage = nullptr) = 0;
    [[nodiscard]] virtual QString readSecret(const QString &key, QString *errorMessage = nullptr) const = 0;
    virtual bool deleteSecret(const QString &key, QString *errorMessage = nullptr) = 0;
};

class InMemoryCredentialStore final : public CredentialStore
{
public:
    bool writeSecret(const QString &key, const QString &value, QString *errorMessage = nullptr) override;
    [[nodiscard]] QString readSecret(const QString &key, QString *errorMessage = nullptr) const override;
    bool deleteSecret(const QString &key, QString *errorMessage = nullptr) override;

private:
    QHash<QString, QString> secrets_;
};

class WindowsCredentialStore final : public CredentialStore
{
public:
    bool writeSecret(const QString &key, const QString &value, QString *errorMessage = nullptr) override;
    [[nodiscard]] QString readSecret(const QString &key, QString *errorMessage = nullptr) const override;
    bool deleteSecret(const QString &key, QString *errorMessage = nullptr) override;
};

