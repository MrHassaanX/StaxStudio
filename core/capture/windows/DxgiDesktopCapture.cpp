#include "DxgiDesktopCapture.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#endif

#include <QScopeGuard>

namespace {
template <typename T> void release(T *&value) { if (value) { value->Release(); value = nullptr; } }
}

struct DxgiDesktopCapture::Session final {
#ifdef Q_OS_WIN
    IDXGIOutputDuplication *duplication = nullptr;
    ID3D11Texture2D *copy = nullptr;
    DXGI_OUTPUT_DESC output{};
#endif
    QSize size;
};

DxgiDesktopCapture::~DxgiDesktopCapture() { clear(); }

DxgiDesktopCapture::Session *DxgiDesktopCapture::session(void *rawDevice, const QString &sourceId, const QString &displayName)
{
#ifdef Q_OS_WIN
    if (Session *existing = sessions_.value(sourceId)) return existing;
    auto *device = static_cast<ID3D11Device *>(rawDevice);
    if (!device) return nullptr;
    IDXGIDevice *dxgiDevice = nullptr;
    IDXGIAdapter *adapter = nullptr;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) || FAILED(dxgiDevice->GetAdapter(&adapter))) {
        release(dxgiDevice); release(adapter); return nullptr;
    }
    IDXGIOutput *output = nullptr;
    IDXGIOutput1 *output1 = nullptr;
    for (UINT index = 0; adapter->EnumOutputs(index, &output) != DXGI_ERROR_NOT_FOUND; ++index) {
        DXGI_OUTPUT_DESC description{};
        output->GetDesc(&description);
        if (QString::fromWCharArray(description.DeviceName) == displayName && SUCCEEDED(output->QueryInterface(IID_PPV_ARGS(&output1)))) break;
        release(output); release(output1);
    }
    release(adapter); release(dxgiDevice);
    if (!output1) return nullptr;
    auto *created = new Session;
    output1->GetDesc(&created->output);
    const HRESULT result = output1->DuplicateOutput(device, &created->duplication);
    release(output); release(output1);
    if (FAILED(result)) { delete created; return nullptr; }
    sessions_.insert(sourceId, created);
    return created;
#else
    Q_UNUSED(rawDevice); Q_UNUSED(sourceId); Q_UNUSED(displayName); return nullptr;
#endif
}

DxgiDesktopCapture::Frame DxgiDesktopCapture::acquire(void *rawDevice, void *rawContext, const QString &sourceId,
                                                       const QString &sourceType, const QString &targetId)
{
    Frame result;
#ifdef Q_OS_WIN
    auto *device = static_cast<ID3D11Device *>(rawDevice);
    auto *context = static_cast<ID3D11DeviceContext *>(rawContext);
    if (!device || !context) { result.message = QStringLiteral("D3D11 renderer unavailable"); return result; }
    QString displayName = targetId;
    RECT windowRect{};
    if (sourceType == QStringLiteral("Window Capture") || sourceType == QStringLiteral("Game Capture")) {
        bool ok = false;
        const HWND window = reinterpret_cast<HWND>(targetId.toULongLong(&ok));
        if (!ok || !IsWindow(window) || IsIconic(window) || !GetWindowRect(window, &windowRect)) { result.message = QStringLiteral("Window unavailable"); return result; }
        HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEXW monitorInfo{}; monitorInfo.cbSize = sizeof(monitorInfo);
        if (!GetMonitorInfoW(monitor, &monitorInfo)) { result.message = QStringLiteral("Window display unavailable"); return result; }
        displayName = QString::fromWCharArray(monitorInfo.szDevice);
    }
    Session *current = session(device, sourceId, displayName);
    if (!current) { result.message = QStringLiteral("DXGI output unavailable"); return result; }
    DXGI_OUTDUPL_FRAME_INFO info{};
    IDXGIResource *resource = nullptr;
    const HRESULT acquired = current->duplication->AcquireNextFrame(0, &info, &resource);
    if (acquired == DXGI_ERROR_WAIT_TIMEOUT && current->copy) {
        result.texture = current->copy; result.size = current->size; result.available = true;
    } else if (FAILED(acquired)) {
        if (acquired == DXGI_ERROR_ACCESS_LOST) remove(sourceId);
        result.message = QStringLiteral("Desktop frame unavailable"); return result;
    } else {
        auto releaseFrame = qScopeGuard([&] { release(resource); current->duplication->ReleaseFrame(); });
        ID3D11Texture2D *captured = nullptr;
        if (FAILED(resource->QueryInterface(IID_PPV_ARGS(&captured)))) { result.message = QStringLiteral("Desktop texture unavailable"); return result; }
        auto releaseCaptured = qScopeGuard([&] { release(captured); });
        D3D11_TEXTURE2D_DESC description{}; captured->GetDesc(&description);
        const QSize size(static_cast<int>(description.Width), static_cast<int>(description.Height));
        if (!current->copy || current->size != size) {
            release(current->copy);
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            description.CPUAccessFlags = 0;
            description.Usage = D3D11_USAGE_DEFAULT;
            description.MiscFlags = 0;
            if (FAILED(device->CreateTexture2D(&description, nullptr, &current->copy))) { result.message = QStringLiteral("GPU capture texture unavailable"); return result; }
            current->size = size;
        }
        context->CopyResource(current->copy, captured);
        result.texture = current->copy; result.size = current->size; result.available = true;
    }
    if (result.available) {
        result.sourceRect = QRect(QPoint(0, 0), result.size);
        if (sourceType == QStringLiteral("Window Capture") || sourceType == QStringLiteral("Game Capture")) {
            const RECT desktop = current->output.DesktopCoordinates;
            result.sourceRect = QRect(windowRect.left - desktop.left, windowRect.top - desktop.top,
                                      windowRect.right - windowRect.left, windowRect.bottom - windowRect.top)
                                    .intersected(QRect(QPoint(0, 0), result.size));
            if (result.sourceRect.isEmpty()) { result.available = false; result.message = QStringLiteral("Window is outside its display"); }
        }
    }
#else
    Q_UNUSED(rawDevice); Q_UNUSED(rawContext); Q_UNUSED(sourceId); Q_UNUSED(sourceType); Q_UNUSED(targetId);
    result.message = QStringLiteral("DXGI capture is only available on Windows");
#endif
    return result;
}

void DxgiDesktopCapture::remove(const QString &sourceId)
{
    Session *current = sessions_.take(sourceId);
    if (!current) return;
#ifdef Q_OS_WIN
    release(current->copy); release(current->duplication);
#endif
    delete current;
}

void DxgiDesktopCapture::clear()
{
    const auto ids = sessions_.keys();
    for (const QString &id : ids) remove(id);
}