#pragma once

#include "core/capture/CaptureTypes.h"
#include "core/source/Source.h"

#include <QHash>
#include <QObject>
#include <QTimer>

class QCamera;
class QMediaCaptureSession;
class QVideoSink;

// Owns platform capture objects. Frames are timestamped here so compositing remains source-agnostic.
class VisualSourceManager final : public QObject
{
    Q_OBJECT
public:
    explicit VisualSourceManager(QObject *parent = nullptr);
    ~VisualSourceManager() override;

    void synchronizeSources(const QVector<Source> &sources);
    [[nodiscard]] CapturedVideoFrame frame(const QString &sourceId) const;
    [[nodiscard]] QVariantList targets(SourceType type) const;
    [[nodiscard]] QVariantList cameraFormats(const QString &cameraId) const;
    [[nodiscard]] QString defaultTarget(SourceType type) const;

signals:
    void framesChanged();
    void sourceStateChanged(const QString &sourceId);

private slots:
    void captureDesktopFrames();

private:
    struct CameraSession final {
        QCamera *camera = nullptr;
        QMediaCaptureSession *session = nullptr;
        QVideoSink *sink = nullptr;
        QString targetId;
    };

    [[nodiscard]] static bool isVisual(SourceType type);
    void updateFrame(const QString &sourceId, CapturedVideoFrame frame);
    void startCamera(const Source &source);
    void stopCamera(const QString &sourceId);
    void captureDesktopSource(const Source &source);

    QHash<QString, Source> sources_;
    QHash<QString, CapturedVideoFrame> frames_;
    QHash<QString, CameraSession> cameras_;
    QTimer timer_;
};
