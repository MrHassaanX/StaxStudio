#pragma once

#include <QObject>
#include <QString>

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activePage READ activePage WRITE setActivePage NOTIFY activePageChanged FINAL)
    Q_PROPERTY(bool gpuPreviewEnabled READ gpuPreviewEnabled WRITE setGpuPreviewEnabled NOTIFY gpuPreviewEnabledChanged FINAL)

public:
    explicit AppController(QObject *parent = nullptr);

    [[nodiscard]] QString activePage() const;
    [[nodiscard]] bool gpuPreviewEnabled() const;

public slots:
    void setActivePage(const QString &pageName);
    void setGpuPreviewEnabled(bool enabled);
    void startRecording();
    void startStreaming();
    void startRecordAndStream();

signals:
    void activePageChanged();
    void gpuPreviewEnabledChanged();
    void actionRequested(const QString &actionName);

private:
    QString activePage_ = QStringLiteral("home");
    bool gpuPreviewEnabled_ = true;
};
