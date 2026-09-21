#include "AppController.h"

AppController::AppController(QObject *parent)
    : QObject(parent)
{
}

QString AppController::activePage() const
{
    return activePage_;
}

void AppController::setActivePage(const QString &pageName)
{
    if (pageName.isEmpty() || activePage_ == pageName) {
        return;
    }

    activePage_ = pageName;
    emit activePageChanged();
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
