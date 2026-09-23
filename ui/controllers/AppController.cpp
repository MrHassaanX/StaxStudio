#include "AppController.h"

AppController::AppController(QObject *parent)
    : QObject(parent)
{
}

QString AppController::activePage() const
{
    return activePage_;
}

bool AppController::gpuPreviewEnabled() const
{
    return gpuPreviewEnabled_;
}

void AppController::setActivePage(const QString &pageName)
{
    if (pageName.isEmpty() || activePage_ == pageName) {
        return;
    }

    activePage_ = pageName;
    emit activePageChanged();
}

void AppController::setGpuPreviewEnabled(const bool enabled)
{
    if (gpuPreviewEnabled_ == enabled) {
        return;
    }

    gpuPreviewEnabled_ = enabled;
    emit gpuPreviewEnabledChanged();
}

void AppController::startRecording()
{
    setActivePage(QStringLiteral("studio"));
    emit actionRequested(QStringLiteral("record"));
}

void AppController::startStreaming()
{
    setActivePage(QStringLiteral("studio"));
    emit actionRequested(QStringLiteral("stream"));
}

void AppController::startRecordAndStream()
{
    setActivePage(QStringLiteral("studio"));
    emit actionRequested(QStringLiteral("record-and-stream"));
}
