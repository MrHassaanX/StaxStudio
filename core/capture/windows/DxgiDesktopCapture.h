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
                  const QString &sourceType, const QString &targetId);
    void remove(const QString &sourceId);
    void clear();

private:
    struct Session;
    Session *session(void *d3dDevice, const QString &sourceId, const QString &displayName);
    QHash<QString, Session *> sessions_;
};