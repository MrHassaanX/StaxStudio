#pragma once

#include <QHash>
#include <QRect>
#include <QSize>
#include <QString>

// DXGI Desktop Duplication keeps display pixels on the same D3D11 device used
// by Qt RHI. Frame textures are GPU-only and never mapped to CPU memory.
class DxgiDesktopCapture final
{
public:
    struct Frame final {
        void *texture = nullptr;
        QSize size;
        QRect sourceRect;
        bool available = false;
        QString message;
    };

    DxgiDesktopCapture() = default;
    ~DxgiDesktopCapture();
    DxgiDesktopCapture(const DxgiDesktopCapture &) = delete;
    DxgiDesktopCapture &operator=(const DxgiDesktopCapture &) = delete;

    Frame acquire(void *d3dDevice, void *d3dContext, const QString &sourceId,
                  const QString &sourceType, const QString &targetId, bool captureCursor = true);
    void remove(const QString &sourceId);
    void clear();
    quint64 calls() const { return calls_; }
    quint64 frames() const { return frames_; }
    quint64 timeouts() const { return timeouts_; }
    quint64 desktopUpdates() const { return desktopUpdates_; }
    quint64 pointerUpdates() const { return pointerUpdates_; }
    quint64 accumulatedFrames() const { return accumulatedFrames_; }
    quint64 failures() const { return failures_; }
    quint32 lastError() const { return lastError_; }
    qint64 lastTextureNs() const { return lastTextureNs_; }

private:
    struct Session;
    Session *session(void *d3dDevice, const QString &sourceId, const QString &displayName);
    QHash<QString, Session *> sessions_;
    quint64 calls_ = 0, frames_ = 0, timeouts_ = 0;
    quint64 desktopUpdates_ = 0, pointerUpdates_ = 0, accumulatedFrames_ = 0, failures_ = 0;
    quint32 lastError_ = 0;
    qint64 lastTextureNs_ = 0;
};
