#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QVariantList>

#include <atomic>
#include <thread>

class LocalRecorder;

// Owns the authoritative offscreen program target. Its clock and D3D11 device
// continue running when Qt Quick has no visible window or preview item.
class ProgramRenderEngine final : public QObject {
    Q_OBJECT
public:
    explicit ProgramRenderEngine(QObject *parent = nullptr);
    ~ProgramRenderEngine() override;
    void setScene(const QVariantList &layers, QSize programSize);
    void updateSourceFrame(const QString &sourceId, const QImage &image, qint64 timestampNs);
    void updateTransform(const QString &itemId, const QVariantMap &transform);
    void setRecorder(LocalRecorder *recorder);
    [[nodiscard]] QSize programSize() const;
    [[nodiscard]] quint64 renderedFrames() const;
    [[nodiscard]] quint64 missedFrames() const;
    [[nodiscard]] quint64 sceneRebuilds() const;
    [[nodiscard]] quint64 transformUpdates() const;
    [[nodiscard]] QVariantMap diagnostics() const;
    // Called only by the Qt Quick render thread. The shared texture is copied
    // to a texture on Qt's D3D11 device under the DXGI keyed mutex.
    bool copyLatestTo(void *qtDevice, void *qtContext, void *qtTexture);
private:
    struct Item;
    void run();
    mutable QMutex mutex_;
    QHash<QString, Item> items_;
    QSize programSize_{1920, 1080};
    LocalRecorder *recorder_ = nullptr;
    std::jthread worker_;
    std::jthread monitor_;
    std::atomic<int> stage_{0};
    std::atomic<quint64> scheduledFrames_{0}, renderAttempts_{0}, generation_{0}, requestedFrames_{0}, submittedFrames_{0}, readbackDrops_{0};
    std::atomic<quint64> captureCalls_{0}, captureFrames_{0}, captureTimeouts_{0};
    std::atomic<qint64> heartbeatNs_{0}, lastRenderNs_{0}, lastSubmittedNs_{0}, lastCaptureNs_{0};
    std::atomic<qint64> frameWorkNs_{0}, maxFrameWorkNs_{0}, readbackWorkNs_{0}, captureWorkNs_{0};
    std::atomic<quint32> captureError_{0};
    std::atomic_bool running_{true};
    std::atomic<quint64> renderedFrames_{0};
    std::atomic<quint64> missedFrames_{0};
    std::atomic<quint64> sceneRebuilds_{0};
    std::atomic<quint64> transformUpdates_{0};
    std::atomic<qint64> lastPreviewRequestNs_{0};
    void *sharedHandle_ = nullptr;
    void *sharedTexture_ = nullptr;
    QMutex sharedMutex_;
};
