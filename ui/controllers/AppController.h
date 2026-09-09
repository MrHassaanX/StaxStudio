#pragma once

#include <QObject>
#include <QString>

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activePage READ activePage WRITE setActivePage NOTIFY activePageChanged FINAL)

public:
    explicit AppController(QObject *parent = nullptr);

    [[nodiscard]] QString activePage() const;

public slots:
    void setActivePage(const QString &pageName);
    void startRecording();
    void startStreaming();
    void startRecordAndStream();

signals:
    void activePageChanged();
    void actionRequested(const QString &actionName);

private:
    QString activePage_ = QStringLiteral("home");
};
