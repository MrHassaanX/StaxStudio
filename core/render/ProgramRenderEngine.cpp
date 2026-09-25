#include "ProgramRenderEngine.h"

#include "core/capture/CaptureTypes.h"
#include "core/capture/windows/DxgiDesktopCapture.h"
#include "core/recorder/LocalRecorder.h"
#include "core/recorder/RecordingTypes.h"
#include "core/render/ProgramFrameMath.h"
#include "D3DProgramSurface.h"
#include <QJsonDocument>

#include <QColor>
#include <QDebug>
#include <QFile>
#include <QScopeGuard>
#include <QSet>
#include <QtMath>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>

struct ProgramRenderEngine::Item {
    QString itemId;
    QString sourceId;
    QString sourceType;
    QString targetId;
    QVariantMap transform;
    QColor color;
    QImage image;
    qint64 imageTimestampNs = 0;
    int zOrder = 0;
};

namespace {
struct Vertex { float x, y, r, g, b, a, u, v; };
template <typename T> void release(T *&value) { if (value) { value->Release(); value = nullptr; } }

bool isDesktop(const QString &type)
{
    return type == QStringLiteral("Display Capture") || type == QStringLiteral("Window Capture") || type == QStringLiteral("Game Capture");
}

QColor gradient(const QColor &base, bool horizontal, bool vertical)
{
    const qreal factor = (horizontal ? 1.12 : 0.88) * (vertical ? 1.06 : 0.94);
    return QColor::fromRgbF(qBound(0.0, base.redF() * factor, 1.0), qBound(0.0, base.greenF() * factor, 1.0), qBound(0.0, base.blueF() * factor, 1.0), base.alphaF());
}

void appendVertices(QVector<Vertex> &vertices, const QVariantMap &v, const QColor &base,
                    const QSize &program, const QSize &textureSize, const QRect &sourceRect, const bool native)
{
    const double sx = v.value("scaleX", 1.0).toDouble(), sy = v.value("scaleY", 1.0).toDouble();
    const double cropL = v.value("cropLeft").toDouble(), cropR = v.value("cropRight").toDouble();
    const double cropT = v.value("cropTop").toDouble(), cropB = v.value("cropBottom").toDouble();
    const double width = qMax(1.0, (v.value("width").toDouble() - cropL - cropR) * sx);
    const double height = qMax(1.0, (v.value("height").toDouble() - cropT - cropB) * sy);
    const double left = v.value("x").toDouble() + cropL * sx, top = v.value("y").toDouble() + cropT * sy;
    const double centerX = left + width / 2.0, centerY = top + height / 2.0;
    const double radians = qDegreesToRadians(v.value("rotation").toDouble());
    const double cs = qCos(radians), sn = qSin(radians);
    const bool flipH = v.value("flipHorizontal").toBool(), flipV = v.value("flipVertical").toBool();
    const QRect source = sourceRect.isEmpty() ? QRect(QPoint(0, 0), textureSize) : sourceRect;
    const float minU = textureSize.isEmpty() ? 0.0f : static_cast<float>(source.left()) / textureSize.width();
    const float maxU = textureSize.isEmpty() ? 1.0f : static_cast<float>(source.right() + 1) / textureSize.width();
    const float minV = textureSize.isEmpty() ? 0.0f : static_cast<float>(source.top()) / textureSize.height();
    const float maxV = textureSize.isEmpty() ? 1.0f : static_cast<float>(source.bottom() + 1) / textureSize.height();
    const auto vertex = [&](const double x, const double y, const bool right, const bool bottom) {
        const double dx = x - centerX, dy = y - centerY;
        const QColor c = gradient(base, flipH ? !right : right, flipV ? !bottom : bottom);
        const float u = flipH ? (right ? minU : maxU) : (right ? maxU : minU);
        const float normalizedV = ProgramFrameMath::textureV(bottom, native, flipV);
        return Vertex{static_cast<float>((centerX + dx * cs - dy * sn) / program.width() * 2.0 - 1.0),
                      static_cast<float>(1.0 - (centerY + dx * sn + dy * cs) / program.height() * 2.0),
                      static_cast<float>(c.redF()), static_cast<float>(c.greenF()), static_cast<float>(c.blueF()),
                      static_cast<float>(c.alphaF()), u, normalizedV == 0.0f ? minV : maxV};
    };
    const std::array<Vertex, 4> corners{vertex(left, top, false, false), vertex(left + width, top, true, false),
                                        vertex(left + width, top + height, true, true), vertex(left, top + height, false, true)};
    for (int index : {0, 1, 2, 0, 2, 3}) vertices.append(corners[index]);
}
}

ProgramRenderEngine::ProgramRenderEngine(QObject *parent) : QObject(parent)
{
    worker_ = std::jthread([this] { run(); });
    monitor_ = std::jthread([this] {
        while(running_) {
            for(int i=0;i<10 && running_;++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            QMutexLocker lock(&mutex_);
            if(!recorder_ || recorder_->state()!=RecordingState::Recording) continue;
            QVariantMap snapshot=diagnostics();
            recorder_->setProgramDiagnostics(snapshot);
            if(stage_==11) recorder_->fail(QStringLiteral("Program output failed; restart StaxStudio to reinitialize the graphics device."));
            snapshot.insert("recorder",recorder_->diagnostics());
            QFile log(recorder_->outputPath()+QStringLiteral(".diagnostics.jsonl"));
            if(log.open(QIODevice::WriteOnly|QIODevice::Append)) {
                log.write(QJsonDocument::fromVariant(snapshot).toJson(QJsonDocument::Compact)); log.write("\n");
            }
            if(recorder_->state()==RecordingState::Recording && recorder_->elapsedMs()>3000
                && mediaTimestampNs()-lastSubmittedNs_.load()>3'000'000'000LL)
                recorder_->fail(QStringLiteral("Video output stalled at stage %1. See the recording's diagnostics log.").arg(stage_.load()));
        }
    });
}

ProgramRenderEngine::~ProgramRenderEngine()
{
    running_ = false;
    if (worker_.joinable()) worker_.join();
    if (monitor_.joinable()) monitor_.join();
}

void ProgramRenderEngine::setScene(const QVariantList &layers, const QSize size)
{
    QMutexLocker lock(&mutex_);
    QHash<QString, Item> wanted;
    for (const QVariant &entry : layers) {
        const QVariantMap v = entry.toMap();
        Item item;
        item.itemId = v.value("itemId").toString();
        item.sourceId = v.value("sourceId").toString();
        item.sourceType = v.value("sourceType").toString();
        item.targetId = v.value("targetId").toString();
        item.color = v.value("color").value<QColor>();
        item.image = v.value("image").value<QImage>();
        item.imageTimestampNs = v.value("timestampNs").toLongLong();
        item.zOrder = v.value("zOrder").toInt();
        for (const char *key : {"x", "y", "width", "height", "scaleX", "scaleY", "rotation", "cropLeft", "cropTop", "cropRight", "cropBottom", "flipHorizontal", "flipVertical"})
            item.transform.insert(QString::fromLatin1(key), v.value(QString::fromLatin1(key)));
        wanted.insert(item.itemId, std::move(item));
    }
    items_ = std::move(wanted);
    if (size.isValid()) programSize_ = size;
    ++sceneRebuilds_;
}

void ProgramRenderEngine::updateSourceFrame(const QString &id, const QImage &image, const qint64 timestamp)
{
    QMutexLocker lock(&mutex_);
    for (auto it = items_.begin(); it != items_.end(); ++it)
        if (it->sourceId == id) { it->image = image; it->imageTimestampNs = timestamp; }
}

void ProgramRenderEngine::updateTransform(const QString &id, const QVariantMap &transform)
{
    QMutexLocker lock(&mutex_);
    auto it = items_.find(id);
    if (it != items_.end()) { it->transform = transform; ++transformUpdates_; }
}

void ProgramRenderEngine::setRecorder(LocalRecorder *recorder)
{
    QMutexLocker lock(&mutex_);
    recorder_ = recorder;
}

QSize ProgramRenderEngine::programSize() const { QMutexLocker lock(&mutex_); return programSize_; }
quint64 ProgramRenderEngine::renderedFrames() const { return renderedFrames_; }
quint64 ProgramRenderEngine::missedFrames() const { return missedFrames_; }
quint64 ProgramRenderEngine::sceneRebuilds() const { return sceneRebuilds_; }
quint64 ProgramRenderEngine::transformUpdates() const { return transformUpdates_; }

bool ProgramRenderEngine::copyLatestTo(void *rawDevice, void *rawContext, void *rawTexture)
{
#ifdef Q_OS_WIN
    lastPreviewRequestNs_ = mediaTimestampNs();
    if (!sharedMutex_.tryLock()) return false;
    auto unlock = qScopeGuard([&] { sharedMutex_.unlock(); });
    if (!sharedHandle_ || !rawDevice || !rawContext || !rawTexture) return false;
    auto *device = static_cast<ID3D11Device *>(rawDevice);
    ID3D11Texture2D *opened = nullptr;
    if (FAILED(device->OpenSharedResource(sharedHandle_, IID_PPV_ARGS(&opened)))) return false;
    auto openedGuard = qScopeGuard([&] { release(opened); });
    IDXGIKeyedMutex *keyed = nullptr;
    if (FAILED(opened->QueryInterface(IID_PPV_ARGS(&keyed)))) return false;
    auto keyedGuard = qScopeGuard([&] { release(keyed); });
    if (keyed->AcquireSync(1, 0) != S_OK) return false;
    static_cast<ID3D11DeviceContext *>(rawContext)->CopyResource(static_cast<ID3D11Texture2D *>(rawTexture), opened);
    static_cast<ID3D11DeviceContext *>(rawContext)->Flush();
    keyed->ReleaseSync(0);
    return true;
#else
    Q_UNUSED(rawDevice); Q_UNUSED(rawContext); Q_UNUSED(rawTexture); return false;
#endif
}

void ProgramRenderEngine::run()
{
#ifdef Q_OS_WIN
    try {
        while (running_) {
            LocalRecorder *recorder;
            { QMutexLocker lock(&mutex_); recorder = recorder_; }
            if ((recorder && recorder->state() == RecordingState::Recording) || lastPreviewRequestNs_ > 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!running_) return;
        stage_ = 1;
        D3DProgramSurface gpu;
        DxgiDesktopCapture capture;
        QSet<QString> activeCapture;
        struct ImageTexture { qint64 timestamp = -1; D3DProgramSurface::Com<ID3D11ShaderResourceView> view; };
        QHash<QString, ImageTexture> images;
        using Clock = std::chrono::steady_clock;
        // High-resolution Windows waits avoid the coarse system timer quantum
        // without busy-waiting or changing timer resolution process-wide.
        HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS);
        if(!timer) timer=CreateWaitableTimerExW(nullptr,nullptr,0,TIMER_ALL_ACCESS);
        auto closeTimer=qScopeGuard([&] { if(timer) CloseHandle(timer); });
        auto epoch = Clock::now();
        qint64 scheduleOriginNs=mediaTimestampNs();
        qint64 tick = 0;
        int previousFps = 0;
        qint64 recordingEpoch = -1;
        auto cleanup = qScopeGuard([&] { QMutexLocker lock(&sharedMutex_); sharedHandle_ = nullptr; });
        while (running_) {
            QSize size;
            QVector<Item> layers;
            LocalRecorder *recorder;
            {
                QMutexLocker lock(&mutex_);
                size=programSize_; recorder=recorder_;
                for (auto it=items_.cbegin();it!=items_.cend();++it) layers.append(it.value());
            }
            const bool recording=recorder && recorder->state()==RecordingState::Recording;
            const bool preview=mediaTimestampNs()-lastPreviewRequestNs_.load()<500'000'000LL;
            const int fps=recording ? recorder->frameRate() : 30;
            if (previousFps!=fps) { epoch=Clock::now(); scheduleOriginNs=mediaTimestampNs(); tick=0; previousFps=fps; }
            stage_=2;
            const auto deadline=epoch+std::chrono::nanoseconds(recordingFrameTimestampNs(tick,fps));
            const auto remaining=std::chrono::duration_cast<std::chrono::nanoseconds>(deadline-Clock::now()).count();
            if(timer && remaining>0) {
                LARGE_INTEGER dueTime; dueTime.QuadPart=-qMax<qint64>(1,remaining/100);
                if(SetWaitableTimer(timer,&dueTime,0,nullptr,nullptr,FALSE)) WaitForSingleObject(timer,100);
            } else if(remaining>0) std::this_thread::sleep_until(deadline);
            const qint64 elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-epoch).count();
            const qint64 due=recordingFrameIndexAt(elapsed,fps);
            if(due>tick) { missedFrames_+=due-tick; tick=due; }
            ++tick;
            ++scheduledFrames_;
            heartbeatNs_=mediaTimestampNs();
            const qint64 scheduledTimestamp=scheduleOriginNs+recordingFrameTimestampNs(tick-1,fps);
            const qint64 workStarted=mediaTimestampNs();
            if(!running_) break;
            if(!recording && !preview) continue;
            if(!size.isValid()) continue;
            if(size!=gpu.size) {
                stage_=3;
                QMutexLocker lock(&sharedMutex_);
                sharedHandle_=nullptr;
                gpu.resize(size);
                sharedHandle_=gpu.sharedHandle;
                ++generation_;
            }
            if(recording && recordingEpoch<0) {
                recordingEpoch=mediaTimestampNs();
                for(auto &slot:gpu.readbacks) slot.timestamp=-1;
            }
            if(!recording) {
                recordingEpoch=-1;
                for(auto &slot:gpu.readbacks) slot.timestamp=-1;
            }
            stage_=4;
            const qint64 readbackStarted=mediaTimestampNs();
            if(recording) gpu.drain([&](QImage image,qint64 timestamp) {
                recorder->submitVideoFrame({std::move(image),timestamp}); ++submittedFrames_; lastSubmittedNs_=timestamp;
            });
            readbackWorkNs_=mediaTimestampNs()-readbackStarted;
            qint64 captureTime=0;
            std::sort(layers.begin(),layers.end(),[](const Item &a,const Item &b){ return a.zOrder<b.zOrder; });
            QSet<QString> wanted;
            for(const auto &item:layers) if(isDesktop(item.sourceType)) wanted.insert(item.itemId);
            for(const auto &id:activeCapture) if(!wanted.contains(id)) capture.remove(id);
            activeCapture=wanted;
            for(auto it=images.begin();it!=images.end();) {
                if(std::none_of(layers.cbegin(),layers.cend(),[&](const Item &item){return item.itemId==it.key();})) it=images.erase(it); else ++it;
            }
            ++renderAttempts_;
            stage_=5;
            gpu.begin();
            for(const auto &item:layers) {
                D3DProgramSurface::Com<ID3D11ShaderResourceView> view;
                QSize textureSize;
                QRect sourceRect;
                if(!item.image.isNull()) {
                    auto &cached=images[item.itemId];
                    if(!cached.view || cached.timestamp!=item.imageTimestampNs) {
                        cached.view=gpu.upload(item.image); cached.timestamp=item.imageTimestampNs;
                    }
                    view=cached.view; textureSize=item.image.size();
                } else if(isDesktop(item.sourceType)) {
                    stage_=6;
                    const qint64 captureStarted=mediaTimestampNs();
                    const auto frame=capture.acquire(gpu.device.Get(),gpu.context.Get(),item.itemId,item.sourceType,item.targetId);
                    captureTime+=mediaTimestampNs()-captureStarted;
                    captureCalls_=capture.calls(); captureFrames_=capture.frames(); captureTimeouts_=capture.timeouts();
                    captureError_=capture.lastError(); lastCaptureNs_=capture.lastTextureNs();
                    if(frame.available) {
                        view=gpu.view(static_cast<ID3D11Texture2D *>(frame.texture)); textureSize=frame.size; sourceRect=frame.sourceRect;
                    }
                }
                stage_=7;
                QVector<Vertex> vertices;
                // Native D3D11 sampling is top-left for both uploads and DXGI textures.
                appendVertices(vertices,item.transform,view ? QColor(Qt::white) : item.color,size,textureSize,sourceRect,true);
                gpu.draw(vertices.constData(),view.Get());
            }
            ++renderedFrames_;
            lastRenderNs_=mediaTimestampNs();
            if(recording) {
                stage_=8;
                ++requestedFrames_;
                if(!gpu.enqueue(scheduledTimestamp)) ++readbackDrops_;
            }
            // Flush independently of Qt presentation; no wait for GPU completion.
            gpu.context->Flush();
            stage_=9;
            if(preview && gpu.keyed->AcquireSync(0,0)==S_OK) {
                gpu.context->CopyResource(gpu.shared.Get(),gpu.target.Get());
                gpu.context->Flush();
                D3DProgramSurface::check(gpu.keyed->ReleaseSync(1),"Release preview bridge");
            }
            D3DProgramSurface::check(gpu.device->GetDeviceRemovedReason(),"Program graphics device");
            captureWorkNs_=captureTime;
            frameWorkNs_=mediaTimestampNs()-workStarted;
            maxFrameWorkNs_=qMax(maxFrameWorkNs_.load(),frameWorkNs_.load());
            stage_=10;
        }
    } catch(const std::exception &error) {
        stage_=11;
        qCritical() << "Program output failed:" << error.what();
        QMutexLocker lock(&mutex_);
        if(recorder_) recorder_->fail(QString::fromUtf8(error.what()));
    }
#endif
}

QVariantMap ProgramRenderEngine::diagnostics() const
{
    return {{"stage",stage_.load()},{"schedulerHeartbeatNs",heartbeatNs_.load()},
        {"scheduled",scheduledFrames_.load()},{"renderAttempts",renderAttempts_.load()},
        {"produced",renderedFrames_.load()},{"missedDeadlines",missedFrames_.load()},
        {"lastRenderNs",lastRenderNs_.load()},{"textureGeneration",generation_.load()},
        {"captureCalls",captureCalls_.load()},{"captureFrames",captureFrames_.load()},
        {"captureTimeouts",captureTimeouts_.load()},{"captureHRESULT",captureError_.load()},
        {"lastCaptureNs",lastCaptureNs_.load()},{"requested",requestedFrames_.load()},
        {"submitted",submittedFrames_.load()},{"readbackDrops",readbackDrops_.load()},
        {"lastSubmittedNs",lastSubmittedNs_.load()},{"frameWorkNs",frameWorkNs_.load()},
        {"maxFrameWorkNs",maxFrameWorkNs_.load()},{"readbackWorkNs",readbackWorkNs_.load()},
        {"captureWorkNs",captureWorkNs_.load()}};
}
